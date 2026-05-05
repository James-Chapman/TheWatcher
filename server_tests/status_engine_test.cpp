#include "../server/status_engine.hpp"
#include "../server/store_sqlite.hpp"

#include "common/metrics.hpp"
#include "common/protocol.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace thewatcher;
using namespace thewatcher::server;

namespace
{
SystemMetrics metrics_with_cpu(double cpu_percent)
{
    SystemMetrics metrics;
    metrics.cpu.usage_percent = cpu_percent;
    metrics.memory.usage_percent = 10.0;
    metrics.hostname = "status-host";
    metrics.platform = "linux";
    metrics.uptime_seconds = 100.0;

    DiskMetrics disk;
    disk.device = "disk0";
    disk.mount_point = "/";
    disk.filesystem = "ext4";
    disk.usage_percent = 10.0;
    metrics.disks.push_back(disk);

    TemperatureMetrics temperature;
    temperature.sensor_name = "cpu";
    temperature.temperature_celsius = 30.0;
    metrics.temperatures.push_back(temperature);

    ProcessInfo process;
    process.pid = 1;
    process.name = "init";
    process.cpu_percent = 1.0;
    metrics.top_processes.push_back(process);

    NetworkMetrics network;
    network.interface_name = "eth0";
    network.is_up = true;
    metrics.networks.push_back(network);

    return metrics;
}

SystemMetrics metrics_with_processes(std::vector<std::string> process_names)
{
    auto metrics = metrics_with_cpu(10.0);
    metrics.top_processes.clear();
    uint32_t pid = 100;
    for (const auto& name : process_names)
    {
        ProcessInfo process;
        process.pid = pid++;
        process.name = name;
        process.cpu_percent = 1.0;
        metrics.top_processes.push_back(process);
    }
    return metrics;
}

void insert_metrics(SqliteStore& store, const std::string& agent_id, int64_t timestamp_ms, const SystemMetrics& metrics)
{
    store.insert_metrics({agent_id, timestamp_ms, thewatcher::proto::pack(metrics)});
}

void insert_approved_agent(SqliteStore& store, const std::string& agent_id)
{
    AgentRecord agent;
    agent.agent_id = agent_id;
    agent.hostname = "status-host";
    agent.platform = "linux";
    agent.approved = true;
    agent.first_seen = 1;
    agent.last_seen = 1;
    store.upsert_agent(agent);
}
} // namespace

SCENARIO("Status engine records transitions and alerts only on confirmed worsening changes")
{
    GIVEN("an approved agent with baseline metrics")
    {
        SqliteStore store(":memory:");
        insert_approved_agent(store, "agent-status");
        StatusEngine engine(store);

        auto baseline = metrics_with_cpu(10.0);
        insert_metrics(store, "agent-status", 1000, baseline);
        engine.evaluate_metrics("agent-status", baseline, 1000);

        WHEN("the CPU indicator crosses the absolute degraded threshold")
        {
            auto high = metrics_with_cpu(91.0);
            insert_metrics(store, "agent-status", 2000, high);
            engine.evaluate_metrics("agent-status", high, 2000);

            THEN("an alert is generated for the worsening transition")
            {
                auto alerts = store.list_unacknowledged_alerts();
                REQUIRE(alerts.size() == 1);
                REQUIRE(alerts[0].agent_id == "agent-status");
                REQUIRE(alerts[0].indicator == "cpu");
                REQUIRE(alerts[0].old_status == "green");
                REQUIRE(alerts[0].new_status == "amber");
            }

            AND_WHEN("the CPU indicator recovers")
            {
                auto recovered = metrics_with_cpu(10.0);
                insert_metrics(store, "agent-status", 3000, recovered);
                engine.evaluate_metrics("agent-status", recovered, 3000);

                THEN("the recovery is stored in history without creating another alert")
                {
                    auto alerts = store.list_alerts(false);
                    REQUIRE(alerts.size() == 1);

                    auto history = store.list_status_history("agent-status", 20);
                    bool saw_recovery = false;
                    for (const auto& row : history)
                        saw_recovery = saw_recovery || (row.indicator == "cpu" && row.old_status == "amber" &&
                                                        row.new_status == "green");
                    REQUIRE(saw_recovery);
                }
            }
        }
    }
}

