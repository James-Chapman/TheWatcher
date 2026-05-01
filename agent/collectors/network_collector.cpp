#include "network_collector.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

namespace thewatcher::agent
{

namespace
{

#ifdef __linux__

    struct InterfaceCounters
    {
        std::string name;
        uint64_t rx_bytes = 0;
        uint64_t rx_packets = 0;
        uint64_t rx_errors = 0;
        uint64_t rx_drops = 0;
        uint64_t tx_bytes = 0;
        uint64_t tx_packets = 0;
        uint64_t tx_errors = 0;
        uint64_t tx_drops = 0;
    };

    std::vector<InterfaceCounters> read_interfaces()
    {
        std::vector<InterfaceCounters> interfaces;
        std::ifstream netdev("/proc/net/dev");
        std::string line;
        std::getline(netdev, line);
        std::getline(netdev, line);

        while (std::getline(netdev, line))
        {
            const auto colon = line.find(':');
            if (colon == std::string::npos)
            {
                continue;
            }

            InterfaceCounters counters;
            counters.name = line.substr(0, colon);
            counters.name.erase(std::remove_if(counters.name.begin(), counters.name.end(), ::isspace),
                                counters.name.end());

            std::istringstream row(line.substr(colon + 1));
            row >> counters.rx_bytes >> counters.rx_packets >> counters.rx_errors >> counters.rx_drops;

            uint64_t ignored = 0;
            row >> ignored >> ignored >> ignored >> ignored;

            row >> counters.tx_bytes >> counters.tx_packets >> counters.tx_errors >> counters.tx_drops;

            if (!counters.name.empty())
            {
                interfaces.push_back(std::move(counters));
            }
        }

        return interfaces;
    }

    uint64_t per_second(uint64_t previous, uint64_t current, double elapsed_seconds)
    {
        if (elapsed_seconds <= 0.0 || current < previous)
        {
            return 0;
        }
        return static_cast<uint64_t>(static_cast<double>(current - previous) / elapsed_seconds);
    }

#endif

} // namespace

void NetworkCollector::update(SystemMetrics& metrics)
{
    metrics.networks.clear();

#ifdef __linux__
    const auto now = std::chrono::steady_clock::now();
    for (const auto& counters : read_interfaces())
    {
        NetworkMetrics network;
        network.interface_name = counters.name;
        network.errors_in = counters.rx_errors;
        network.errors_out = counters.tx_errors;
        network.drops_in = counters.rx_drops;
        network.drops_out = counters.tx_drops;
        network.is_up = true;

        auto previous = prev_.find(counters.name);
        if (previous != prev_.end())
        {
            const auto elapsed = std::chrono::duration<double>(now - previous->second.sampled_at).count();
            network.bytes_recv_per_sec = per_second(previous->second.rx_bytes, counters.rx_bytes, elapsed);
            network.bytes_sent_per_sec = per_second(previous->second.tx_bytes, counters.tx_bytes, elapsed);
            network.packets_recv_per_sec = per_second(previous->second.rx_packets, counters.rx_packets, elapsed);
            network.packets_sent_per_sec = per_second(previous->second.tx_packets, counters.tx_packets, elapsed);
        }

        prev_[counters.name] = {
            counters.rx_bytes, counters.tx_bytes, counters.rx_packets, counters.tx_packets, now,
        };
        metrics.networks.push_back(std::move(network));
    }
#endif
}

} // namespace thewatcher::agent
