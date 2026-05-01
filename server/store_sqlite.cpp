#include "store_sqlite.hpp"

#include "common/SingleLog.hpp"

#include <stdexcept>
#include <string>

namespace thewatcher::server
{

// ── Helpers ───────────────────────────────────────────────────────────────────

namespace
{

    struct Stmt
    {
        sqlite3_stmt* s = nullptr;
        ~Stmt()
        {
            if (s)
                sqlite3_finalize(s);
        }
        sqlite3_stmt* operator->()
        {
            return s;
        }
    };

    void check(int rc, sqlite3* db, const char* ctx)
    {
        if (rc != SQLITE_OK && rc != SQLITE_ROW && rc != SQLITE_DONE)
            throw std::runtime_error(std::string(ctx) + ": " + sqlite3_errmsg(db));
    }

    AgentRecord row_to_agent(sqlite3_stmt* s)
    {
        AgentRecord r;
        r.agent_id = reinterpret_cast<const char*>(sqlite3_column_text(s, 0));
        r.hostname = reinterpret_cast<const char*>(sqlite3_column_text(s, 1));
        r.platform = reinterpret_cast<const char*>(sqlite3_column_text(s, 2));
        r.curve_public_key_z85 = reinterpret_cast<const char*>(sqlite3_column_text(s, 3));
        r.approved = sqlite3_column_int(s, 4) != 0;
        r.rejected = sqlite3_column_int(s, 5) != 0;
        r.connected = sqlite3_column_int(s, 6) != 0;
        r.maintenance = sqlite3_column_int(s, 7) != 0;
        r.collection_interval = sqlite3_column_int(s, 8);
        r.process_limit = sqlite3_column_int(s, 9);
        r.first_seen = sqlite3_column_int64(s, 10);
        r.last_seen = sqlite3_column_int64(s, 11);
        return r;
    }

    MetricsRow row_to_metrics(sqlite3_stmt* s)
    {
        MetricsRow r;
        r.agent_id = reinterpret_cast<const char*>(sqlite3_column_text(s, 0));
        r.timestamp_ms = sqlite3_column_int64(s, 1);
        r.metrics_json = reinterpret_cast<const char*>(sqlite3_column_text(s, 2));
        return r;
    }

} // namespace

// ── Construction ──────────────────────────────────────────────────────────────

SqliteStore::SqliteStore(const std::string& path)
{
    LOG_FUNCTION_TRACE
    LOGF_DEBUG("Opening SQLite store path=%s", path.c_str());
    int rc = sqlite3_open(path.c_str(), &db_);
    check(rc, db_, "sqlite3_open");
    init_schema();
}

SqliteStore::~SqliteStore()
{
    LOG_FUNCTION_TRACE
    if (db_)
    {
        sqlite3_close(db_);
        LOG_DEBUG("SQLite store closed");
    }
}

void SqliteStore::exec(const char* sql)
{
    LOGF_TRACE("Executing SQL statement prefix=%.80s", sql);
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK)
    {
        std::string msg = err ? err : "unknown";
        sqlite3_free(err);
        throw std::runtime_error(std::string("sqlite exec: ") + msg);
    }
}

bool SqliteStore::column_exists(const std::string& table, const std::string& column)
{
    const auto sql = "PRAGMA table_info(" + table + ");";
    Stmt st;
    check(sqlite3_prepare_v2(db_, sql.c_str(), -1, &st.s, nullptr), db_, "prepare table_info");
    while (sqlite3_step(st.s) == SQLITE_ROW)
    {
        const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(st.s, 1));
        if (name != nullptr && column == name)
            return true;
    }
    return false;
}

void SqliteStore::add_column_if_missing(const std::string& table, const std::string& column, const char* ddl)
{
    if (!column_exists(table, column))
        exec(ddl);
}

