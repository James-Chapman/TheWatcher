#pragma once

#include "collector.hpp"

#include <chrono>
#include <cstdint>
#include <vector>

namespace thewatcher::agent
{

class CpuCollector final : public Collector
{
public:
    std::string_view name() const noexcept override
    {
        return "cpu";
    }
    void update(SystemMetrics& metrics) override;

private:
    struct Sample
    {
        uint64_t idle = 0;
        uint64_t total = 0;
        std::vector<uint64_t> core_idle;
        std::vector<uint64_t> core_total;
        std::chrono::steady_clock::time_point sampled_at{};
    };

    Sample prev_{};
    bool first_sample_{true};

#if defined(__linux__) || defined(_WIN32)
    Sample read_sample() const;
#endif
};

} // namespace thewatcher::agent
