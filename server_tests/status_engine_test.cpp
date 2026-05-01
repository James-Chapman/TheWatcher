#include "../server/status_engine.hpp"
#include "../server/store_sqlite.hpp"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

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

void insert_metrics(SqliteStore& store, const std::string& agent_id, int64_t timestamp_ms, const SystemMetrics& metrics)
{
    store.insert_metrics({agent_id, timestamp_ms, nlohmann::json(metrics).dump()});
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

SCENARIO("Status engine records transitions and alerts only on worsening changes")
{
    GIVEN("an approved agent with baseline metrics")
    {
        SqliteStore store(":memory:");
        insert_approved_agent(store, "agent-status");
        StatusEngine engine(store);

        auto baseline = metrics_with_cpu(10.0);
        insert_metrics(store, "agent-status", 1000, baseline);
        engine.evaluate_metrics("agent-status", baseline, 1000);

        WHEN("the CPU indicator worsens relative to the recent average")
        {
            auto high = metrics_with_cpu(30.0);
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

SCENARIO("Status engine uses per-agent thresholds before global settings")
{
    GIVEN("two approved agents with different CPU threshold settings")
    {
        SqliteStore store(":memory:");
        insert_approved_agent(store, "agent-custom-thresholds");
        insert_approved_agent(store, "agent-absolute-cap");

        auto custom = store.get_agent("agent-custom-thresholds");
        REQUIRE(custom.has_value());
        custom->cpu_warning_pct_of_avg = 140.0;
        custom->cpu_degraded_pct_of_avg = 180.0;
        custom->cpu_critical_pct_of_avg = 220.0;
        store.set_agent_thresholds(*custom);

        auto capped = store.get_agent("agent-absolute-cap");
        REQUIRE(capped.has_value());
        capped->cpu_warning_pct_of_avg = 500.0;
        capped->cpu_degraded_pct_of_avg = 600.0;
        capped->cpu_critical_pct_of_avg = 700.0;
        store.set_agent_thresholds(*capped);

        StatusEngine engine(store);

        WHEN("CPU worsens enough for the custom warning threshold but not the custom degraded threshold")
        {
            auto baseline = metrics_with_cpu(10.0);
            insert_metrics(store, "agent-custom-thresholds", 1000, baseline);
            engine.evaluate_metrics("agent-custom-thresholds", baseline, 1000);

            auto elevated = metrics_with_cpu(30.0);
            insert_metrics(store, "agent-custom-thresholds", 2000, elevated);
            engine.evaluate_metrics("agent-custom-thresholds", elevated, 2000);

            THEN("the per-agent thresholds classify the indicator as warning")
            {
                auto status = store.latest_status_for_indicator("agent-custom-thresholds", "cpu");
                REQUIRE(status.has_value());
                REQUIRE(status->new_status == "yellow");
            }
        }

        AND_WHEN("CPU crosses the absolute warning cap despite high custom thresholds")
        {
            auto baseline = metrics_with_cpu(10.0);
            insert_metrics(store, "agent-absolute-cap", 1000, baseline);
            engine.evaluate_metrics("agent-absolute-cap", baseline, 1000);

            auto capped_cpu = metrics_with_cpu(72.0);
            insert_metrics(store, "agent-absolute-cap", 2000, capped_cpu);
            engine.evaluate_metrics("agent-absolute-cap", capped_cpu, 2000);

            THEN("the absolute cap still marks the indicator as warning")
            {
                auto status = store.latest_status_for_indicator("agent-absolute-cap", "cpu");
                REQUIRE(status.has_value());
                REQUIRE(status->new_status == "yellow");
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
