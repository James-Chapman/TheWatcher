#pragma once

#include "collector.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace thewatcher::agent
{

class NetworkCollector final : public Collector
{
public:
    std::string_view name() const noexcept override
    {
        return "network";
    }
    void update(SystemMetrics& metrics) override;

private:
    struct InterfaceSample
    {
        uint64_t rx_bytes = 0;
        uint64_t tx_bytes = 0;
        uint64_t rx_packets = 0;
        uint64_t tx_packets = 0;
        std::chrono::steady_clock::time_point sampled_at{};
    };

    std::unordered_map<std::string, InterfaceSample> prev_;
};

} // namespace thewatcher::agent
