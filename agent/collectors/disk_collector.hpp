#pragma once

#include "collector.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace thewatcher::agent
{

class DiskCollector final : public Collector
{
public:
    std::string_view name() const noexcept override
    {
        return "disk";
    }
    void update(SystemMetrics& metrics) override;

private:
    struct IoSample
    {
        uint64_t read_sectors = 0;
        uint64_t write_sectors = 0;
        std::chrono::steady_clock::time_point sampled_at{};
    };

    std::unordered_map<std::string, IoSample> prev_io_;

#ifdef __linux__
    std::unordered_map<std::string, IoSample> read_io_samples() const;
#endif
};

} // namespace thewatcher::agent