SCENARIO("Status engine confirms numeric collector alerts after configured consecutive readings")
{
    GIVEN("an approved agent requiring two CPU readings before changing state")
    {
        SqliteStore store(":memory:");
        insert_approved_agent(store, "agent-confirmed");

        auto agent = store.get_agent("agent-confirmed");
        REQUIRE(agent.has_value());
        agent->collector_config.cpu_readings = 2;
        store.set_agent_collector_config(agent->agent_id, agent->collector_config);

        StatusEngine engine(store);

        WHEN("only one high CPU reading is received")
        {
            auto baseline = metrics_with_cpu(10.0);
            insert_metrics(store, "agent-confirmed", 1000, baseline);
            engine.evaluate_metrics("agent-confirmed", baseline, 1000);

            auto elevated = metrics_with_cpu(96.0);
            insert_metrics(store, "agent-confirmed", 2000, elevated);
            engine.evaluate_metrics("agent-confirmed", elevated, 2000);

            THEN("the last committed status stays green and the pending count is stored")
            {
                auto status = store.latest_status_for_indicator("agent-confirmed", "cpu");
                REQUIRE(status.has_value());
                REQUIRE(status->new_status == "green");
                auto pending = store.get_pending_status("agent-confirmed", "cpu");
                REQUIRE(pending.has_value());
                REQUIRE(pending->target_status == "red");
                REQUIRE(pending->count == 1);
            }
        }

        AND_WHEN("two consecutive high CPU readings are received")
        {
            auto baseline = metrics_with_cpu(10.0);
            insert_metrics(store, "agent-confirmed", 1000, baseline);
            engine.evaluate_metrics("agent-confirmed", baseline, 1000);

            auto high = metrics_with_cpu(96.0);
            insert_metrics(store, "agent-confirmed", 2000, high);
            engine.evaluate_metrics("agent-confirmed", high, 2000);
            insert_metrics(store, "agent-confirmed", 3000, high);
            engine.evaluate_metrics("agent-confirmed", high, 3000);

            THEN("the red status is committed and an alert is generated")
            {
                auto status = store.latest_status_for_indicator("agent-confirmed", "cpu");
                REQUIRE(status.has_value());
                REQUIRE(status->new_status == "red");
                REQUIRE_FALSE(store.get_pending_status("agent-confirmed", "cpu").has_value());
                REQUIRE(store.list_unacknowledged_alerts().size() == 1);
            }
        }
    }
}

SCENARIO("Status engine uses disk and network collector configuration")
{
    GIVEN("an approved agent with configured disk and network checks")
    {
        SqliteStore store(":memory:");
        insert_approved_agent(store, "agent-items");

        auto agent = store.get_agent("agent-items");
        REQUIRE(agent.has_value());
        DiskMonitorConfig disk_config;
        disk_config.mount_point = "/data";
        disk_config.device = "/dev/sdb1";
        agent->collector_config.disks.push_back(disk_config);
        NetworkInterfaceConfig network_config;
        network_config.interface_name = "eth0";
        agent->collector_config.networks.push_back(network_config);
        store.set_agent_collector_config(agent->agent_id, agent->collector_config);

        auto metrics = metrics_with_cpu(10.0);
        metrics.disks[0].mount_point = "/data";
        metrics.disks[0].device = "/dev/sdb1";
        metrics.disks[0].usage_percent = 96.0;
        metrics.networks[0].interface_name = "eth0";
        metrics.networks[0].bytes_recv_per_sec = 20'000'000;
        metrics.networks[0].bytes_sent_per_sec = 20'000'000;

        WHEN("metrics cross disk percent and interface Mbps thresholds")
        {
            StatusEngine engine(store);
            insert_metrics(store, "agent-items", 1000, metrics);
            engine.evaluate_metrics("agent-items", metrics, 1000);

            THEN("per-item status history and alerts identify the disk and interface")
            {
                auto disk_status = store.latest_status_for_indicator("agent-items", "disk:/data");
                REQUIRE(disk_status.has_value());
                REQUIRE(disk_status->new_status == "red");
                REQUIRE(disk_status->message.find("/data (/dev/sdb1)") != std::string::npos);

                auto network_status = store.latest_status_for_indicator("agent-items", "network:eth0");
                REQUIRE(network_status.has_value());
                REQUIRE(network_status->new_status == "red");
                REQUIRE(network_status->message.find("eth0") != std::string::npos);
            }
        }
    }
}