void SqliteStore::init_schema()
{
    LOG_FUNCTION_TRACE
    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA foreign_keys=ON;");
    exec(R"(
        CREATE TABLE IF NOT EXISTS agents (
            agent_id             TEXT PRIMARY KEY,
            hostname             TEXT NOT NULL DEFAULT '',
            platform             TEXT NOT NULL DEFAULT '',
            curve_public_key_z85 TEXT NOT NULL DEFAULT '',
            approved             INTEGER NOT NULL DEFAULT 0,
            rejected             INTEGER NOT NULL DEFAULT 0,
            connected            INTEGER NOT NULL DEFAULT 0,
            maintenance          INTEGER NOT NULL DEFAULT 0,
            collection_interval  INTEGER NOT NULL DEFAULT 30,
            process_limit        INTEGER NOT NULL DEFAULT 25,
            first_seen           INTEGER NOT NULL DEFAULT 0,
            last_seen            INTEGER NOT NULL DEFAULT 0
        );
    )");
    add_column_if_missing("agents", "rejected", "ALTER TABLE agents ADD COLUMN rejected INTEGER NOT NULL DEFAULT 0;");
    add_column_if_missing("agents", "connected", "ALTER TABLE agents ADD COLUMN connected INTEGER NOT NULL DEFAULT 0;");
    add_column_if_missing("agents", "maintenance",
                          "ALTER TABLE agents ADD COLUMN maintenance INTEGER NOT NULL DEFAULT 0;");
    add_column_if_missing("agents", "collection_interval",
                          "ALTER TABLE agents ADD COLUMN collection_interval INTEGER NOT NULL DEFAULT 30;");
    add_column_if_missing("agents", "process_limit",
                          "ALTER TABLE agents ADD COLUMN process_limit INTEGER NOT NULL DEFAULT 25;");
    exec(R"(
        CREATE TABLE IF NOT EXISTS metrics (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            agent_id     TEXT NOT NULL,
            timestamp_ms INTEGER NOT NULL,
            metrics_json TEXT NOT NULL,
            FOREIGN KEY (agent_id) REFERENCES agents(agent_id) ON DELETE CASCADE
        );
    )");
    exec("CREATE INDEX IF NOT EXISTS idx_metrics_agent_ts ON metrics(agent_id, timestamp_ms DESC);");
}

// ── Agent CRUD ────────────────────────────────────────────────────────────────

void SqliteStore::upsert_agent(const AgentRecord& rec)
{
    LOGF_TRACE("Upserting agent id=%s approved=%d rejected=%d connected=%d maintenance=%d interval=%d process_limit=%d",
               rec.agent_id.c_str(), rec.approved ? 1 : 0, rec.rejected ? 1 : 0, rec.connected ? 1 : 0,
               rec.maintenance ? 1 : 0, rec.collection_interval, rec.process_limit);
    const char* sql = R"(
        INSERT INTO agents(agent_id,hostname,platform,curve_public_key_z85,approved,rejected,connected,maintenance,collection_interval,process_limit,first_seen,last_seen)
        VALUES(?,?,?,?,?,?,?,?,?,?,?,?)
        ON CONFLICT(agent_id) DO UPDATE SET
            hostname=excluded.hostname,
            platform=excluded.platform,
            curve_public_key_z85=excluded.curve_public_key_z85,
            connected=excluded.connected,
            maintenance=excluded.maintenance,
            collection_interval=excluded.collection_interval,
            process_limit=excluded.process_limit,
            last_seen=excluded.last_seen;
    )";
    Stmt st;
    check(sqlite3_prepare_v2(db_, sql, -1, &st.s, nullptr), db_, "prepare upsert_agent");
    sqlite3_bind_text(st.s, 1, rec.agent_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st.s, 2, rec.hostname.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st.s, 3, rec.platform.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st.s, 4, rec.curve_public_key_z85.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st.s, 5, rec.approved ? 1 : 0);
    sqlite3_bind_int(st.s, 6, rec.rejected ? 1 : 0);
    sqlite3_bind_int(st.s, 7, rec.connected ? 1 : 0);
    sqlite3_bind_int(st.s, 8, rec.maintenance ? 1 : 0);
    sqlite3_bind_int(st.s, 9, rec.collection_interval);
    sqlite3_bind_int(st.s, 10, rec.process_limit);
    sqlite3_bind_int64(st.s, 11, rec.first_seen);
    sqlite3_bind_int64(st.s, 12, rec.last_seen);
    check(sqlite3_step(st.s), db_, "step upsert_agent");
}

