#include "network_collector.hpp"

#include "common/SingleLog.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
// clang-format off
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
// clang-format on
#endif

namespace thewatcher::agent
{

namespace detail
{
    namespace
    {
        std::string lowercase_ascii(std::string_view value)
        {
            std::string out(value);
            std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return out;
        }

        bool contains(std::string_view haystack, std::string_view needle)
        {
            return haystack.find(needle) != std::string_view::npos;
        }
    } // namespace

    bool is_windows_ipconfig_adapter_name(std::string_view name, std::string_view description)
    {
        const auto haystack = lowercase_ascii(std::string(name) + " " + std::string(description));
        return !contains(haystack, "packet scheduler") && !contains(haystack, "wfp native mac layer") &&
               !contains(haystack, "lightweight filter") && !contains(haystack, "hyper-v virtual switch") &&
               !contains(haystack, "network bridge") && !contains(haystack, "6to4 adapter") &&
               !contains(haystack, "isatap") && !contains(haystack, "teredo") && !contains(haystack, "wan miniport");
    }

    bool is_windows_reportable_if_type(unsigned int if_type)
    {
        constexpr unsigned int ethernet_csmacd = 6;
        constexpr unsigned int ppp = 23;
        constexpr unsigned int ieee80211 = 71;
        return if_type == ethernet_csmacd || if_type == ppp || if_type == ieee80211;
    }
} // namespace detail

namespace
{

#if defined(__linux__) || defined(_WIN32)

    uint64_t per_second(uint64_t previous, uint64_t current, double elapsed_seconds)
    {
        if (elapsed_seconds <= 0.0 || current < previous)
        {
            return 0;
        }
        return static_cast<uint64_t>(static_cast<double>(current - previous) / elapsed_seconds);
    }

#endif

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
    struct WindowsAdapterInfo
    {
        unsigned long if_index = 0;
        std::string name;
        std::string description;
        std::string ip_address;
    };

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

    std::optional<std::string> first_reportable_ip(const IP_ADAPTER_UNICAST_ADDRESS* first_unicast)
    {
        for (auto* unicast = first_unicast; unicast != nullptr; unicast = unicast->Next)
        {
            const auto ip = sockaddr_to_string(unicast->Address.lpSockaddr);
            if (!ip.empty() && ip != "127.0.0.1" && ip != "::1")
                return ip;
        }
        return std::nullopt;
    }

    std::vector<WindowsAdapterInfo> read_windows_ipconfig_adapters()
    {
        std::vector<WindowsAdapterInfo> result;
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
            return result;

        for (auto* adapter = adapters; adapter != nullptr; adapter = adapter->Next)
        {
            WindowsAdapterInfo info;
            info.if_index = adapter->IfIndex;
            info.name = wide_to_utf8(adapter->FriendlyName);
            info.description = wide_to_utf8(adapter->Description);
            if (info.name.empty())
                info.name = info.description;

            if (!detail::is_windows_reportable_if_type(adapter->IfType) ||
                !detail::is_windows_ipconfig_adapter_name(info.name, info.description))
            {
                continue;
            }

            if (const auto ip = first_reportable_ip(adapter->FirstUnicastAddress))
                info.ip_address = *ip;
            result.push_back(std::move(info));
        }
        return result;
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
    const auto adapters = read_windows_ipconfig_adapters();
    const auto now = std::chrono::steady_clock::now();
    for (const auto& adapter : adapters)
    {
        MIB_IFROW row{};
        row.dwIndex = adapter.if_index;
        if (::GetIfEntry(&row) != NO_ERROR)
            continue;

        NetworkMetrics network;
        network.interface_name = adapter.name;
        network.ip_address = adapter.ip_address;
        network.is_up = row.dwOperStatus == IF_OPER_STATUS_OPERATIONAL;
        network.errors_in = row.dwInErrors;
        network.errors_out = row.dwOutErrors;
        network.drops_in = row.dwInDiscards;
        network.drops_out = row.dwOutDiscards;

        const auto key = std::to_string(adapter.if_index);
        auto previous = prev_.find(key);
        if (previous != prev_.end())
        {
            const auto elapsed = std::chrono::duration<double>(now - previous->second.sampled_at).count();
            network.bytes_recv_per_sec = per_second(previous->second.rx_bytes, row.dwInOctets, elapsed);
            network.bytes_sent_per_sec = per_second(previous->second.tx_bytes, row.dwOutOctets, elapsed);
            network.packets_recv_per_sec = per_second(previous->second.rx_packets, row.dwInUcastPkts, elapsed);
            network.packets_sent_per_sec = per_second(previous->second.tx_packets, row.dwOutUcastPkts, elapsed);
        }

        prev_[key] = {row.dwInOctets, row.dwOutOctets, row.dwInUcastPkts, row.dwOutUcastPkts, now};
        metrics.networks.push_back(std::move(network));
    }
    LOGF_TRACE("Windows network collector updated interfaces=%zu", metrics.networks.size());
#else
    LOG_DEBUG("Network collector has no implementation for this platform");
#endif
}

} // namespace thewatcher::agent
