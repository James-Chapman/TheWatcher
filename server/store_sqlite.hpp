#pragma once

#include "store.hpp"

#include <sqlite3.h>
#include <string>

namespace thewatcher::server
{

class SqliteStore final : public Store
{
public:
    explicit SqliteStore(const std::string& path);
    ~SqliteStore() override;

    void upsert_agent(const AgentRecord& rec) override;
    std::optional<AgentRecord> get_agent(const std::string& agent_id) override;
    std::vector<AgentRecord> list_agents() override;
    void approve_agent(const std::string& agent_id) override;
    void reject_agent(const std::string& agent_id) override;
    void delete_agent(const std::string& agent_id) override;
    void mark_agents_offline_before(int64_t cutoff_ms) override;

    void insert_metrics(const MetricsRow& row) override;
    std::vector<MetricsRow> get_metrics(const std::string& agent_id, int limit) override;
    std::vector<MetricsRow> latest_metrics() override;

private:
    void exec(const char* sql);
    bool column_exists(const std::string& table, const std::string& column);
    void add_column_if_missing(const std::string& table, const std::string& column, const char* ddl);
    void init_schema();

    sqlite3* db_ = nullptr;
};

} // namespace thewatcher::server
