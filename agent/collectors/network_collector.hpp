#pragma once

#include "collector.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace thewatcher::agent
{

namespace detail
{
    bool is_windows_ipconfig_adapter_name(std::string_view name, std::string_view description);
    bool is_windows_reportable_if_type(unsigned int if_type);
} // namespace detail

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
