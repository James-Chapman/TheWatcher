#include "../server/store_sqlite.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

using namespace thewatcher::server;

namespace
{
std::filesystem::path unique_store_path(const char* name)
{
    auto dir = std::filesystem::temp_directory_path() / "thewatcher-store-tests";
    std::filesystem::create_directories(dir);
    auto path = dir / name;
    std::filesystem::remove(path);
    std::filesystem::remove(path.string() + "-wal");
    std::filesystem::remove(path.string() + "-shm");
    return path;
}
} // namespace

// Each SCENARIO creates its own in-memory SQLite database, so tests are
// fully isolated and leave no files on disk.

// ── Empty store ───────────────────────────────────────────────────────────────

SCENARIO("A freshly created store has no agents or metrics")
{
    GIVEN("an in-memory SQLite store")
    {
        SqliteStore store(":memory:");

        THEN("list_agents returns an empty vector")
        {
            REQUIRE(store.list_agents().empty());
        }

        AND_THEN("latest_metrics returns an empty vector")
        {
            REQUIRE(store.latest_metrics().empty());
        }
    }
}

// ── Agent upsert and retrieval ────────────────────────────────────────────────

SCENARIO("An agent can be upserted and retrieved by id")
{
    GIVEN("an in-memory store and a populated AgentRecord")
    {
        SqliteStore store(":memory:");

        AgentRecord rec;
        rec.agent_id = "agent-001";
        rec.hostname = "test-host";
        rec.platform = "linux";
        rec.curve_public_key_z85 = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
        rec.approved = false;
        rec.first_seen = 1000;
        rec.last_seen = 2000;

        WHEN("the record is upserted")
        {
            store.upsert_agent(rec);

            THEN("get_agent returns the stored record")
            {
                auto got = store.get_agent("agent-001");
                REQUIRE(got.has_value());
                REQUIRE(got->agent_id == "agent-001");
                REQUIRE(got->hostname == "test-host");
                REQUIRE(got->platform == "linux");
                REQUIRE(got->curve_public_key_z85 == "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
                REQUIRE(got->approved == false);
                REQUIRE(got->first_seen == 1000);
                REQUIRE(got->last_seen == 2000);
            }
        }
    }
}

SCENARIO("get_agent returns nullopt for an unknown id")
{
    GIVEN("an empty store")
    {
        SqliteStore store(":memory:");

        WHEN("get_agent is called with an id that was never inserted")
        {
            auto result = store.get_agent("does-not-exist");

            THEN("nullopt is returned")
            {
                REQUIRE_FALSE(result.has_value());
            }
        }
    }
}

SCENARIO("Upserting an existing agent updates mutable fields but preserves first_seen")
{
    GIVEN("a store with an existing agent")
    {
        SqliteStore store(":memory:");

        AgentRecord rec;
        rec.agent_id = "agent-004";
        rec.hostname = "original-host";
        rec.first_seen = 500;
        rec.last_seen = 600;
        store.upsert_agent(rec);

        WHEN("the same agent is upserted with an updated hostname and last_seen")
        {
            rec.hostname = "updated-host";
            rec.last_seen = 700;
            store.upsert_agent(rec);

            THEN("hostname and last_seen are updated")
            {
                auto got = store.get_agent("agent-004");
                REQUIRE(got.has_value());
                REQUIRE(got->hostname == "updated-host");
                REQUIRE(got->last_seen == 700);
            }

            AND_THEN("first_seen is preserved from the original insert")
            {
                auto got = store.get_agent("agent-004");
                REQUIRE(got.has_value());
                REQUIRE(got->first_seen == 500);
            }
        }
    }
}

// ── Agent list ────────────────────────────────────────────────────────────────

SCENARIO("list_agents returns all stored agents")
{
    GIVEN("a store with two agents inserted")
    {
        SqliteStore store(":memory:");

        for (auto id : {"agent-A", "agent-B"})
        {
            AgentRecord r;
            r.agent_id = id;
            r.first_seen = 1;
            r.last_seen = 1;
            store.upsert_agent(r);
        }

        WHEN("list_agents is called")
        {
            auto agents = store.list_agents();

            THEN("exactly two records are returned")
            {
                REQUIRE(agents.size() == 2);
            }
        }
    }
}

// ── Approval ──────────────────────────────────────────────────────────────────

SCENARIO("Approving an agent sets the approved flag to true")
{
    GIVEN("a store with an unapproved agent")
    {
        SqliteStore store(":memory:");

        AgentRecord rec;
        rec.agent_id = "agent-002";
        rec.approved = false;
        rec.first_seen = 1;
        rec.last_seen = 1;
        store.upsert_agent(rec);

        WHEN("approve_agent is called")
        {
            store.approve_agent("agent-002");

            THEN("get_agent shows approved = true")
            {
                auto got = store.get_agent("agent-002");
                REQUIRE(got.has_value());
                REQUIRE(got->approved == true);
            }
        }
    }
}

SCENARIO("Rejecting an agent prevents later enrollment approval")
{
    GIVEN("a store with a pending agent")
    {
        SqliteStore store(":memory:");

        AgentRecord rec;
        rec.agent_id = "agent-reject";
        rec.approved = false;
        rec.rejected = false;
        rec.first_seen = 1;
        rec.last_seen = 1;
        store.upsert_agent(rec);

        WHEN("reject_agent is called")
        {
            store.reject_agent("agent-reject");

            THEN("get_agent shows rejected = true and approved = false")
            {
                auto got = store.get_agent("agent-reject");
                REQUIRE(got.has_value());
                REQUIRE(got->rejected == true);
                REQUIRE(got->approved == false);
            }
        }
    }
}

SCENARIO("Agent maintenance state is persisted across upserts")
{
    GIVEN("a store with an approved agent")
    {
        SqliteStore store(":memory:");

        AgentRecord rec;
        rec.agent_id = "agent-maintenance";
        rec.approved = true;
        rec.maintenance = false;
        rec.first_seen = 1;
        rec.last_seen = 1;
        store.upsert_agent(rec);

        WHEN("the agent is upserted after entering maintenance mode")
        {
            rec.maintenance = true;
            rec.last_seen = 2;
            store.upsert_agent(rec);

            THEN("get_agent shows maintenance = true")
            {
                auto got = store.get_agent("agent-maintenance");
                REQUIRE(got.has_value());
                REQUIRE(got->maintenance == true);
            }
        }
    }
}

SCENARIO("Agent runtime settings are persisted across upserts")
{
    GIVEN("a store with an approved agent")
    {
        SqliteStore store(":memory:");

        AgentRecord rec;
        rec.agent_id = "agent-settings";
        rec.approved = true;
        rec.collection_interval = 30;
        rec.process_limit = 25;
        rec.first_seen = 1;
        rec.last_seen = 1;
        store.upsert_agent(rec);

        WHEN("the agent is upserted with updated runtime settings")
        {
            rec.collection_interval = 12;
            rec.process_limit = 7;
            rec.last_seen = 2;
            store.upsert_agent(rec);

            THEN("get_agent returns the updated settings")
            {
                auto got = store.get_agent("agent-settings");
                REQUIRE(got.has_value());
                REQUIRE(got->collection_interval == 12);
                REQUIRE(got->process_limit == 7);
            }
        }
    }
}

SCENARIO("Agent threshold settings are persisted across upserts")
{
    GIVEN("a store with an approved agent")
    {
        SqliteStore store(":memory:");

        AgentRecord rec;
        rec.agent_id = "agent-thresholds";
        rec.approved = true;
        rec.first_seen = 1;
        rec.last_seen = 1;
        store.upsert_agent(rec);

        WHEN("per-agent indicator thresholds are saved")
        {
            rec.cpu_warning_pct_of_avg = 110.0;
            rec.cpu_degraded_pct_of_avg = 140.0;
            rec.cpu_critical_pct_of_avg = 180.0;
            rec.memory_warning_pct_of_avg = 115.0;
            rec.memory_degraded_pct_of_avg = 145.0;
            rec.memory_critical_pct_of_avg = 185.0;
            rec.disk_warning_pct_of_avg = 120.0;
            rec.disk_degraded_pct_of_avg = 150.0;
            rec.disk_critical_pct_of_avg = 190.0;
            rec.network_warning_pct_of_avg = 130.0;
            rec.network_degraded_pct_of_avg = 160.0;
            rec.network_critical_pct_of_avg = 210.0;
            store.set_agent_thresholds(rec);

            THEN("get_agent returns the stored threshold values")
            {
                auto got = store.get_agent("agent-thresholds");
                REQUIRE(got.has_value());
                REQUIRE(got->cpu_warning_pct_of_avg == 110.0);
                REQUIRE(got->cpu_degraded_pct_of_avg == 140.0);
                REQUIRE(got->cpu_critical_pct_of_avg == 180.0);
                REQUIRE(got->memory_warning_pct_of_avg == 115.0);
                REQUIRE(got->memory_degraded_pct_of_avg == 145.0);
                REQUIRE(got->memory_critical_pct_of_avg == 185.0);
                REQUIRE(got->disk_warning_pct_of_avg == 120.0);
                REQUIRE(got->disk_degraded_pct_of_avg == 150.0);
                REQUIRE(got->disk_critical_pct_of_avg == 190.0);
                REQUIRE(got->network_warning_pct_of_avg == 130.0);
                REQUIRE(got->network_degraded_pct_of_avg == 160.0);
                REQUIRE(got->network_critical_pct_of_avg == 210.0);
            }
        }
    }
}

SCENARIO("Agent maintenance metadata and group membership survive reopening the store")
{
    GIVEN("a file-backed store with a maintained approved agent assigned to groups")
    {
        auto path = unique_store_path("agent-groups-maintenance.db");
        int64_t engineering = 0;
        int64_t production = 0;

        {
            SqliteStore store(path.string());
            engineering = store.create_group("Engineering");
            production = store.create_group("Production");

            AgentRecord rec;
            rec.agent_id = "agent-grouped";
            rec.hostname = "grouped-host";
            rec.approved = true;
            rec.maintenance = true;
            rec.maintenance_reason = "patch window";
            rec.maintenance_until = 123456;
            rec.first_seen = 1;
            rec.last_seen = 2;
            store.upsert_agent(rec);
            store.set_agent_groups(rec.agent_id, {engineering, production});
        }

        WHEN("the store is reopened")
        {
            SqliteStore reopened(path.string());

            THEN("the agent maintenance metadata is retained")
            {
                auto got = reopened.get_agent("agent-grouped");
                REQUIRE(got.has_value());
                REQUIRE(got->maintenance == true);
                REQUIRE(got->maintenance_reason == "patch window");
                REQUIRE(got->maintenance_until == 123456);
            }

            AND_THEN("the agent group membership is retained")
            {
                auto groups = reopened.get_agent_groups("agent-grouped");
                REQUIRE(groups.size() == 2);
                REQUIRE(groups[0] == engineering);
                REQUIRE(groups[1] == production);
            }
        }
    }
}

SCENARIO("A new store bootstraps the default admin user and Admins group")
{
    GIVEN("an empty in-memory store")
    {
        SqliteStore store(":memory:");

        WHEN("users and groups are listed")
        {
            auto user = store.get_user_by_username("thewatcher");
            auto groups = store.list_groups();

            THEN("the default admin user exists without storing the plaintext password")
            {
                REQUIRE(user.has_value());
                REQUIRE(user->role == "admin");
                REQUIRE(user->built_in == true);
                REQUIRE(user->password_hash != "look_at_me");
            }

            AND_THEN("the Admins group exists")
            {
                bool found = false;
                for (const auto& group : groups)
                    found = found || (group.name == "Admins" && group.built_in);
                REQUIRE(found);
            }
        }
    }
}

SCENARIO("Soft-deleted alerts are hidden from active alert queries but remain historical")
{
    GIVEN("a store with an agent and an alert")
    {
        SqliteStore store(":memory:");
        AgentRecord rec;
        rec.agent_id = "agent-alert";
        rec.approved = true;
        rec.first_seen = 1;
        rec.last_seen = 1;
        store.upsert_agent(rec);

        AlertRecord alert;
        alert.agent_id = rec.agent_id;
        alert.indicator = "cpu";
        alert.old_status = "green";
        alert.new_status = "red";
        alert.message = "cpu changed from green to red";
        alert.created_at = 100;
        auto alert_id = store.insert_alert(alert);

        WHEN("the alert is soft-deleted")
        {
            store.soft_delete_alert(alert_id, 200);

            THEN("active alert queries hide it")
            {
                REQUIRE(store.list_alerts(false).empty());
                REQUIRE(store.list_unacknowledged_alerts().empty());
            }

            AND_THEN("historical queries can still include it")
            {
                auto all = store.list_alerts(true);
                REQUIRE(all.size() == 1);
                REQUIRE(all[0].deleted_at == 200);
            }
        }
    }
}

SCENARIO("Approved agents are marked offline after the configured cutoff")
{
    GIVEN("a store with a connected approved agent last seen before the cutoff")
    {
        SqliteStore store(":memory:");

        AgentRecord rec;
        rec.agent_id = "agent-offline";
        rec.approved = true;
        rec.connected = true;
        rec.first_seen = 1;
        rec.last_seen = 1000;
        store.upsert_agent(rec);

        WHEN("mark_agents_offline_before is called with a newer cutoff")
        {
            store.mark_agents_offline_before(2000);

            THEN("the agent is no longer connected")
            {
                auto got = store.get_agent("agent-offline");
                REQUIRE(got.has_value());
                REQUIRE(got->connected == false);
            }
        }
    }
}

// ── Deletion ──────────────────────────────────────────────────────────────────

SCENARIO("Deleting an agent removes it from the store")
{
    GIVEN("a store with a known agent")
    {
        SqliteStore store(":memory:");

        AgentRecord rec;
        rec.agent_id = "agent-003";
        rec.first_seen = 1;
        rec.last_seen = 1;
        store.upsert_agent(rec);

        WHEN("delete_agent is called")
        {
            store.delete_agent("agent-003");

            THEN("get_agent returns nullopt")
            {
                REQUIRE_FALSE(store.get_agent("agent-003").has_value());
            }

            AND_THEN("list_agents no longer includes that agent")
            {
                auto agents = store.list_agents();
                for (auto& a : agents)
                    REQUIRE(a.agent_id != "agent-003");
            }
        }
    }
}

// ── Metrics insertion and retrieval ───────────────────────────────────────────

SCENARIO("Metrics can be inserted and retrieved for an agent")
{
    GIVEN("a store with an agent and one metrics row")
    {
        SqliteStore store(":memory:");

        AgentRecord rec;
        rec.agent_id = "agent-m1";
        rec.first_seen = 1;
        rec.last_seen = 1;
        store.upsert_agent(rec);

        MetricsRow row;
        row.agent_id = "agent-m1";
        row.timestamp_ms = 5000;
        row.metrics_json = R"({"cpu":{"usage_percent":12.5}})";
        store.insert_metrics(row);

        WHEN("get_metrics is called")
        {
            auto rows = store.get_metrics("agent-m1", 10);

            THEN("one row is returned with the correct fields")
            {
                REQUIRE(rows.size() == 1);
                REQUIRE(rows[0].agent_id == "agent-m1");
                REQUIRE(rows[0].timestamp_ms == 5000);
                REQUIRE(rows[0].metrics_json == row.metrics_json);
            }
        }
    }
}

SCENARIO("get_metrics respects the row limit and returns newest rows first")
{
    GIVEN("a store with 5 metrics rows for one agent")
    {
        SqliteStore store(":memory:");

        AgentRecord rec;
        rec.agent_id = "agent-m2";
        rec.first_seen = 1;
        rec.last_seen = 1;
        store.upsert_agent(rec);

        for (int i = 1; i <= 5; ++i)
        {
            MetricsRow row;
            row.agent_id = "agent-m2";
            row.timestamp_ms = i * 1000;
            row.metrics_json = "{}";
            store.insert_metrics(row);
        }

        WHEN("get_metrics is called with limit=2")
        {
            auto rows = store.get_metrics("agent-m2", 2);

            THEN("exactly 2 rows are returned")
            {
                REQUIRE(rows.size() == 2);
            }

            AND_THEN("the rows are ordered newest first")
            {
                REQUIRE(rows[0].timestamp_ms == 5000);
                REQUIRE(rows[1].timestamp_ms == 4000);
            }
        }
    }
}

SCENARIO("latest_metrics returns one row per agent with their most recent timestamp")
{
    GIVEN("two agents each with three metrics rows at different timestamps")
    {
        SqliteStore store(":memory:");

        for (auto id : {"alpha", "beta"})
        {
            AgentRecord r;
            r.agent_id = id;
            r.first_seen = 1;
            r.last_seen = 1;
            store.upsert_agent(r);

            for (int t : {1000, 2000, 3000})
            {
                MetricsRow row;
                row.agent_id = id;
                row.timestamp_ms = t;
                row.metrics_json = "{}";
                store.insert_metrics(row);
            }
        }

        WHEN("latest_metrics is called")
        {
            auto latest = store.latest_metrics();

            THEN("exactly one row is returned per agent")
            {
                REQUIRE(latest.size() == 2);
            }

            AND_THEN("each row has the most recent timestamp")
            {
                for (auto& r : latest)
                    REQUIRE(r.timestamp_ms == 3000);
            }
        }
    }
}
