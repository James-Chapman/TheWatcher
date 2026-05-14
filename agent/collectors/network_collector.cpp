#include "network_collector.hpp"

#include "common/SingleLog.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#endif

namespace thewatcher::agent
{

namespace
{

    uint64_t per_second(uint64_t previous, uint64_t current, double elapsed_seconds)
    {
        if (elapsed_seconds <= 0.0 || current < previous)
        {
            return 0;
        }
        return static_cast<uint64_t>(static_cast<double>(current - previous) / elapsed_seconds);
    }

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

#endif

#ifdef _WIN32
    std::string wide_to_utf8(const wchar_t* value)
    {
        if (value == nullptr || *value == L'\0')
            return {};
        const int needed = ::WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
        if (needed <= 1)
            return {};
        std::string out(static_cast<size_t>(needed - 1), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, value, -1, out.data(), needed, nullptr, nullptr);
        return out;
    }

    std::string sockaddr_to_string(const SOCKADDR* address)
    {
        char buffer[INET6_ADDRSTRLEN] = {};
        if (address == nullptr)
            return {};
        if (address->sa_family == AF_INET)
        {
            const auto* in = reinterpret_cast<const sockaddr_in*>(address);
            return ::InetNtopA(AF_INET, const_cast<IN_ADDR*>(&in->sin_addr), buffer, sizeof(buffer)) ? buffer : "";
        }
        if (address->sa_family == AF_INET6)
        {
            const auto* in6 = reinterpret_cast<const sockaddr_in6*>(address);
            return ::InetNtopA(AF_INET6, const_cast<IN6_ADDR*>(&in6->sin6_addr), buffer, sizeof(buffer)) ? buffer : "";
        }
        return {};
    }

    std::unordered_map<uint64_t, std::string> adapter_ips()
    {
        std::unordered_map<uint64_t, std::string> ips;
        ULONG size = 15 * 1024;
        std::vector<unsigned char> buffer(size);
        auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        ULONG rc = ::GetAdaptersAddresses(AF_UNSPEC,
                                          GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                                          nullptr, adapters, &size);
        if (rc == ERROR_BUFFER_OVERFLOW)
        {
            buffer.resize(size);
            adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
            rc = ::GetAdaptersAddresses(AF_UNSPEC,
                                        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                                        nullptr, adapters, &size);
        }
        if (rc != NO_ERROR)
            return ips;

        for (auto* adapter = adapters; adapter != nullptr; adapter = adapter->Next)
        {
            for (auto* unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next)
            {
                const auto ip = sockaddr_to_string(unicast->Address.lpSockaddr);
                if (!ip.empty() && ip != "127.0.0.1" && ip != "::1")
                {
                    ips[adapter->Luid.Value] = ip;
                    break;
                }
            }
        }
        return ips;
    }
#endif

} // namespace

void NetworkCollector::update(SystemMetrics& metrics)
{
    LOG_FUNCTION_TRACE
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
    LOGF_TRACE("Network collector updated interfaces=%zu", metrics.networks.size());
#elif defined(_WIN32)
    PMIB_IF_TABLE2 table = nullptr;
    if (::GetIfTable2(&table) != NO_ERROR || table == nullptr)
    {
        LOG_DEBUG("Windows network collector could not read interface table");
        return;
    }

    const auto ips = adapter_ips();
    const auto now = std::chrono::steady_clock::now();
    for (ULONG i = 0; i < table->NumEntries; ++i)
    {
        const auto& row = table->Table[i];
        NetworkMetrics network;
        network.interface_name = wide_to_utf8(row.Alias);
        if (network.interface_name.empty())
            network.interface_name = wide_to_utf8(row.Description);
        network.ip_address = ips.contains(row.InterfaceLuid.Value) ? ips.at(row.InterfaceLuid.Value) : "";
        network.is_up = row.OperStatus == IfOperStatusUp;
        network.errors_in = row.InErrors;
        network.errors_out = row.OutErrors;
        network.drops_in = row.InDiscards;
        network.drops_out = row.OutDiscards;

        const auto key = std::to_string(row.InterfaceIndex);
        auto previous = prev_.find(key);
        if (previous != prev_.end())
        {
            const auto elapsed = std::chrono::duration<double>(now - previous->second.sampled_at).count();
            network.bytes_recv_per_sec = per_second(previous->second.rx_bytes, row.InOctets, elapsed);
            network.bytes_sent_per_sec = per_second(previous->second.tx_bytes, row.OutOctets, elapsed);
            network.packets_recv_per_sec = per_second(previous->second.rx_packets, row.InUcastPkts, elapsed);
            network.packets_sent_per_sec = per_second(previous->second.tx_packets, row.OutUcastPkts, elapsed);
        }

        prev_[key] = {row.InOctets, row.OutOctets, row.InUcastPkts, row.OutUcastPkts, now};
        metrics.networks.push_back(std::move(network));
    }
    ::FreeMibTable(table);
    LOGF_TRACE("Windows network collector updated interfaces=%zu", metrics.networks.size());
#else
    LOG_DEBUG("Network collector has no implementation for this platform");
#endif
}

} // namespace thewatcher::agent
