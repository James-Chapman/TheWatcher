#pragma once

#include "collector.hpp"

namespace thewatcher::agent
{

class MemoryCollector final : public Collector
{
public:
    std::string_view name() const noexcept override
    {
        return "memory";
    }
    void update(SystemMetrics& metrics) override;
};

} // namespace thewatcher::agent
