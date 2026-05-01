#include "../agent/collectors/collector.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace thewatcher;
using namespace thewatcher::agent;

namespace
{

class FakeCollector final : public Collector
{
public:
    std::string_view name() const noexcept override
    {
        return "fake";
    }

    void update(SystemMetrics& metrics) override
    {
        metrics.hostname = "updated-by-fake";
    }
};

} // namespace

SCENARIO("Collectors expose the agent metrics update contract")
{
    GIVEN("a collector referenced through the base interface")
    {
        FakeCollector fake;
        Collector& collector = fake;
        SystemMetrics metrics;

        WHEN("the agent asks the collector to update metrics")
        {
            collector.update(metrics);

            THEN("the collector can mutate the shared SystemMetrics snapshot")
            {
                REQUIRE(metrics.hostname == "updated-by-fake");
            }

            AND_THEN("the collector reports a stable name")
            {
                REQUIRE(collector.name() == "fake");
            }
        }
    }
}