std::optional<AgentRecord> SqliteStore::get_agent(const std::string& agent_id)
{
    LOGF_TRACE("Getting agent id=%s", agent_id.c_str());
    const char* sql = "SELECT "
                      "agent_id,hostname,platform,curve_public_key_z85,approved,rejected,connected,maintenance,"
                      "collection_interval,process_limit,first_seen,last_seen "
                      "FROM agents WHERE agent_id=?;";
    Stmt st;
    check(sqlite3_prepare_v2(db_, sql, -1, &st.s, nullptr), db_, "prepare get_agent");
    sqlite3_bind_text(st.s, 1, agent_id.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st.s);
    if (rc == SQLITE_ROW)
    {
        LOGF_TRACE("Agent found id=%s", agent_id.c_str());
        return row_to_agent(st.s);
    }
    LOGF_TRACE("Agent not found id=%s", agent_id.c_str());
    return std::nullopt;
}

std::vector<AgentRecord> SqliteStore::list_agents()
{
    LOG_TRACE("Listing agents");
    const char* sql = "SELECT "
                      "agent_id,hostname,platform,curve_public_key_z85,approved,rejected,connected,maintenance,"
                      "collection_interval,process_limit,first_seen,last_seen "
                      "FROM agents ORDER BY last_seen DESC;";
    Stmt st;
    check(sqlite3_prepare_v2(db_, sql, -1, &st.s, nullptr), db_, "prepare list_agents");
    std::vector<AgentRecord> out;
    while (sqlite3_step(st.s) == SQLITE_ROW)
        out.push_back(row_to_agent(st.s));
    LOGF_TRACE("Listed %zu agent(s)", out.size());
    return out;
}

void SqliteStore::approve_agent(const std::string& agent_id)
{
    LOGF_INFO("Approving agent in store id=%s", agent_id.c_str());
    const char* sql = "UPDATE agents SET approved=1,rejected=0 WHERE agent_id=?;";
    Stmt st;
    check(sqlite3_prepare_v2(db_, sql, -1, &st.s, nullptr), db_, "prepare approve_agent");
    sqlite3_bind_text(st.s, 1, agent_id.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(st.s), db_, "step approve_agent");
}

void SqliteStore::reject_agent(const std::string& agent_id)
{
    LOGF_INFO("Rejecting agent in store id=%s", agent_id.c_str());
    const char* sql = "UPDATE agents SET approved=0,rejected=1 WHERE agent_id=?;";
    Stmt st;
    check(sqlite3_prepare_v2(db_, sql, -1, &st.s, nullptr), db_, "prepare reject_agent");
    sqlite3_bind_text(st.s, 1, agent_id.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(st.s), db_, "step reject_agent");
}

void SqliteStore::delete_agent(const std::string& agent_id)
{
    LOGF_INFO("Deleting agent from store id=%s", agent_id.c_str());
    const char* sql = "DELETE FROM agents WHERE agent_id=?;";
    Stmt st;
    check(sqlite3_prepare_v2(db_, sql, -1, &st.s, nullptr), db_, "prepare delete_agent");
    sqlite3_bind_text(st.s, 1, agent_id.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(st.s), db_, "step delete_agent");
}

