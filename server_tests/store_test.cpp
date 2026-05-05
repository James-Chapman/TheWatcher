#include "../server/store_sqlite.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

using namespace thewatcher;
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

SCENARIO("Agent collector config is persisted across upserts")
{
    GIVEN("a store with an approved agent")
    {
        SqliteStore store(":memory:");

        AgentRecord rec;
        rec.agent_id = "agent-collector-config";
        rec.approved = true;
        rec.first_seen = 1;
        rec.last_seen = 1;
        store.upsert_agent(rec);

        WHEN("collector configuration is saved")
        {
            CollectorConfig config;
            config.cpu.warning_percent = 80.0;
            config.cpu.degraded_percent = 90.0;
            config.cpu.critical_percent = 95.0;
            config.cpu_readings = 2;
            config.disk_readings = 3;

            DiskMonitorConfig disk;
            disk.mount_point = "/";
            disk.device = "/dev/sda1";
            disk.enabled = true;
            disk.thresholds.warning_percent = 81.0;
            config.disks.push_back(disk);

            NetworkInterfaceConfig network;
            network.interface_name = "eth0";
            network.enabled = false;
            network.thresholds.warning_mbps = 100.0;
            config.networks.push_back(network);

            ProcessWatchConfig process;
            process.name = "TheWatcherAgent.exe";
            process.expected_count = 2;
            config.processes.push_back(process);

            rec.collector_config = config;
            store.set_agent_collector_config(rec.agent_id, config);

            THEN("get_agent returns the stored collector config")
            {
                auto got = store.get_agent(rec.agent_id);
                REQUIRE(got.has_value());
                REQUIRE(got->collector_config.cpu.warning_percent == 80.0);
                REQUIRE(got->collector_config.cpu_readings == 2);
                REQUIRE(got->collector_config.disk_readings == 3);
                REQUIRE(got->collector_config.disks.size() == 1);
                REQUIRE(got->collector_config.disks[0].mount_point == "/");
                REQUIRE(got->collector_config.networks.size() == 1);
                REQUIRE(got->collector_config.networks[0].enabled == false);
                REQUIRE(got->collector_config.processes.size() == 1);
                REQUIRE(got->collector_config.processes[0].expected_count == 2);
            }
        }
    }
}