SCENARIO("Status engine escalates missing watched processes by consecutive failed readings")
{
    GIVEN("an approved agent watching an exact process name")
    {
        SqliteStore store(":memory:");
        insert_approved_agent(store, "agent-process");

        auto agent = store.get_agent("agent-process");
        REQUIRE(agent.has_value());
        ProcessWatchConfig watch;
        watch.name = "TheWatcherAgent.exe";
        watch.expected_count = 1;
        agent->collector_config.processes.push_back(watch);
        store.set_agent_collector_config(agent->agent_id, agent->collector_config);

        StatusEngine engine(store);

        WHEN("the watched process is missing three times")
        {
            for (int i = 1; i <= 3; ++i)
            {
                auto metrics = metrics_with_processes({"other.exe"});
                insert_metrics(store, "agent-process", i * 1000, metrics);
                engine.evaluate_metrics("agent-process", metrics, i * 1000);
            }

            THEN("the process status reaches red and the alert message names the missing process")
            {
                auto status = store.latest_status_for_indicator("agent-process", "process:TheWatcherAgent.exe");
                REQUIRE(status.has_value());
                REQUIRE(status->new_status == "red");

                auto alerts = store.list_unacknowledged_alerts();
                REQUIRE_FALSE(alerts.empty());
                REQUIRE(alerts.back().message.find("TheWatcherAgent.exe") != std::string::npos);
            }
        }

        AND_WHEN("the watched process recovers")
        {
            auto missing = metrics_with_processes({"other.exe"});
            insert_metrics(store, "agent-process", 1000, missing);
            engine.evaluate_metrics("agent-process", missing, 1000);

            auto recovered = metrics_with_processes({"TheWatcherAgent.exe"});
            insert_metrics(store, "agent-process", 2000, recovered);
            engine.evaluate_metrics("agent-process", recovered, 2000);

            THEN("the status returns to green and pending count is cleared")
            {
                auto status = store.latest_status_for_indicator("agent-process", "process:TheWatcherAgent.exe");
                REQUIRE(status.has_value());
                REQUIRE(status->new_status == "green");
                REQUIRE_FALSE(store.get_pending_status("agent-process", "process:TheWatcherAgent.exe").has_value());
            }
        }
    }
}

SCENARIO("Maintenance clears active alerts and records blue indicator state")
{
    GIVEN("an approved agent with an active alert")
    {
        SqliteStore store(":memory:");
        insert_approved_agent(store, "agent-maint");

        AlertRecord alert;
        alert.agent_id = "agent-maint";
        alert.indicator = "cpu";
        alert.old_status = "green";
        alert.new_status = "red";
        alert.message = "cpu changed from green to red";
        alert.created_at = 1000;
        store.insert_alert(alert);

        WHEN("the status engine enters maintenance")
        {
            StatusEngine engine(store);
            engine.enter_maintenance("agent-maint", "patch window", 5000, 2000);

            THEN("active alerts are cleared")
            {
                REQUIRE(store.list_unacknowledged_alerts().empty());
            }

            AND_THEN("the agent and indicators are put into maintenance state")
            {
                auto agent = store.get_agent("agent-maint");
                REQUIRE(agent.has_value());
                REQUIRE(agent->maintenance == true);
                REQUIRE(agent->maintenance_reason == "patch window");
                REQUIRE(agent->maintenance_until == 5000);

                auto cpu_status = store.latest_status_for_indicator("agent-maint", "cpu");
                REQUIRE(cpu_status.has_value());
                REQUIRE(cpu_status->new_status == "blue");
            }
        }
    }
}