void SqliteStore::mark_agents_offline_before(int64_t cutoff_ms)
{
    LOGF_TRACE("Marking stale connected agents offline cutoff_ms=%lld", static_cast<long long>(cutoff_ms));
    const char* sql = "UPDATE agents SET connected=0 WHERE approved=1 AND connected=1 AND last_seen>0 AND last_seen<?;";
    Stmt st;
    check(sqlite3_prepare_v2(db_, sql, -1, &st.s, nullptr), db_, "prepare mark_agents_offline_before");
    sqlite3_bind_int64(st.s, 1, cutoff_ms);
    check(sqlite3_step(st.s), db_, "step mark_agents_offline_before");
}

// ── Metrics ───────────────────────────────────────────────────────────────────

void SqliteStore::insert_metrics(const MetricsRow& row)
{
    LOGF_TRACE("Inserting metrics agent_id=%s timestamp_ms=%lld json_size=%zu", row.agent_id.c_str(),
               static_cast<long long>(row.timestamp_ms), row.metrics_json.size());
    const char* sql = "INSERT INTO metrics(agent_id,timestamp_ms,metrics_json) VALUES(?,?,?);";
    Stmt st;
    check(sqlite3_prepare_v2(db_, sql, -1, &st.s, nullptr), db_, "prepare insert_metrics");
    sqlite3_bind_text(st.s, 1, row.agent_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st.s, 2, row.timestamp_ms);
    sqlite3_bind_text(st.s, 3, row.metrics_json.c_str(), -1, SQLITE_TRANSIENT);
    check(sqlite3_step(st.s), db_, "step insert_metrics");
}

std::vector<MetricsRow> SqliteStore::get_metrics(const std::string& agent_id, int limit)
{
    LOGF_TRACE("Getting metrics agent_id=%s limit=%d", agent_id.c_str(), limit);
    const char* sql = "SELECT agent_id,timestamp_ms,metrics_json FROM metrics "
                      "WHERE agent_id=? ORDER BY timestamp_ms DESC LIMIT ?;";
    Stmt st;
    check(sqlite3_prepare_v2(db_, sql, -1, &st.s, nullptr), db_, "prepare get_metrics");
    sqlite3_bind_text(st.s, 1, agent_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st.s, 2, limit);
    std::vector<MetricsRow> out;
    while (sqlite3_step(st.s) == SQLITE_ROW)
        out.push_back(row_to_metrics(st.s));
    LOGF_TRACE("Got %zu metric row(s) for agent_id=%s", out.size(), agent_id.c_str());
    return out;
}

std::vector<MetricsRow> SqliteStore::latest_metrics()
{
    LOG_TRACE("Getting latest metrics for all agents");
    const char* sql = R"(
        SELECT m.agent_id, m.timestamp_ms, m.metrics_json
        FROM metrics m
        INNER JOIN (
            SELECT agent_id, MAX(timestamp_ms) AS ts FROM metrics GROUP BY agent_id
        ) latest ON m.agent_id = latest.agent_id AND m.timestamp_ms = latest.ts
        ORDER BY m.agent_id;
    )";
    Stmt st;
    check(sqlite3_prepare_v2(db_, sql, -1, &st.s, nullptr), db_, "prepare latest_metrics");
    std::vector<MetricsRow> out;
    while (sqlite3_step(st.s) == SQLITE_ROW)
        out.push_back(row_to_metrics(st.s));
    LOGF_TRACE("Got %zu latest metric row(s)", out.size());
    return out;
}

// ── Factory ───────────────────────────────────────────────────────────────────

std::unique_ptr<Store> make_store(const std::string& db_type, const std::string& db_path_or_dsn)
{
    LOGF_DEBUG("Creating store db_type=%s target=%s", db_type.c_str(), db_path_or_dsn.c_str());
    if (db_type == "sqlite")
        return std::make_unique<SqliteStore>(db_path_or_dsn);
    throw std::runtime_error("Unsupported db_type: " + db_type + " (compile with postgres support)");
}

} // namespace thewatcher::server
