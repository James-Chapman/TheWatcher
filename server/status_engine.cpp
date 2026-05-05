#include "status_engine.hpp"

#include "common/SingleLog.hpp"

#include <algorithm>
#include <cmath>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace thewatcher::server
{

using json = nlohmann::json;

namespace
{
    double max_percent(const std::vector<double>& values)
    {
        double result = 0.0;
        for (auto value : values)
        {
            if (std::isfinite(value))
                result = std::max(result, value);
        }
        return result;
    }

    double temp_percent(const SystemMetrics& metrics)
    {
        std::vector<double> values;
        values.reserve(metrics.temperatures.size());
        for (const auto& temp : metrics.temperatures)
            values.push_back(temp.temperature_celsius);
        return max_percent(values);
    }

    double network_mbps(const NetworkMetrics& network)
    {
        return static_cast<double>(network.bytes_recv_per_sec + network.bytes_sent_per_sec) * 8.0 / 1'000'000.0;
    }

    std::string disk_label(const DiskMetrics& disk)
    {
        return disk.mount_point + " (" + disk.device + ")";
    }

    std::string transition_message(const std::string& indicator, IndicatorStatus previous, IndicatorStatus next,
                                   double value, const std::string& unit)
    {
        return indicator + " changed from " + status_to_string(previous) + " to " + status_to_string(next) +
               " value=" + std::to_string(value) + unit;
    }

    const DiskMonitorConfig* disk_config_for(const CollectorConfig& config, const DiskMetrics& disk)
    {
        if (config.disks.empty())
            return nullptr;
        for (const auto& item : config.disks)
        {
            if (item.mount_point == disk.mount_point)
                return &item;
        }
        return nullptr;
    }

    const NetworkInterfaceConfig* network_config_for(const CollectorConfig& config, const NetworkMetrics& network)
    {
        if (config.networks.empty())
            return nullptr;
        for (const auto& item : config.networks)
        {
            if (item.interface_name == network.interface_name)
                return &item;
        }
        return nullptr;
    }

    bool should_monitor_default_network(const NetworkMetrics& network)
    {
        return network.interface_name != "lo";
    }

    struct WebhookTarget
    {
        std::string host;
        int port = 80;
        std::string path = "/";
    };

    std::optional<WebhookTarget> parse_http_webhook(const std::string& url)
    {
        constexpr std::string_view prefix = "http://";
        if (url.rfind(prefix, 0) != 0)
            return std::nullopt;

        WebhookTarget target;
        auto rest = url.substr(prefix.size());
        auto slash = rest.find('/');
        auto host_port = slash == std::string::npos ? rest : rest.substr(0, slash);
        target.path = slash == std::string::npos ? "/" : rest.substr(slash);
        auto colon = host_port.rfind(':');
        if (colon != std::string::npos)
        {
            target.host = host_port.substr(0, colon);
            target.port = std::stoi(host_port.substr(colon + 1));
        }
        else
        {
            target.host = host_port;
        }
        if (target.host.empty())
            return std::nullopt;
        return target;
    }

    void send_webhook(Store& store, const AlertRecord& alert, int64_t alert_id)
    {
        const auto webhook_url = store.get_setting("notifications.webhook_url", "");
        if (webhook_url.empty())
            return;

        const auto target = parse_http_webhook(webhook_url);
        if (!target)
        {
            LOGF_WARNING("Skipping unsupported webhook URL: %s", webhook_url.c_str());
            return;
        }

        json payload = {
            {"alert_id",   alert_id        },
            {"agent_id",   alert.agent_id  },
            {"indicator",  alert.indicator },
            {"old_status", alert.old_status},
            {"new_status", alert.new_status},
            {"message",    alert.message   },
            {"created_at", alert.created_at},
        };

        httplib::Client client(target->host, target->port);
        client.set_connection_timeout(2, 0);
        client.set_read_timeout(2, 0);
        auto response = client.Post(target->path, payload.dump(), "application/json");
        if (!response)
        {
            LOGF_WARNING("Webhook notification failed alert_id=%lld url=%s", static_cast<long long>(alert_id),
                         webhook_url.c_str());
            return;
        }
        LOGF_DEBUG("Webhook notification sent alert_id=%lld status=%d", static_cast<long long>(alert_id),
                   response->status);
    }
} // namespace

std::string status_to_string(IndicatorStatus status)
{
    switch (status)
    {
    case IndicatorStatus::Green:
        return "green";
    case IndicatorStatus::Grey:
        return "grey";
    case IndicatorStatus::Yellow:
        return "yellow";
    case IndicatorStatus::Amber:
        return "amber";
    case IndicatorStatus::Red:
        return "red";
    case IndicatorStatus::Blue:
        return "blue";
    }
    return "grey";
}

IndicatorStatus status_from_string(const std::string& value)
{
    if (value == "green")
        return IndicatorStatus::Green;
    if (value == "yellow")
        return IndicatorStatus::Yellow;
    if (value == "amber" || value == "orange")
        return IndicatorStatus::Amber;
    if (value == "red")
        return IndicatorStatus::Red;
    if (value == "blue")
        return IndicatorStatus::Blue;
    return IndicatorStatus::Grey;
}

bool is_worse_status(IndicatorStatus previous, IndicatorStatus current)
{
    if (current == IndicatorStatus::Blue || previous == IndicatorStatus::Blue)
        return false;
    return static_cast<int>(current) > static_cast<int>(previous);
}

StatusEngine::StatusEngine(Store& store) : store_(store)
{
}

void StatusEngine::evaluate_metrics(const std::string& agent_id, const SystemMetrics& metrics, int64_t timestamp_ms)
{
    constexpr int64_t prune_interval_ms = 3'600'000; // prune once per hour
    if (timestamp_ms - last_prune_ms_ >= prune_interval_ms)
    {
        last_prune_ms_ = timestamp_ms;
        const auto retention_days_str = store_.get_setting("metrics_retention_days", "30");
        const int retention_days = std::max(1, std::stoi(retention_days_str));
        const int64_t cutoff_ms = timestamp_ms - static_cast<int64_t>(retention_days) * 86'400'000LL;
        store_.prune_metrics_before(cutoff_ms);
        LOGF_DEBUG("Pruned metrics older than %d days", retention_days);
    }

    auto agent = store_.get_agent(agent_id);
    if (agent && agent->maintenance)
    {
        LOGF_DEBUG("Skipping status evaluation during maintenance agent_id=%s", agent_id.c_str());
        return;
    }

    const auto config = agent ? agent->collector_config : default_collector_config();

    {
        const auto indicator = std::string("cpu");
        const auto raw = classify_percent(metrics.cpu.usage_percent, config.cpu);
        const auto previous_row = store_.latest_status_for_indicator(agent_id, indicator);
        const auto previous = previous_row ? status_from_string(previous_row->new_status) : IndicatorStatus::Grey;
        const auto next = confirm_numeric_status(agent_id, indicator, previous, raw, config.cpu_readings);
        record_transition(agent_id, indicator, next,
                          transition_message(indicator, previous, next, metrics.cpu.usage_percent, "%"), timestamp_ms);
    }

    {
        const auto indicator = std::string("memory");
        const auto raw = classify_percent(metrics.memory.usage_percent, config.memory);
        const auto previous_row = store_.latest_status_for_indicator(agent_id, indicator);
        const auto previous = previous_row ? status_from_string(previous_row->new_status) : IndicatorStatus::Grey;
        const auto next = confirm_numeric_status(agent_id, indicator, previous, raw, config.memory_readings);
        record_transition(agent_id, indicator, next,
                          transition_message(indicator, previous, next, metrics.memory.usage_percent, "%"),
                          timestamp_ms);
    }

    for (const auto& disk : metrics.disks)
    {
        const auto* disk_config = disk_config_for(config, disk);
        if (disk_config && !disk_config->enabled)
            continue;
        if (!config.disks.empty() && !disk_config)
            continue;

        const auto thresholds = disk_config ? disk_config->thresholds : PercentThresholds{};
        const auto indicator = "disk:" + disk.mount_point;
        const auto raw = classify_percent(disk.usage_percent, thresholds);
        const auto previous_row = store_.latest_status_for_indicator(agent_id, indicator);
        const auto previous = previous_row ? status_from_string(previous_row->new_status) : IndicatorStatus::Grey;
        const auto next = confirm_numeric_status(agent_id, indicator, previous, raw, config.disk_readings);
        record_transition(agent_id, indicator, next,
                          transition_message("disk " + disk_label(disk), previous, next, disk.usage_percent, "%"),
                          timestamp_ms);
    }

    for (const auto& network : metrics.networks)
    {
        const auto* network_config = network_config_for(config, network);
        if (network_config && !network_config->enabled)
            continue;
        if (!config.networks.empty() && !network_config)
            continue;
        if (config.networks.empty() && !should_monitor_default_network(network))
            continue;

        const auto thresholds = network_config ? network_config->thresholds : NetworkThresholds{};
        const auto value = network_mbps(network);
        auto raw = classify_network_mbps(value, thresholds);
        if (!network.is_up)
            raw = IndicatorStatus::Red;

        const auto indicator = "network:" + network.interface_name;
        const auto previous_row = store_.latest_status_for_indicator(agent_id, indicator);
        const auto previous = previous_row ? status_from_string(previous_row->new_status) : IndicatorStatus::Grey;
        const auto next = confirm_numeric_status(agent_id, indicator, previous, raw, config.network_readings);
        record_transition(agent_id, indicator, next,
                          transition_message("network " + network.interface_name, previous, next, value, "Mbps"),
                          timestamp_ms);
    }

    for (const auto& watch : config.processes)
    {
        if (!watch.enabled || watch.name.empty())
            continue;

        int seen = 0;
        for (const auto& process : metrics.top_processes)
        {
            if (process.name == watch.name)
                ++seen;
        }

        const auto indicator = "process:" + watch.name;
        const auto previous_row = store_.latest_status_for_indicator(agent_id, indicator);
        const auto previous = previous_row ? status_from_string(previous_row->new_status) : IndicatorStatus::Grey;
        IndicatorStatus next = IndicatorStatus::Green;
        if (seen < watch.expected_count)
        {
            const auto pending = store_.get_pending_status(agent_id, indicator);
            const auto missing_count = pending && pending->target_status == "missing" ? pending->count + 1 : 1;
            store_.set_pending_status(agent_id, indicator, "missing", missing_count);
            next = process_status_for_count(missing_count, config.process_readings);
        }
        else
        {
            store_.clear_pending_status(agent_id, indicator);
        }

        const auto message = "process " + watch.name + " expected=" + std::to_string(watch.expected_count) +
                             " found=" + std::to_string(seen);
        record_transition(agent_id, indicator, next, message, timestamp_ms);
    }

    {
        const auto indicator = std::string("temperature");
        const auto value = temp_percent(metrics);
        const auto next = classify_percent(value, PercentThresholds{});
        const auto previous_row = store_.latest_status_for_indicator(agent_id, indicator);
        const auto previous = previous_row ? status_from_string(previous_row->new_status) : IndicatorStatus::Grey;
        record_transition(agent_id, indicator, next, transition_message(indicator, previous, next, value, "C"),
                          timestamp_ms);
    }
}

void StatusEngine::enter_maintenance(const std::string& agent_id, const std::string& reason, int64_t until_ms,
                                     int64_t now_ms)
{
    store_.set_agent_maintenance(agent_id, true, reason, until_ms);
    store_.clear_active_alerts_for_agent(agent_id, now_ms);
    for (const auto& indicator : {"cpu", "memory", "disk", "network", "temperature", "processes"})
        record_transition(agent_id, indicator, IndicatorStatus::Blue, "maintenance mode", now_ms);
}

void StatusEngine::exit_maintenance(const std::string& agent_id)
{
    store_.set_agent_maintenance(agent_id, false, "", 0);
}

IndicatorStatus StatusEngine::classify_percent(double value, const PercentThresholds& thresholds) const
{
    if (!std::isfinite(value))
        return IndicatorStatus::Grey;

    if (value >= thresholds.critical_percent)
        return IndicatorStatus::Red;
    if (value >= thresholds.degraded_percent)
        return IndicatorStatus::Amber;
    if (value >= thresholds.warning_percent)
        return IndicatorStatus::Yellow;
    return IndicatorStatus::Green;
}

IndicatorStatus StatusEngine::classify_network_mbps(double value, const NetworkThresholds& thresholds) const
{
    if (!std::isfinite(value))
        return IndicatorStatus::Grey;
    if (value >= thresholds.critical_mbps)
        return IndicatorStatus::Red;
    if (value >= thresholds.degraded_mbps)
        return IndicatorStatus::Amber;
    if (value >= thresholds.warning_mbps)
        return IndicatorStatus::Yellow;
    return IndicatorStatus::Green;
}

IndicatorStatus StatusEngine::confirm_numeric_status(const std::string& agent_id, const std::string& indicator,
                                                     IndicatorStatus previous, IndicatorStatus raw,
                                                     int required_readings)
{
    const auto required = std::max(1, required_readings);
    if (raw == IndicatorStatus::Green || raw == IndicatorStatus::Grey || !is_worse_status(previous, raw))
    {
        store_.clear_pending_status(agent_id, indicator);
        return raw;
    }

    const auto target = status_to_string(raw);
    const auto pending = store_.get_pending_status(agent_id, indicator);
    const auto count = pending && pending->target_status == target ? pending->count + 1 : 1;
    if (count < required)
    {
        store_.set_pending_status(agent_id, indicator, target, count);
        LOGF_DEBUG("Pending status confirmation agent_id=%s indicator=%s target=%s count=%d required=%d",
                   agent_id.c_str(), indicator.c_str(), target.c_str(), count, required);
        return previous == IndicatorStatus::Grey ? IndicatorStatus::Green : previous;
    }

    store_.clear_pending_status(agent_id, indicator);
    return raw;
}

IndicatorStatus StatusEngine::process_status_for_count(int missing_count, int readings_to_red) const
{
    const auto red_after = std::max(1, readings_to_red);
    if (missing_count >= red_after)
        return IndicatorStatus::Red;
    if (missing_count >= std::max(2, ((red_after * 2) + 2) / 3))
        return IndicatorStatus::Amber;
    return IndicatorStatus::Yellow;
}

void StatusEngine::record_transition(const std::string& agent_id, const std::string& indicator, IndicatorStatus next,
                                     const std::string& message, int64_t timestamp_ms)
{
    auto previous_row = store_.latest_status_for_indicator(agent_id, indicator);
    const auto previous = previous_row ? status_from_string(previous_row->new_status) : IndicatorStatus::Grey;
    if (previous_row && previous == next)
        return;

    const auto final_message = message.empty() ? indicator + " initial status " + status_to_string(next) : message;
    store_.insert_status_history(
        {0, agent_id, indicator, status_to_string(previous), status_to_string(next), final_message, timestamp_ms});

    if (previous_row && is_worse_status(previous, next))
    {
        if (store_.is_silenced(agent_id, indicator, timestamp_ms))
        {
            LOGF_DEBUG("Alert suppressed by silence rule agent_id=%s indicator=%s", agent_id.c_str(),
                       indicator.c_str());
        }
        else
        {
            AlertRecord alert;
            alert.agent_id = agent_id;
            alert.indicator = indicator;
            alert.old_status = status_to_string(previous);
            alert.new_status = status_to_string(next);
            alert.message = final_message;
            alert.created_at = timestamp_ms;
            const auto alert_id = store_.insert_alert(alert);
            LOGF_WARNING("Generated alert id=%lld agent_id=%s indicator=%s old=%s new=%s",
                         static_cast<long long>(alert_id), agent_id.c_str(), indicator.c_str(),
                         alert.old_status.c_str(), alert.new_status.c_str());
            send_webhook(store_, alert, alert_id);
        }
    }
}

} // namespace thewatcher::server