SCENARIO("status_to_string and status_from_string are mutual inverses for all states")
{
    GIVEN("each IndicatorStatus value")
    {
        const std::vector<std::pair<IndicatorStatus, std::string>> cases = {
            {IndicatorStatus::Green,  "green"},
            {IndicatorStatus::Grey,   "grey"},
            {IndicatorStatus::Yellow, "yellow"},
            {IndicatorStatus::Amber,  "amber"},
            {IndicatorStatus::Red,    "red"},
            {IndicatorStatus::Blue,   "blue"},
        };

        WHEN("each status is converted to string and back")
        {
            THEN("the roundtrip produces the original status")
            {
                for (const auto& [status, name] : cases)
                {
                    REQUIRE(status_to_string(status) == name);
                    REQUIRE(status_from_string(name) == status);
                }
            }
        }

        WHEN("an unknown string is converted")
        {
            THEN("it maps to Grey")
            {
                REQUIRE(status_from_string("unknown") == IndicatorStatus::Grey);
                REQUIRE(status_from_string("") == IndicatorStatus::Grey);
            }
        }

        WHEN("the legacy alias 'orange' is converted")
        {
            THEN("it maps to Amber")
            {
                REQUIRE(status_from_string("orange") == IndicatorStatus::Amber);
            }
        }
    }
}

SCENARIO("is_worse_status returns true only when severity strictly increases")
{
    GIVEN("the full severity ordering: Green < Grey < Yellow < Amber < Red")
    {
        WHEN("a status transitions to something more severe")
        {
            THEN("is_worse_status returns true")
            {
                REQUIRE(is_worse_status(IndicatorStatus::Green,  IndicatorStatus::Yellow));
                REQUIRE(is_worse_status(IndicatorStatus::Green,  IndicatorStatus::Amber));
                REQUIRE(is_worse_status(IndicatorStatus::Green,  IndicatorStatus::Red));
                REQUIRE(is_worse_status(IndicatorStatus::Yellow, IndicatorStatus::Amber));
                REQUIRE(is_worse_status(IndicatorStatus::Yellow, IndicatorStatus::Red));
                REQUIRE(is_worse_status(IndicatorStatus::Amber,  IndicatorStatus::Red));
                REQUIRE(is_worse_status(IndicatorStatus::Grey,   IndicatorStatus::Yellow));
            }
        }

        WHEN("a status stays the same or improves")
        {
            THEN("is_worse_status returns false")
            {
                REQUIRE_FALSE(is_worse_status(IndicatorStatus::Red,    IndicatorStatus::Red));
                REQUIRE_FALSE(is_worse_status(IndicatorStatus::Red,    IndicatorStatus::Amber));
                REQUIRE_FALSE(is_worse_status(IndicatorStatus::Red,    IndicatorStatus::Green));
                REQUIRE_FALSE(is_worse_status(IndicatorStatus::Amber,  IndicatorStatus::Green));
                REQUIRE_FALSE(is_worse_status(IndicatorStatus::Yellow, IndicatorStatus::Green));
                REQUIRE_FALSE(is_worse_status(IndicatorStatus::Green,  IndicatorStatus::Green));
            }
        }

        WHEN("either status is Blue (maintenance)")
        {
            THEN("is_worse_status always returns false")
            {
                REQUIRE_FALSE(is_worse_status(IndicatorStatus::Blue,  IndicatorStatus::Red));
                REQUIRE_FALSE(is_worse_status(IndicatorStatus::Green, IndicatorStatus::Blue));
                REQUIRE_FALSE(is_worse_status(IndicatorStatus::Blue,  IndicatorStatus::Blue));
            }
        }
    }
}

SCENARIO("exit_maintenance clears the maintenance flag on the agent")
{
    GIVEN("an approved agent in maintenance mode")
    {
        SqliteStore store(":memory:");
        insert_approved_agent(store, "agent-exit-maint");

        StatusEngine engine(store);
        engine.enter_maintenance("agent-exit-maint", "planned downtime", 99999, 1000);

        auto in_maint = store.get_agent("agent-exit-maint");
        REQUIRE(in_maint.has_value());
        REQUIRE(in_maint->maintenance == true);

        WHEN("exit_maintenance is called")
        {
            engine.exit_maintenance("agent-exit-maint");

            THEN("the agent is no longer in maintenance mode")
            {
                auto after = store.get_agent("agent-exit-maint");
                REQUIRE(after.has_value());
                REQUIRE(after->maintenance == false);
                REQUIRE(after->maintenance_reason.empty());
                REQUIRE(after->maintenance_until == 0);
            }
        }
    }
}
