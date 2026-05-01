#pragma once

#include "../../common/metrics.hpp"

#include <string_view>

namespace thewatcher::agent
{

class Collector
{
public:
    Collector() = default;
    virtual ~Collector() = default;

    virtual std::string_view name() const noexcept = 0;
    virtual void update(SystemMetrics& metrics) = 0;
};

} // namespace thewatcher::agent
