#include "report_generator.hpp"

#include "common/SingleLog.hpp"
#include "status_engine.hpp"

#include <algorithm>
#include <httplib.h>
#include <string_view>

namespace thewatcher::server
{

using json = nlohmann::json;

namespace
{
    // Reuses the same private-host guard as status_engine.cpp.
    bool is_report_private_host(const std::string& host)
    {
        if (host == "localhost" || host == "::1" || host == "[::1]")
            return true;
        auto starts = [&](const char* p) { return host.rfind(p, 0) == 0; };
        if (starts("127.") || starts("10.") || starts("192.168.") || starts("169.254.") || starts("0."))
            return true;
        if (starts("172."))
        {
            const auto dot2 = host.find('.', 4);
            if (dot2 != std::string::npos)
            {
                const int octet = std::stoi(host.substr(4, dot2 - 4));
                if (octet >= 16 && octet <= 31)
                    return true;
            }
        }
        return false;
    }

    struct WebhookTarget
    {
        std::string host;
        int port = 80;
        std::string path = "/";
    };

    std::optional<WebhookTarget> parse_webhook(const std::string& url)
    {
        constexpr std::string_view prefix = "http://";
        if (url.rfind(prefix, 0) != 0)
            return std::nullopt;
        auto rest = url.substr(prefix.size());
        auto slash = rest.find('/');
        auto host_port = slash == std::string::npos ? rest : rest.substr(0, slash);
        WebhookTarget t;
        t.path = slash == std::string::npos ? "/" : rest.substr(slash);
        auto colon = host_port.rfind(':');
        if (colon != std::string::npos)
        {
            t.host = host_port.substr(0, colon);
            t.port = std::stoi(host_port.substr(colon + 1));
        }
        else
        {
            t.host = host_port;
        }
        if (t.host.empty() || is_report_private_host(t.host))
            return std::nullopt;
        return t;
    }

} // namespace

json build_report_json(Store& store, int64_t now_ms)
{
    const auto agents = store.list_approved_agents();
    const auto alerts = store.list_unacknowledged_alerts();

    int total = static_cast<int>(agents.size());
    int offline = 0;
    int maintenance = 0;
    for (const auto& a : agents)
    {
        if (a.maintenance)
            ++maintenance;
        else if (!a.connected)
            ++offline;
    }

    // Alert counts by severity
    int yellow_count = 0, amber_count = 0, red_count = 0;
    for (const auto& al : alerts)
    {
        const auto s = status_from_string(al.new_status);
        if (s == IndicatorStatus::Red)
            ++red_count;
        else if (s == IndicatorStatus::Amber)
            ++amber_count;
        else if (s == IndicatorStatus::Yellow)
            ++yellow_count;
    }

    // Top-5 agents by oldest unacknowledged alert
    struct AgentAge
    {
        std::string agent_id;
        int64_t oldest_alert_ms = 0;
        int alert_count = 0;
    };
    std::unordered_map<std::string, AgentAge> agent_age_map;
    for (const auto& al : alerts)
    {
        auto& aa = agent_age_map[al.agent_id];
        aa.agent_id = al.agent_id;
        ++aa.alert_count;
        if (aa.oldest_alert_ms == 0 || al.created_at < aa.oldest_alert_ms)
            aa.oldest_alert_ms = al.created_at;
    }
    std::vector<AgentAge> sorted_agents;
    sorted_agents.reserve(agent_age_map.size());
    for (auto& [id, aa] : agent_age_map)
        sorted_agents.push_back(aa);
    std::sort(sorted_agents.begin(), sorted_agents.end(),
              [](const AgentAge& a, const AgentAge& b) { return a.oldest_alert_ms < b.oldest_alert_ms; });
    if (sorted_agents.size() > 5)
        sorted_agents.resize(5);

    // Build agent lookup for names
    std::unordered_map<std::string, std::string> agent_names;
    for (const auto& a : agents)
        agent_names[a.agent_id] = a.hostname.empty() ? a.agent_id : a.hostname;

    json top5 = json::array();
    for (const auto& aa : sorted_agents)
    {
        const int64_t age_seconds = (now_ms - aa.oldest_alert_ms) / 1000;
        top5.push_back({{"agent_id", aa.agent_id},
                        {"name", agent_names.count(aa.agent_id) ? agent_names.at(aa.agent_id) : aa.agent_id},
                        {"alert_count", aa.alert_count},
                        {"oldest_alert_age_seconds", age_seconds}});
    }

    return json{{"generated_at", now_ms},
                {"agents",
                 {{"total", total}, {"online", total - offline - maintenance}, {"offline", offline},
                  {"maintenance", maintenance}}},
                {"alerts",
                 {{"total", static_cast<int>(alerts.size())}, {"red", red_count}, {"amber", amber_count},
                  {"yellow", yellow_count}}},
                {"top_agents_by_alert_age", top5}};
}

bool generate_and_send_report(Store& store, int64_t now_ms)
{
    const auto webhook_url = store.get_setting("reports.webhook_url", "");
    if (webhook_url.empty())
    {
        LOG_DEBUG("No reports.webhook_url configured, skipping report");
        return false;
    }

    const auto target = parse_webhook(webhook_url);
    if (!target)
    {
        LOGF_WARNING("Reports: invalid or private webhook URL: %s", webhook_url.c_str());
        return false;
    }

    const auto payload = build_report_json(store, now_ms);

    httplib::Client client(target->host, target->port);
    client.set_connection_timeout(5, 0);
    client.set_read_timeout(5, 0);
    auto response = client.Post(target->path, payload.dump(), "application/json");
    if (!response)
    {
        LOGF_WARNING("Reports: webhook POST failed url=%s", webhook_url.c_str());
        return false;
    }
    LOGF_INFO("Reports: digest sent status=%d url=%s", response->status, webhook_url.c_str());
    return true;
}

} // namespace thewatcher::server