SCENARIO("Pending indicator readings are persisted and can be cleared")
{
    GIVEN("a store with an approved agent")
    {
        SqliteStore store(":memory:");

        AgentRecord rec;
        rec.agent_id = "agent-pending-status";
        rec.approved = true;
        rec.first_seen = 1;
        rec.last_seen = 1;
        store.upsert_agent(rec);

        WHEN("a pending indicator status is stored")
        {
            store.set_pending_status("agent-pending-status", "cpu", "red", 2);

            THEN("the pending status can be read back")
            {
                auto pending = store.get_pending_status("agent-pending-status", "cpu");
                REQUIRE(pending.has_value());
                REQUIRE(pending->agent_id == "agent-pending-status");
                REQUIRE(pending->indicator == "cpu");
                REQUIRE(pending->target_status == "red");
                REQUIRE(pending->count == 2);
            }

            AND_WHEN("the pending status is cleared")
            {
                store.clear_pending_status("agent-pending-status", "cpu");

                THEN("the pending status is removed")
                {
                    REQUIRE_FALSE(store.get_pending_status("agent-pending-status", "cpu").has_value());
                }
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
        row.metrics_cbor = {0xA1, 0x63, 'c', 'p', 'u', 0x00}; // arbitrary fixture bytes (CBOR map with one entry).
        store.insert_metrics(row);

        WHEN("get_metrics is called")
        {
            auto rows = store.get_metrics("agent-m1", 10);

            THEN("one row is returned with the correct fields")
            {
                REQUIRE(rows.size() == 1);
                REQUIRE(rows[0].agent_id == "agent-m1");
                REQUIRE(rows[0].timestamp_ms == 5000);
                REQUIRE(rows[0].metrics_cbor == row.metrics_cbor);
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
            row.metrics_cbor = {0xA0}; // empty CBOR map.
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
                row.metrics_cbor = {0xA0}; // empty CBOR map.
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

// ── Alert note field ──────────────────────────────────────────────────────────

SCENARIO("Acknowledging an alert stores the operator note alongside the ack metadata")
{
    GIVEN("a store with an agent and an active alert")
    {
        SqliteStore store(":memory:");

        AgentRecord rec;
        rec.agent_id = "agent-note";
        rec.approved = true;
        rec.first_seen = 1;
        rec.last_seen = 1;
        store.upsert_agent(rec);

        AlertRecord alert;
        alert.agent_id = "agent-note";
        alert.indicator = "cpu";
        alert.old_status = "green";
        alert.new_status = "red";
        alert.message = "cpu changed";
        alert.created_at = 100;
        const auto alert_id = store.insert_alert(alert);

        WHEN("the alert is acknowledged with a note")
        {
            store.acknowledge_alert(alert_id, "operator1", 200, "Investigated — transient spike, no action needed");

            THEN("list_alerts returns the stored note and ack metadata")
            {
                auto alerts = store.list_alerts(false);
                REQUIRE(alerts.size() == 1);
                REQUIRE(alerts[0].acknowledged_by == "operator1");
                REQUIRE(alerts[0].acknowledged_at == 200);
                REQUIRE(alerts[0].note == "Investigated — transient spike, no action needed");
            }
        }

        WHEN("the alert is acknowledged without a note")
        {
            store.acknowledge_alert(alert_id, "operator2", 300);

            THEN("the note is empty and ack metadata is still recorded")
            {
                auto alerts = store.list_alerts(false);
                REQUIRE(alerts.size() == 1);
                REQUIRE(alerts[0].acknowledged_by == "operator2");
                REQUIRE(alerts[0].note == "");
            }
        }
    }
}

// ── Alert escalation ──────────────────────────────────────────────────────────

SCENARIO("Unacknowledged alerts older than the cutoff are escalated")
{
    GIVEN("a store with two unacknowledged alerts at different ages")
    {
        SqliteStore store(":memory:");

        AgentRecord rec;
        rec.agent_id = "agent-esc";
        rec.approved = true;
        rec.first_seen = 1;
        rec.last_seen = 1;
        store.upsert_agent(rec);

        AlertRecord old_alert;
        old_alert.agent_id = "agent-esc";
        old_alert.indicator = "cpu";
        old_alert.old_status = "green";
        old_alert.new_status = "red";
        old_alert.created_at = 1000;
        store.insert_alert(old_alert);

        AlertRecord recent_alert;
        recent_alert.agent_id = "agent-esc";
        recent_alert.indicator = "memory";
        recent_alert.old_status = "green";
        recent_alert.new_status = "yellow";
        recent_alert.created_at = 9000;
        store.insert_alert(recent_alert);

        WHEN("escalate_old_alerts is called with a cutoff that only covers the older alert")
        {
            store.escalate_old_alerts(5000, 10000);

            THEN("the old alert has a non-zero escalated_at")
            {
                auto alerts = store.list_alerts(false);
                REQUIRE(alerts.size() == 2);
                auto it_old = std::find_if(alerts.begin(), alerts.end(), [](const AlertRecord& a) { return a.indicator == "cpu"; });
                auto it_recent = std::find_if(alerts.begin(), alerts.end(), [](const AlertRecord& a) { return a.indicator == "memory"; });
                REQUIRE(it_old != alerts.end());
                REQUIRE(it_recent != alerts.end());
                REQUIRE(it_old->escalated_at == 10000);
                REQUIRE(it_recent->escalated_at == 0);
            }
        }
    }
}

// ── Bulk alert operations ─────────────────────────────────────────────────────

SCENARIO("Bulk acknowledging alerts sets ack metadata on all specified alerts")
{
    GIVEN("a store with an agent and three active alerts")
    {
        SqliteStore store(":memory:");

        AgentRecord rec;
        rec.agent_id = "agent-bulk";
        rec.approved = true;
        rec.first_seen = 1;
        rec.last_seen = 1;
        store.upsert_agent(rec);

        std::vector<int64_t> ids;
        for (int i = 0; i < 3; ++i)
        {
            AlertRecord alert;
            alert.agent_id = "agent-bulk";
            alert.indicator = "cpu";
            alert.old_status = "green";
            alert.new_status = "red";
            alert.message = "alert";
            alert.created_at = 100 + i;
            ids.push_back(store.insert_alert(alert));
        }

        WHEN("bulk_acknowledge_alerts is called on the first two")
        {
            store.bulk_acknowledge_alerts({ids[0], ids[1]}, "ops-team", 500, "bulk ack");

            THEN("the first two are acknowledged with the note")
            {
                auto alerts = store.list_alerts(false);
                int acked = 0;
                for (const auto& a : alerts)
                {
                    if (a.acknowledged_at > 0)
                    {
                        REQUIRE(a.acknowledged_by == "ops-team");
                        REQUIRE(a.note == "bulk ack");
                        ++acked;
                    }
                }
                REQUIRE(acked == 2);
            }

            AND_THEN("the third alert remains unacknowledged")
            {
                REQUIRE(store.list_unacknowledged_alerts().size() == 1);
            }
        }
    }
}

// ── Metrics window count ──────────────────────────────────────────────────────

SCENARIO("count_metrics_in_window counts only rows within the time window")
{
    GIVEN("a store with 5 metrics rows for an agent spread over time")
    {
        SqliteStore store(":memory:");

        AgentRecord rec;
        rec.agent_id = "agent-cnt";
        rec.first_seen = 1;
        rec.last_seen = 1;
        store.upsert_agent(rec);

        for (int t : {1000, 2000, 3000, 4000, 5000})
        {
            MetricsRow row;
            row.agent_id = "agent-cnt";
            row.timestamp_ms = t;
            row.metrics_cbor = {0xA0};
            store.insert_metrics(row);
        }

        WHEN("count_metrics_in_window is called for the middle three timestamps")
        {
            auto count = store.count_metrics_in_window("agent-cnt", 2000, 4000);

            THEN("exactly 3 rows are counted")
            {
                REQUIRE(count == 3);
            }
        }
    }
}

// ── Maintenance windows ───────────────────────────────────────────────────────

SCENARIO("Maintenance windows can be created, listed, and deleted")
{
    GIVEN("an empty store")
    {
        SqliteStore store(":memory:");

        WHEN("a maintenance window is created")
        {
            MaintenanceWindowRecord w;
            w.agent_id = "*";
            w.start_ms = 1000;
            w.end_ms = 5000;
            w.reason = "patching";
            w.created_by = "admin";
            w.created_at = 500;
            auto window_id = store.create_maintenance_window(w);

            THEN("list_maintenance_windows includes it")
            {
                auto windows = store.list_maintenance_windows();
                REQUIRE(windows.size() == 1);
                REQUIRE(windows[0].window_id == window_id);
                REQUIRE(windows[0].agent_id == "*");
                REQUIRE(windows[0].reason == "patching");
            }

            AND_WHEN("it is deleted")
            {
                store.delete_maintenance_window(window_id);

                THEN("the list is empty")
                {
                    REQUIRE(store.list_maintenance_windows().empty());
                }
            }
        }
    }
}


SCENARIO("Bulk soft-deleting alerts hides all specified alerts from active queries")
{
    GIVEN("a store with an agent and four alerts")
    {
        SqliteStore store(":memory:");

        AgentRecord rec;
        rec.agent_id = "agent-bulk-del";
        rec.approved = true;
        rec.first_seen = 1;
        rec.last_seen = 1;
        store.upsert_agent(rec);

        std::vector<int64_t> ids;
        for (int i = 0; i < 4; ++i)
        {
            AlertRecord alert;
            alert.agent_id = "agent-bulk-del";
            alert.indicator = "disk";
            alert.old_status = "green";
            alert.new_status = "amber";
            alert.message = "alert";
            alert.created_at = 200 + i;
            ids.push_back(store.insert_alert(alert));
        }

        WHEN("bulk_soft_delete_alerts archives three of the four")
        {
            store.bulk_soft_delete_alerts({ids[0], ids[1], ids[2]}, 999);

            THEN("only one active alert remains")
            {
                REQUIRE(store.list_alerts(false).size() == 1);
            }

            AND_THEN("the historical view includes all four")
            {
                REQUIRE(store.list_alerts(true).size() == 4);
            }
        }
    }
}

// ── Dead-agent alert helpers ──────────────────────────────────────────────────

SCENARIO("get_offline_unalerted_agent_ids returns only offline approved agents with no active Heartbeat alert")
{
    GIVEN("a store with several agents in different states")
    {
        SqliteStore store(":memory:");

        auto make_agent = [&](const std::string& id, bool approved, bool connected, bool maintenance) {
            AgentRecord r;
            r.agent_id = id;
            r.approved = approved;
            r.connected = connected;
            r.maintenance = maintenance;
            r.first_seen = 1;
            r.last_seen = connected ? 9999 : 1; // offline agents have old last_seen
            store.upsert_agent(r);
        };

        make_agent("agent-online",      true,  true,  false); // online — excluded
        make_agent("agent-offline-ok",  true,  false, false); // offline, no alert — should appear
        make_agent("agent-offline-alerted", true, false, false); // offline, has alert — excluded
        make_agent("agent-pending",     false, false, false); // not approved — excluded
        make_agent("agent-maintenance", true,  false, true);  // maintenance — excluded

        AlertRecord alert;
        alert.agent_id = "agent-offline-alerted";
        alert.indicator = "Heartbeat";
        alert.old_status = "green";
        alert.new_status = "red";
        alert.message = "offline";
        alert.created_at = 100;
        store.insert_alert(alert);

        WHEN("get_offline_unalerted_agent_ids is called")
        {
            auto ids = store.get_offline_unalerted_agent_ids();

            THEN("only the unalerted offline approved non-maintenance agent is returned")
            {
                REQUIRE(ids.size() == 1);
                REQUIRE(ids[0] == "agent-offline-ok");
            }
        }
    }
}

SCENARIO("archive_heartbeat_alerts_for_agent removes active Heartbeat alerts for that agent only")
{
    GIVEN("two agents each with an active Heartbeat alert")
    {
        SqliteStore store(":memory:");

        for (auto id : {"agent-A", "agent-B"})
        {
            AgentRecord r;
            r.agent_id = id;
            r.approved = true;
            r.first_seen = 1;
            r.last_seen = 1;
            store.upsert_agent(r);

            AlertRecord alert;
            alert.agent_id = id;
            alert.indicator = "Heartbeat";
            alert.old_status = "green";
            alert.new_status = "red";
            alert.message = "offline";
            alert.created_at = 100;
            store.insert_alert(alert);
        }

        WHEN("archive_heartbeat_alerts_for_agent is called for agent-A")
        {
            store.archive_heartbeat_alerts_for_agent("agent-A", 500);

            THEN("agent-A has no active alerts")
            {
                auto active = store.list_alerts(false);
                for (const auto& a : active)
                    REQUIRE(a.agent_id != "agent-A");
            }

            AND_THEN("agent-B still has its active alert")
            {
                auto active = store.list_alerts(false);
                bool found = false;
                for (const auto& a : active)
                    found = found || (a.agent_id == "agent-B");
                REQUIRE(found);
            }

            AND_THEN("the archived alert for agent-A is visible in the historical view")
            {
                auto all = store.list_alerts(true);
                bool found = false;
                for (const auto& a : all)
                    found = found || (a.agent_id == "agent-A" && a.deleted_at == 500);
                REQUIRE(found);
            }
        }
    }
}

SCENARIO("active_maintenance_windows only returns windows that span the current time")
{
    GIVEN("a store with windows in the past, present, and future")
    {
        SqliteStore store(":memory:");

        MaintenanceWindowRecord past;
        past.agent_id = "*";
        past.start_ms = 100;
        past.end_ms = 500;
        past.created_at = 50;
        store.create_maintenance_window(past);

        MaintenanceWindowRecord active;
        active.agent_id = "agent-x";
        active.start_ms = 900;
        active.end_ms = 2000;
        active.created_at = 800;
        store.create_maintenance_window(active);

        MaintenanceWindowRecord future;
        future.agent_id = "*";
        future.start_ms = 5000;
        future.end_ms = 9000;
        future.created_at = 800;
        store.create_maintenance_window(future);

        WHEN("active_maintenance_windows is queried at time 1000")
        {
            auto windows = store.active_maintenance_windows(1000);

            THEN("only the currently active window is returned")
            {
                REQUIRE(windows.size() == 1);
                REQUIRE(windows[0].agent_id == "agent-x");
            }
        }
    }
}

SCENARIO("set_agent_description stores a description against a known agent")
{
    GIVEN("a store with an approved agent")
    {
        SqliteStore store(":memory:");

        AgentRecord agent;
        agent.agent_id = "desc-agent";
        agent.hostname = "host";
        agent.approved = true;
        store.upsert_agent(agent);

        WHEN("set_agent_description is called with a non-empty string")
        {
            store.set_agent_description("desc-agent", "Primary database server");

            THEN("the description is returned when the agent is listed")
            {
                auto agents = store.list_agents();
                auto it = std::find_if(agents.begin(), agents.end(),
                    [](const AgentRecord& r) { return r.agent_id == "desc-agent"; });
                REQUIRE(it != agents.end());
                REQUIRE(it->description == "Primary database server");
            }
        }

        WHEN("set_agent_description is called twice")
        {
            store.set_agent_description("desc-agent", "first");
            store.set_agent_description("desc-agent", "second");

            THEN("the latest value is stored")
            {
                auto agents = store.list_agents();
                auto it = std::find_if(agents.begin(), agents.end(),
                    [](const AgentRecord& r) { return r.agent_id == "desc-agent"; });
                REQUIRE(it != agents.end());
                REQUIRE(it->description == "second");
            }
        }
    }
}

SCENARIO("disable_user and enable_user toggle the disabled flag")
{
    GIVEN("a store with a non-built-in user")
    {
        SqliteStore store(":memory:");

        store.create_user("operator1", "hash", "operator");

        auto users = store.list_users();
        auto it = std::find_if(users.begin(), users.end(),
            [](const UserRecord& r) { return r.username == "operator1"; });
        REQUIRE(it != users.end());
        int64_t uid = it->user_id;

        WHEN("the user is disabled")
        {
            store.disable_user(uid);

            THEN("list_users shows the user as disabled")
            {
                auto updated = store.list_users();
                auto u = std::find_if(updated.begin(), updated.end(),
                    [](const UserRecord& r) { return r.username == "operator1"; });
                REQUIRE(u != updated.end());
                REQUIRE(u->disabled == true);
            }
        }

        WHEN("a disabled user is re-enabled")
        {
            store.disable_user(uid);
            store.enable_user(uid);

            THEN("list_users shows the user as active again")
            {
                auto updated = store.list_users();
                auto u = std::find_if(updated.begin(), updated.end(),
                    [](const UserRecord& r) { return r.username == "operator1"; });
                REQUIRE(u != updated.end());
                REQUIRE(u->disabled == false);
            }
        }
    }
}

SCENARIO("delete_user removes a non-built-in user")
{
    GIVEN("a store with two users")
    {
        SqliteStore store(":memory:");

        store.create_user("to-delete", "h1", "viewer");
        store.create_user("to-keep", "h2", "viewer");

        auto before = store.list_users();
        auto it = std::find_if(before.begin(), before.end(),
            [](const UserRecord& r) { return r.username == "to-delete"; });
        REQUIRE(it != before.end());
        int64_t del_uid = it->user_id;

        WHEN("delete_user is called for the first user")
        {
            store.delete_user(del_uid);

            THEN("the user no longer appears in list_users")
            {
                auto after = store.list_users();
                auto gone = std::find_if(after.begin(), after.end(),
                    [](const UserRecord& r) { return r.username == "to-delete"; });
                REQUIRE(gone == after.end());

                auto kept = std::find_if(after.begin(), after.end(),
                    [](const UserRecord& r) { return r.username == "to-keep"; });
                REQUIRE(kept != after.end());
            }
        }
    }
}

SCENARIO("update_user_password changes the stored credential hash")
{
    GIVEN("a store with a user")
    {
        SqliteStore store(":memory:");

        store.create_user("pwuser", "old-hash", "viewer");

        auto users = store.list_users();
        auto it = std::find_if(users.begin(), users.end(),
            [](const UserRecord& r) { return r.username == "pwuser"; });
        REQUIRE(it != users.end());
        int64_t uid = it->user_id;

        WHEN("update_user_password is called with a new hash")
        {
            store.update_user_password(uid, "new-hash");

            THEN("the stored password_hash reflects the new value")
            {
                auto updated = store.list_users();
                auto u = std::find_if(updated.begin(), updated.end(),
                    [](const UserRecord& r) { return r.username == "pwuser"; });
                REQUIRE(u != updated.end());
                REQUIRE(u->password_hash == "new-hash");
                REQUIRE(u->password_hash != "old-hash");
            }
        }
    }
}

SCENARIO("list_approved_agents and list_pending_agents filter by enrollment state")
{
    GIVEN("a store with approved, pending, and rejected agents")
    {
        SqliteStore store(":memory:");

        AgentRecord pending_agent;
        pending_agent.agent_id = "pending-1";
        pending_agent.hostname = "pending-host";
        pending_agent.approved = false;
        pending_agent.rejected = false;
        store.upsert_agent(pending_agent);

        AgentRecord approved_agent;
        approved_agent.agent_id = "approved-1";
        approved_agent.hostname = "approved-host";
        approved_agent.approved = false;
        store.upsert_agent(approved_agent);
        store.approve_agent("approved-1");

        AgentRecord rejected_agent;
        rejected_agent.agent_id = "rejected-1";
        rejected_agent.hostname = "rejected-host";
        rejected_agent.approved = false;
        rejected_agent.rejected = false;
        store.upsert_agent(rejected_agent);
        store.reject_agent("rejected-1");

        WHEN("list_approved_agents is called")
        {
            auto approved = store.list_approved_agents();

            THEN("only the approved agent is returned")
            {
                REQUIRE(approved.size() == 1);
                REQUIRE(approved[0].agent_id == "approved-1");
            }
        }

        WHEN("list_pending_agents is called")
        {
            auto pending = store.list_pending_agents();

            THEN("only the unapproved, unrejected agent is returned")
            {
                REQUIRE(pending.size() == 1);
                REQUIRE(pending[0].agent_id == "pending-1");
            }
        }
    }
}

SCENARIO("session lifecycle: create, retrieve by token, and expire")
{
    GIVEN("an empty store")
    {
        SqliteStore store(":memory:");

        WHEN("a session is created and retrieved before expiry")
        {
            SessionRecord session;
            session.token = "tok-abc";
            session.user_id = 1;
            session.username = "thewatcher";
            session.role = "admin";
            session.created_at = 1000;
            session.expires_at = 9999;
            store.create_session(session);

            auto result = store.get_session("tok-abc", 5000);

            THEN("the session record is returned with correct fields")
            {
                REQUIRE(result.has_value());
                REQUIRE(result->username == "thewatcher");
                REQUIRE(result->role == "admin");
                REQUIRE(result->expires_at == 9999);
            }
        }

        WHEN("a session is retrieved after its expiry time")
        {
            SessionRecord session;
            session.token = "tok-expired";
            session.user_id = 1;
            session.username = "thewatcher";
            session.role = "admin";
            session.created_at = 1000;
            session.expires_at = 2000;
            store.create_session(session);

            auto result = store.get_session("tok-expired", 5000);

            THEN("no session is returned")
            {
                REQUIRE_FALSE(result.has_value());
            }
        }

        WHEN("a session is deleted and then looked up")
        {
            SessionRecord session;
            session.token = "tok-delete";
            session.user_id = 1;
            session.username = "thewatcher";
            session.role = "admin";
            session.created_at = 1000;
            session.expires_at = 99999;
            store.create_session(session);
            store.delete_session("tok-delete");

            auto result = store.get_session("tok-delete", 5000);

            THEN("no session is returned")
            {
                REQUIRE_FALSE(result.has_value());
            }
        }

        WHEN("an unknown token is looked up")
        {
            auto result = store.get_session("nonexistent-token", 1000);

            THEN("no session is returned")
            {
                REQUIRE_FALSE(result.has_value());
            }
        }
    }
}

SCENARIO("settings key-value store persists values with fallback for missing keys")
{
    GIVEN("an empty store")
    {
        SqliteStore store(":memory:");

        WHEN("a missing key is fetched without a fallback")
        {
            auto value = store.get_setting("no_such_key", "");

            THEN("an empty string is returned")
            {
                REQUIRE(value.empty());
            }
        }

        WHEN("a missing key is fetched with an explicit fallback")
        {
            auto value = store.get_setting("no_such_key", "default-value");

            THEN("the fallback is returned")
            {
                REQUIRE(value == "default-value");
            }
        }

        WHEN("a key is set and then retrieved")
        {
            store.set_setting("notifications.webhook_url", "https://hooks.example.com/alert");
            auto value = store.get_setting("notifications.webhook_url", "");

            THEN("the stored value is returned")
            {
                REQUIRE(value == "https://hooks.example.com/alert");
            }
        }

        WHEN("a key is overwritten")
        {
            store.set_setting("offline_after_seconds", "120");
            store.set_setting("offline_after_seconds", "300");

            THEN("the latest value is returned")
            {
                REQUIRE(store.get_setting("offline_after_seconds", "") == "300");
            }
        }
    }
}

SCENARIO("group management: groups can be created and listed")
{
    GIVEN("a fresh store with only the built-in Admins group")
    {
        SqliteStore store(":memory:");
        auto initial = store.list_groups();
        REQUIRE(initial.size() == 1);
        REQUIRE(initial[0].name == "Admins");
        REQUIRE(initial[0].built_in == true);

        WHEN("a new group is created")
        {
            auto new_id = store.create_group("Production");

            THEN("it appears in list_groups and the returned id is positive")
            {
                REQUIRE(new_id > 0);
                auto groups = store.list_groups();
                REQUIRE(groups.size() == 2);
                bool found = false;
                for (const auto& g : groups)
                    found = found || (g.name == "Production" && !g.built_in);
                REQUIRE(found);
            }
        }

        WHEN("multiple groups are created")
        {
            store.create_group("Group A");
            store.create_group("Group B");

            THEN("list_groups returns all of them")
            {
                REQUIRE(store.list_groups().size() == 3); // Admins + A + B
            }
        }
    }
}

SCENARIO("agent group membership can be set and retrieved")
{
    GIVEN("a store with an agent and two groups")
    {
        SqliteStore store(":memory:");

        AgentRecord agent;
        agent.agent_id = "grp-agent";
        agent.hostname = "grp-host";
        agent.approved = true;
        store.upsert_agent(agent);

        int64_t gid1 = store.create_group("Production");
        int64_t gid2 = store.create_group("Databases");

        WHEN("the agent is assigned to both groups")
        {
            store.set_agent_groups("grp-agent", {gid1, gid2});
            auto groups = store.get_agent_groups("grp-agent");

            THEN("both group ids are returned")
            {
                REQUIRE(groups.size() == 2);
                bool has1 = std::find(groups.begin(), groups.end(), gid1) != groups.end();
                bool has2 = std::find(groups.begin(), groups.end(), gid2) != groups.end();
                REQUIRE(has1);
                REQUIRE(has2);
            }
        }

        WHEN("the agent's group assignment is replaced")
        {
            store.set_agent_groups("grp-agent", {gid1, gid2});
            store.set_agent_groups("grp-agent", {gid2});
            auto groups = store.get_agent_groups("grp-agent");

            THEN("only the new group is retained")
            {
                REQUIRE(groups.size() == 1);
                REQUIRE(groups[0] == gid2);
            }
        }

        WHEN("the agent is assigned to no groups")
        {
            store.set_agent_groups("grp-agent", {gid1});
            store.set_agent_groups("grp-agent", {});

            THEN("get_agent_groups returns an empty list")
            {
                REQUIRE(store.get_agent_groups("grp-agent").empty());
            }
        }
    }
}

SCENARIO("user group membership can be set and retrieved")
{
    GIVEN("a store with a user and two groups")
    {
        SqliteStore store(":memory:");

        int64_t uid = store.create_user("viewer1", "hash", "viewer");
        int64_t gid1 = store.create_group("TeamA");
        int64_t gid2 = store.create_group("TeamB");

        WHEN("the user is assigned to a group")
        {
            store.set_user_groups(uid, {gid1});
            auto groups = store.get_user_groups(uid);

            THEN("that group is returned")
            {
                REQUIRE(groups.size() == 1);
                REQUIRE(groups[0] == gid1);
            }
        }

        WHEN("the user's group assignment is replaced")
        {
            store.set_user_groups(uid, {gid1});
            store.set_user_groups(uid, {gid2});
            auto groups = store.get_user_groups(uid);

            THEN("only the new group is retained")
            {
                REQUIRE(groups.size() == 1);
                REQUIRE(groups[0] == gid2);
            }
        }
    }
}

SCENARIO("status history records transitions and the latest entry is queryable per indicator")
{
    GIVEN("a store with an approved agent")
    {
        SqliteStore store(":memory:");

        AgentRecord agent;
        agent.agent_id = "hist-agent";
        agent.hostname = "hist-host";
        agent.approved = true;
        store.upsert_agent(agent);

        WHEN("two status transitions are inserted for the cpu indicator")
        {
            StatusHistoryRow row1;
            row1.agent_id = "hist-agent";
            row1.indicator = "cpu";
            row1.old_status = "green";
            row1.new_status = "yellow";
            row1.message = "cpu changed";
            row1.created_at = 1000;
            store.insert_status_history(row1);

            StatusHistoryRow row2;
            row2.agent_id = "hist-agent";
            row2.indicator = "cpu";
            row2.old_status = "yellow";
            row2.new_status = "red";
            row2.message = "cpu worsened";
            row2.created_at = 2000;
            store.insert_status_history(row2);

            THEN("latest_status_for_indicator returns the most recent row")
            {
                auto latest = store.latest_status_for_indicator("hist-agent", "cpu");
                REQUIRE(latest.has_value());
                REQUIRE(latest->new_status == "red");
                REQUIRE(latest->old_status == "yellow");
                REQUIRE(latest->created_at == 2000);
            }

            AND_THEN("list_status_history returns both rows with the most recent first")
            {
                auto history = store.list_status_history("hist-agent", 10);
                REQUIRE(history.size() == 2);
                REQUIRE(history[0].new_status == "red");
                REQUIRE(history[1].new_status == "yellow");
            }
        }

        WHEN("a transition is inserted for a different indicator")
        {
            StatusHistoryRow cpu_row;
            cpu_row.agent_id = "hist-agent";
            cpu_row.indicator = "cpu";
            cpu_row.old_status = "green";
            cpu_row.new_status = "amber";
            cpu_row.created_at = 1000;
            store.insert_status_history(cpu_row);

            StatusHistoryRow mem_row;
            mem_row.agent_id = "hist-agent";
            mem_row.indicator = "memory";
            mem_row.old_status = "green";
            mem_row.new_status = "yellow";
            mem_row.created_at = 2000;
            store.insert_status_history(mem_row);

            THEN("latest_status_for_indicator is independent per indicator")
            {
                auto cpu_latest = store.latest_status_for_indicator("hist-agent", "cpu");
                REQUIRE(cpu_latest.has_value());
                REQUIRE(cpu_latest->new_status == "amber");

                auto mem_latest = store.latest_status_for_indicator("hist-agent", "memory");
                REQUIRE(mem_latest.has_value());
                REQUIRE(mem_latest->new_status == "yellow");
            }
        }

        WHEN("latest_status_for_indicator is called for an unknown indicator")
        {
            THEN("nullopt is returned")
            {
                REQUIRE_FALSE(store.latest_status_for_indicator("hist-agent", "unknown").has_value());
            }
        }
    }
}

SCENARIO("pending status accumulates consecutive readings and clears on recovery")
{
    GIVEN("a store with an approved agent")
    {
        SqliteStore store(":memory:");

        AgentRecord agent;
        agent.agent_id = "pending-agent";
        agent.hostname = "pending-host";
        agent.approved = true;
        store.upsert_agent(agent);

        WHEN("a pending status is set for the first reading")
        {
            store.set_pending_status("pending-agent", "cpu", "red", 1);

            THEN("get_pending_status returns the stored record")
            {
                auto pending = store.get_pending_status("pending-agent", "cpu");
                REQUIRE(pending.has_value());
                REQUIRE(pending->target_status == "red");
                REQUIRE(pending->count == 1);
            }
        }

        WHEN("the pending count is incremented across two calls")
        {
            store.set_pending_status("pending-agent", "cpu", "red", 1);
            store.set_pending_status("pending-agent", "cpu", "red", 2);

            THEN("the count is updated to the latest value")
            {
                auto pending = store.get_pending_status("pending-agent", "cpu");
                REQUIRE(pending.has_value());
                REQUIRE(pending->count == 2);
            }
        }

        WHEN("the pending status is cleared")
        {
            store.set_pending_status("pending-agent", "cpu", "red", 1);
            store.clear_pending_status("pending-agent", "cpu");

            THEN("get_pending_status returns nullopt")
            {
                REQUIRE_FALSE(store.get_pending_status("pending-agent", "cpu").has_value());
            }
        }

        WHEN("get_pending_status is called when no record exists")
        {
            THEN("nullopt is returned")
            {
                REQUIRE_FALSE(store.get_pending_status("pending-agent", "cpu").has_value());
            }
        }
    }
}

SCENARIO("clear_active_alerts_for_agent archives only that agent's alerts")
{
    GIVEN("a store with active alerts for two different agents")
    {
        SqliteStore store(":memory:");

        AgentRecord a1;
        a1.agent_id = "agent-one";
        a1.hostname = "host-one";
        a1.approved = true;
        store.upsert_agent(a1);

        AgentRecord a2;
        a2.agent_id = "agent-two";
        a2.hostname = "host-two";
        a2.approved = true;
        store.upsert_agent(a2);

        AlertRecord alert1;
        alert1.agent_id = "agent-one";
        alert1.indicator = "cpu";
        alert1.old_status = "green";
        alert1.new_status = "red";
        alert1.created_at = 1000;
        store.insert_alert(alert1);

        AlertRecord alert2;
        alert2.agent_id = "agent-two";
        alert2.indicator = "memory";
        alert2.old_status = "green";
        alert2.new_status = "amber";
        alert2.created_at = 2000;
        store.insert_alert(alert2);

        WHEN("clear_active_alerts_for_agent is called for agent-one")
        {
            store.clear_active_alerts_for_agent("agent-one", 5000);

            THEN("agent-one's alert is archived but agent-two's alert remains active")
            {
                auto unacked = store.list_unacknowledged_alerts();
                REQUIRE(unacked.size() == 1);
                REQUIRE(unacked[0].agent_id == "agent-two");
            }

            AND_THEN("the cleared alert is still retrievable and shows as acknowledged by maintenance")
            {
                auto all = store.list_alerts(false);
                bool found_cleared = false;
                for (const auto& a : all)
                    found_cleared = found_cleared || (a.agent_id == "agent-one" && a.acknowledged_at > 0);
                REQUIRE(found_cleared);
            }
        }
    }
}

// ── Silence rules ─────────────────────────────────────────────────────────────

SCENARIO("Silence rules can be created, listed, and deleted")
{
    SqliteStore store(":memory:");
    const int64_t now = 1'000'000;

    GIVEN("an empty silences table")
    {
        THEN("list_silences returns empty")
        {
            REQUIRE(store.list_silences().empty());
        }

        WHEN("a global silence rule is created")
        {
            SilenceRecord rec;
            rec.agent_id   = "*";
            rec.indicator  = "*";
            rec.reason     = "planned maintenance";
            rec.until_ms   = now + 3'600'000;
            rec.created_by = "admin";
            rec.created_at = now;
            const auto id  = store.create_silence(rec);

            THEN("the rule is returned by list_silences")
            {
                auto list = store.list_silences();
                REQUIRE(list.size() == 1);
                REQUIRE(list[0].silence_id == id);
                REQUIRE(list[0].agent_id == "*");
                REQUIRE(list[0].indicator == "*");
                REQUIRE(list[0].reason == "planned maintenance");
                REQUIRE(list[0].until_ms == now + 3'600'000);
                REQUIRE(list[0].created_by == "admin");
            }

            AND_THEN("is_silenced returns true for any agent/indicator during the window")
            {
                REQUIRE(store.is_silenced("agent-x", "cpu", now + 1000));
                REQUIRE(store.is_silenced("agent-y", "memory", now + 1000));
            }

            AND_THEN("is_silenced returns false after the window expires")
            {
                REQUIRE_FALSE(store.is_silenced("agent-x", "cpu", now + 3'600'001));
            }

            AND_THEN("deleting the rule removes it")
            {
                store.delete_silence(id);
                REQUIRE(store.list_silences().empty());
                REQUIRE_FALSE(store.is_silenced("agent-x", "cpu", now + 1000));
            }
        }
    }
}

SCENARIO("Silence rules support wildcard and specific matching")
{
    SqliteStore store(":memory:");
    const int64_t now = 2'000'000;
    const int64_t future = now + 3'600'000;

    GIVEN("a silence rule scoped to a specific agent")
    {
        SilenceRecord rec;
        rec.agent_id   = "agent-alpha";
        rec.indicator  = "*";
        rec.reason     = "agent-specific";
        rec.until_ms   = future;
        rec.created_by = "op";
        rec.created_at = now;
        store.create_silence(rec);

        THEN("is_silenced is true for that agent")
        {
            REQUIRE(store.is_silenced("agent-alpha", "cpu", now + 1000));
        }

        AND_THEN("is_silenced is false for a different agent")
        {
            REQUIRE_FALSE(store.is_silenced("agent-beta", "cpu", now + 1000));
        }
    }

    GIVEN("a silence rule scoped to a specific indicator")
    {
        SilenceRecord rec;
        rec.agent_id   = "*";
        rec.indicator  = "memory";
        rec.reason     = "memory spike expected";
        rec.until_ms   = future;
        rec.created_by = "op";
        rec.created_at = now;
        store.create_silence(rec);

        THEN("is_silenced is true for that indicator on any agent")
        {
            REQUIRE(store.is_silenced("agent-x", "memory", now + 1000));
        }

        AND_THEN("is_silenced is false for a different indicator")
        {
            REQUIRE_FALSE(store.is_silenced("agent-x", "cpu", now + 1000));
        }
    }
}

// ── Metrics pruning ───────────────────────────────────────────────────────────

SCENARIO("prune_metrics_before removes old metrics rows")
{
    SqliteStore store(":memory:");

    GIVEN("three metrics rows at different timestamps")
    {
        AgentRecord agent;
        agent.agent_id = "agent-prune";
        agent.hostname = "host";
        agent.approved = true;
        store.upsert_agent(agent);

        MetricsRow r1;
        r1.agent_id    = "agent-prune";
        r1.timestamp_ms = 1000;
        r1.metrics_cbor = {0x01};
        store.insert_metrics(r1);

        MetricsRow r2;
        r2.agent_id    = "agent-prune";
        r2.timestamp_ms = 2000;
        r2.metrics_cbor = {0x02};
        store.insert_metrics(r2);

        MetricsRow r3;
        r3.agent_id    = "agent-prune";
        r3.timestamp_ms = 3000;
        r3.metrics_cbor = {0x03};
        store.insert_metrics(r3);

        WHEN("pruning with cutoff=2500 (removes rows <=2499)")
        {
            store.prune_metrics_before(2500);

            THEN("only the row at timestamp 3000 remains")
            {
                auto rows = store.get_metrics("agent-prune", 100);
                REQUIRE(rows.size() == 1);
                REQUIRE(rows[0].timestamp_ms == 3000);
            }
        }

        WHEN("pruning with cutoff=0 removes nothing")
        {
            store.prune_metrics_before(0);

            THEN("all three rows remain")
            {
                REQUIRE(store.get_metrics("agent-prune", 100).size() == 3);
            }
        }

        WHEN("pruning with cutoff=4000 removes everything")
        {
            store.prune_metrics_before(4000);

            THEN("no rows remain")
            {
                REQUIRE(store.get_metrics("agent-prune", 100).empty());
            }
        }
    }
}
