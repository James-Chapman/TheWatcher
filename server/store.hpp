#pragma once

#include "common/metrics.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace thewatcher::server
{

struct AgentRecord
{
    std::string agent_id;
    std::string hostname;
    std::string platform;
    std::string curve_public_key_z85;
    bool approved = false;
    bool rejected = false;
    bool connected = false;
    bool maintenance = false;
    int collection_interval = 30;
    int process_limit = 25;
    int64_t first_seen = 0;
    int64_t last_seen = 0;
};

struct MetricsRow
{
    std::string agent_id;
    int64_t timestamp_ms = 0;
    std::string metrics_json;
};

class Store
{
public:
    virtual ~Store() = default;

    // ── Agent enrollment ──────────────────────────────────────────────────────
    virtual void upsert_agent(const AgentRecord& rec) = 0;
    virtual std::optional<AgentRecord> get_agent(const std::string& agent_id) = 0;
    virtual std::vector<AgentRecord> list_agents() = 0;
    virtual void approve_agent(const std::string& agent_id) = 0;
    virtual void reject_agent(const std::string& agent_id) = 0;
    virtual void delete_agent(const std::string& agent_id) = 0;
    virtual void mark_agents_offline_before(int64_t cutoff_ms) = 0;

    // ── Metrics ───────────────────────────────────────────────────────────────
    virtual void insert_metrics(const MetricsRow& row) = 0;

    // Returns up to `limit` rows for the agent, newest first.
    virtual std::vector<MetricsRow> get_metrics(const std::string& agent_id, int limit = 100) = 0;

    // Returns the single latest metrics row for each agent.
    virtual std::vector<MetricsRow> latest_metrics() = 0;
};

std::unique_ptr<Store> make_store(const std::string& db_type, const std::string& db_path_or_dsn);

} // namespace thewatcher::server
