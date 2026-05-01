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
    double setting_number(Store& store, const std::string& key, double fallback)
    {
        try
        {
            return std::stod(store.get_setting(key, std::to_string(fallback)));
        }
        catch (...)
        {
            return fallback;
        }
    }

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

    double disk_percent(const SystemMetrics& metrics)
    {
        std::vector<double> values;
        values.reserve(metrics.disks.size());
        for (const auto& disk : metrics.disks)
            values.push_back(disk.usage_percent);
        return max_percent(values);
    }

    double temp_percent(const SystemMetrics& metrics)
    {
        std::vector<double> values;
        values.reserve(metrics.temperatures.size());
        for (const auto& temp : metrics.temperatures)
            values.push_back(temp.temperature_celsius);
        return max_percent(values);
    }

    double process_percent(const SystemMetrics& metrics)
    {
        std::vector<double> values;
        values.reserve(metrics.top_processes.size());
        for (const auto& process : metrics.top_processes)
            values.push_back(process.cpu_percent);
        return max_percent(values);
    }

    double network_score(const SystemMetrics& metrics)
    {
        double score = 0.0;
        for (const auto& net : metrics.networks)
        {
            score += static_cast<double>(net.errors_in + net.errors_out + net.drops_in + net.drops_out);
            if (!net.is_up)
                score += 1.0;
        }
        return score;
    }

    double value_for_indicator(const SystemMetrics& metrics, const std::string& indicator)
    {
        if (indicator == "cpu")
            return metrics.cpu.usage_percent;
        if (indicator == "memory")
            return metrics.memory.usage_percent;
        if (indicator == "disk")
            return disk_percent(metrics);
        if (indicator == "temperature")
            return temp_percent(metrics);
        if (indicator == "processes")
            return process_percent(metrics);
        if (indicator == "network")
            return network_score(metrics);
        return 0.0;
    }

    std::string transition_message(const std::string& indicator, IndicatorStatus previous, IndicatorStatus next,
                                   double value, double average)
    {
        return indicator + " changed from " + status_to_string(previous) + " to " + status_to_string(next) +
               " value=" + std::to_string(value) + " five_minute_average=" + std::to_string(average);
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
    auto agent = store_.get_agent(agent_id);
    if (agent && agent->maintenance)
    {
        LOGF_DEBUG("Skipping status evaluation during maintenance agent_id=%s", agent_id.c_str());
        return;
    }

    const auto rows = store_.get_metrics(agent_id, 256);
    const auto cutoff = timestamp_ms - 300000;
    const std::vector<std::string> indicators = {"cpu", "memory", "disk", "network", "temperature", "processes"};

    for (const auto& indicator : indicators)
    {
        double sum = 0.0;
        int count = 0;
        for (const auto& row : rows)
        {
            if (row.timestamp_ms < cutoff)
                continue;
            try
            {
                auto parsed = json::parse(row.metrics_json).get<SystemMetrics>();
                sum += value_for_indicator(parsed, indicator);
                ++count;
            }
            catch (const std::exception& e)
            {
                LOGF_WARNING("Skipping metric row during status evaluation agent_id=%s indicator=%s error=%s",
                             agent_id.c_str(), indicator.c_str(), e.what());
            }
        }

        const auto current_value = value_for_indicator(metrics, indicator);
        const auto average = count > 0 ? sum / static_cast<double>(count) : current_value;
        const auto next = classify_percent(agent ? &*agent : nullptr, indicator, current_value, average);
        const auto previous_row = store_.latest_status_for_indicator(agent_id, indicator);
        const auto previous = previous_row ? status_from_string(previous_row->new_status) : IndicatorStatus::Grey;
        record_transition(agent_id, indicator, next,
                          transition_message(indicator, previous, next, current_value, average), timestamp_ms);
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

IndicatorStatus StatusEngine::classify_percent(const AgentRecord* agent, const std::string& indicator, double value,
                                               double average)
{
    if (!std::isfinite(value))
        return IndicatorStatus::Grey;

    auto threshold_value = [&](const std::string& level, double fallback) {
        if (agent)
        {
            if (indicator == "cpu")
            {
                if (level == "warning")
                    return agent->cpu_warning_pct_of_avg;
                if (level == "degraded")
                    return agent->cpu_degraded_pct_of_avg;
                return agent->cpu_critical_pct_of_avg;
            }
            if (indicator == "memory")
            {
                if (level == "warning")
                    return agent->memory_warning_pct_of_avg;
                if (level == "degraded")
                    return agent->memory_degraded_pct_of_avg;
                return agent->memory_critical_pct_of_avg;
            }
            if (indicator == "disk")
            {
                if (level == "warning")
                    return agent->disk_warning_pct_of_avg;
                if (level == "degraded")
                    return agent->disk_degraded_pct_of_avg;
                return agent->disk_critical_pct_of_avg;
            }
            if (indicator == "network")
            {
                if (level == "warning")
                    return agent->network_warning_pct_of_avg;
                if (level == "degraded")
                    return agent->network_degraded_pct_of_avg;
                return agent->network_critical_pct_of_avg;
            }
        }
        return setting_number(store_, "threshold." + indicator + "." + level + "_pct_of_avg", fallback);
    };

    const auto warning_pct = threshold_value("warning", 125.0);
    const auto amber_pct = threshold_value("degraded", 150.0);
    const auto red_pct = threshold_value("critical", 200.0);
    const auto base = average > 0.0 ? average : value;
    const auto pct_of_avg = base > 0.0 ? (value / base) * 100.0 : 0.0;

    if (pct_of_avg >= red_pct || value >= 95.0)
        return IndicatorStatus::Red;
    if (pct_of_avg >= amber_pct || value >= 85.0)
        return IndicatorStatus::Amber;
    if (pct_of_avg >= warning_pct || value >= 70.0)
        return IndicatorStatus::Yellow;
    return IndicatorStatus::Green;
}

void StatusEngine::record_transition(const std::string& agent_id, const std::string& indicator, IndicatorStatus next,
                                     const std::string& message, int64_t timestamp_ms)
{
    auto previous_row = store_.latest_status_for_indicator(agent_id, indicator);
    const auto previous = previous_row ? status_from_string(previous_row->new_status) : IndicatorStatus::Grey;
    if (previous_row && previous == next)
        return;

    const auto final_message = previous_row ? message : indicator + " initial status " + status_to_string(next);
    store_.insert_status_history(
        {0, agent_id, indicator, status_to_string(previous), status_to_string(next), final_message, timestamp_ms});

    if (previous_row && is_worse_status(previous, next))
    {
        AlertRecord alert;
        alert.agent_id = agent_id;
        alert.indicator = indicator;
        alert.old_status = status_to_string(previous);
        alert.new_status = status_to_string(next);
        alert.message = final_message;
        alert.created_at = timestamp_ms;
        const auto alert_id = store_.insert_alert(alert);
        LOGF_WARNING("Generated alert id=%lld agent_id=%s indicator=%s old=%s new=%s", static_cast<long long>(alert_id),
                     agent_id.c_str(), indicator.c_str(), alert.old_status.c_str(), alert.new_status.c_str());
        send_webhook(store_, alert, alert_id);
    }
}

} // namespace thewatcher::server
