#pragma once

#include "collector.hpp"

namespace thewatcher::agent
{

class TemperatureCollector final : public Collector
{
public:
    std::string_view name() const noexcept override
    {
        return "temperature";
    }
    void update(SystemMetrics& metrics) override;
};

} // namespace thewatcher::agent
