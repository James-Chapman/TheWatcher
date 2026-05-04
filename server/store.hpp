#pragma once

#include "common/collector_config.hpp"
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
    std::string maintenance_reason;
    int64_t maintenance_until = 0;
    int collection_interval = 30;
    int process_limit = 25;
    int64_t first_seen = 0;
    int64_t last_seen = 0;
    double cpu_warning_pct_of_avg = 125.0;
    double cpu_degraded_pct_of_avg = 150.0;
    double cpu_critical_pct_of_avg = 200.0;
    double memory_warning_pct_of_avg = 125.0;
    double memory_degraded_pct_of_avg = 150.0;
    double memory_critical_pct_of_avg = 200.0;
    double disk_warning_pct_of_avg = 125.0;
    double disk_degraded_pct_of_avg = 150.0;
    double disk_critical_pct_of_avg = 200.0;
    double network_warning_pct_of_avg = 125.0;
    double network_degraded_pct_of_avg = 150.0;
    double network_critical_pct_of_avg = 200.0;
    CollectorConfig collector_config;
};

struct MetricsRow
{
    std::string agent_id;
    int64_t timestamp_ms = 0;
    std::vector<uint8_t> metrics_cbor; // CBOR-encoded SystemMetrics payload (matches the wire format).
};

struct GroupRecord
{
    int64_t group_id = 0;
    std::string name;
    bool built_in = false;
};

struct UserRecord
{
    int64_t user_id = 0;
    std::string username;
    std::string password_hash;
    std::string role;
    bool built_in = false;
    bool disabled = false;
};

struct SessionRecord
{
    std::string token;
    int64_t user_id = 0;
    std::string username;
    std::string role;
    int64_t created_at = 0;
    int64_t expires_at = 0;
};

struct StatusHistoryRow
{
    int64_t id = 0;
    std::string agent_id;
    std::string indicator;
    std::string old_status;
    std::string new_status;
    std::string message;
    int64_t created_at = 0;
};

struct AlertRecord
{
    int64_t alert_id = 0;
    std::string agent_id;
    std::string indicator;
    std::string old_status;
    std::string new_status;
    std::string message;
    int64_t created_at = 0;
    std::string acknowledged_by;
    int64_t acknowledged_at = 0;
    int64_t deleted_at = 0;
    std::string note;
};

struct PendingStatusRecord
{
    std::string agent_id;
    std::string indicator;
    std::string target_status;
    int count = 0;
};

class Store
{
public:
    virtual ~Store() = default;

    virtual void upsert_agent(const AgentRecord& rec) = 0;
    virtual std::optional<AgentRecord> get_agent(const std::string& agent_id) = 0;
    virtual std::vector<AgentRecord> list_agents() = 0;
    virtual std::vector<AgentRecord> list_approved_agents() = 0;
    virtual std::vector<AgentRecord> list_pending_agents() = 0;
    virtual void approve_agent(const std::string& agent_id) = 0;
    virtual void reject_agent(const std::string& agent_id) = 0;
    virtual void delete_agent(const std::string& agent_id) = 0;
    virtual void mark_agents_offline_before(int64_t cutoff_ms) = 0;
    virtual void set_agent_maintenance(const std::string& agent_id, bool maintenance, const std::string& reason,
                                       int64_t until_ms) = 0;
    virtual void set_agent_thresholds(const AgentRecord& rec) = 0;
    virtual void set_agent_collector_config(const std::string& agent_id, const CollectorConfig& config) = 0;

    virtual std::vector<GroupRecord> list_groups() = 0;
    virtual int64_t create_group(const std::string& name) = 0;
    virtual std::vector<int64_t> get_agent_groups(const std::string& agent_id) = 0;
    virtual void set_agent_groups(const std::string& agent_id, const std::vector<int64_t>& group_ids) = 0;
    virtual std::optional<UserRecord> get_user_by_username(const std::string& username) = 0;
    virtual std::vector<UserRecord> list_users() = 0;
    virtual int64_t create_user(const std::string& username, const std::string& password_hash,
                                const std::string& role) = 0;
    virtual void set_user_groups(int64_t user_id, const std::vector<int64_t>& group_ids) = 0;
    virtual std::vector<int64_t> get_user_groups(int64_t user_id) = 0;
    virtual void create_session(const SessionRecord& session) = 0;
    virtual std::optional<SessionRecord> get_session(const std::string& token, int64_t now_ms) = 0;
    virtual void delete_session(const std::string& token) = 0;

    virtual void insert_metrics(const MetricsRow& row) = 0;
    virtual std::vector<MetricsRow> get_metrics(const std::string& agent_id, int limit = 100) = 0;
    virtual std::vector<MetricsRow> latest_metrics() = 0;

    virtual void insert_status_history(const StatusHistoryRow& row) = 0;
    virtual std::optional<StatusHistoryRow> latest_status_for_indicator(const std::string& agent_id,
                                                                        const std::string& indicator) = 0;
    virtual std::vector<StatusHistoryRow> list_status_history(const std::string& agent_id, int limit = 100) = 0;
    virtual std::optional<PendingStatusRecord> get_pending_status(const std::string& agent_id,
                                                                  const std::string& indicator) = 0;
    virtual void set_pending_status(const std::string& agent_id, const std::string& indicator,
                                    const std::string& target_status, int count) = 0;
    virtual void clear_pending_status(const std::string& agent_id, const std::string& indicator) = 0;
    virtual int64_t insert_alert(const AlertRecord& alert) = 0;
    virtual std::vector<AlertRecord> list_alerts(bool include_deleted = false) = 0;
    virtual std::vector<AlertRecord> list_unacknowledged_alerts() = 0;
    virtual void acknowledge_alert(int64_t alert_id, const std::string& username, int64_t acknowledged_at,
                                   const std::string& note = "") = 0;
    virtual void bulk_acknowledge_alerts(const std::vector<int64_t>& alert_ids, const std::string& username,
                                         int64_t acknowledged_at, const std::string& note = "") = 0;
    virtual void soft_delete_alert(int64_t alert_id, int64_t deleted_at) = 0;
    virtual void bulk_soft_delete_alerts(const std::vector<int64_t>& alert_ids, int64_t deleted_at) = 0;
    virtual void clear_active_alerts_for_agent(const std::string& agent_id, int64_t cleared_at) = 0;
    virtual std::vector<std::string> get_offline_unalerted_agent_ids() = 0;
    virtual void archive_heartbeat_alerts_for_agent(const std::string& agent_id, int64_t deleted_at) = 0;
    virtual std::string get_setting(const std::string& key, const std::string& fallback = "") = 0;
    virtual void set_setting(const std::string& key, const std::string& value) = 0;
};

std::unique_ptr<Store> make_store(const std::string& db_type, const std::string& db_path_or_dsn);

} // namespace thewatcher::server
