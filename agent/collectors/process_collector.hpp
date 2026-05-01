#pragma once

#include "collector.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace thewatcher::agent
{

class ProcessCollector final : public Collector
{
public:
    explicit ProcessCollector(int limit = 25) : limit_{limit}
    {
    }

    std::string_view name() const noexcept override
    {
        return "process";
    }
    void update(SystemMetrics& metrics) override;
    void set_limit(int limit)
    {
        limit_ = limit;
    }

private:
    int limit_;

    struct CpuSample
    {
        uint64_t cpu_ticks = 0;
        std::chrono::steady_clock::time_point sampled_at{};
    };

    std::unordered_map<uint32_t, CpuSample> prev_cpu_;
};

} // namespace thewatcher::agent
