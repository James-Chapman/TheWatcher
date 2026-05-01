#include "memory_collector.hpp"

#include "common/SingleLog.hpp"

#include <cstdint>

#ifdef __linux__
#include <fstream>
#include <string>
#elif defined(_WIN32)
#include <sysinfoapi.h>
#include <windows.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <vm/vm_param.h>
#else
#error "Unsupported platform"
#endif

namespace thewatcher::agent
{

#ifdef __linux__

void MemoryCollector::update(SystemMetrics& metrics)
{
    LOG_FUNCTION_TRACE
    std::ifstream f("/proc/meminfo");
    if (!f)
    {
        LOG_DEBUG("Memory collector could not open /proc/meminfo");
        return;
    }

    auto& m = metrics.memory;
    std::string key;
    uint64_t value;
    std::string unit;

    uint64_t mem_total = 0, mem_free = 0, mem_available = 0, buffers = 0, cached = 0, sreclaimable = 0, swap_total = 0,
             swap_free = 0;

    while (f >> key >> value)
    {
        f.ignore(256, '\n'); // skip unit and newline
        value *= 1024;       // kB → bytes
        if (key == "MemTotal:")
            mem_total = value;
        else if (key == "MemFree:")
            mem_free = value;
        else if (key == "MemAvailable:")
            mem_available = value;
        else if (key == "Buffers:")
            buffers = value;
        else if (key == "Cached:")
            cached = value;
        else if (key == "SReclaimable:")
            sreclaimable = value;
        else if (key == "SwapTotal:")
            swap_total = value;
        else if (key == "SwapFree:")
            swap_free = value;
    }

    m.total_bytes = mem_total;
    m.free_bytes = mem_free;
    m.available_bytes = mem_available;
    m.buffers_bytes = buffers;
    m.cached_bytes = cached + sreclaimable;
    m.used_bytes = mem_total - mem_free - buffers - cached - sreclaimable;
    m.usage_percent = mem_total > 0 ? 100.0 * static_cast<double>(mem_total - mem_available) / mem_total : 0.0;
    m.swap_total_bytes = swap_total;
    m.swap_free_bytes = swap_free;
    m.swap_used_bytes = swap_total - swap_free;
    LOGF_TRACE("Memory collector updated total=%llu used=%llu available=%llu usage=%.2f",
               static_cast<unsigned long long>(m.total_bytes), static_cast<unsigned long long>(m.used_bytes),
               static_cast<unsigned long long>(m.available_bytes), m.usage_percent);
}

#elif defined(_WIN32)

void MemoryCollector::update(SystemMetrics& metrics)
{
    LOG_FUNCTION_TRACE
    MEMORYSTATUSEX stat{};
    stat.dwLength = sizeof(stat);
    if (!::GlobalMemoryStatusEx(&stat))
    {
        LOG_DEBUG("Memory collector GlobalMemoryStatusEx failed");
        return;
    }

    auto& m = metrics.memory;
    m.total_bytes = stat.ullTotalPhys;
    m.free_bytes = stat.ullAvailPhys;
    m.available_bytes = stat.ullAvailPhys;
    m.used_bytes = stat.ullTotalPhys - stat.ullAvailPhys;
    m.usage_percent = static_cast<double>(stat.dwMemoryLoad); // already 0-100
    m.swap_total_bytes = stat.ullTotalPageFile - stat.ullTotalPhys;
    m.swap_free_bytes = stat.ullAvailPageFile;
    m.swap_used_bytes = m.swap_total_bytes - m.swap_free_bytes;
    LOGF_TRACE("Memory collector updated total=%llu used=%llu available=%llu usage=%.2f",
               static_cast<unsigned long long>(m.total_bytes), static_cast<unsigned long long>(m.used_bytes),
               static_cast<unsigned long long>(m.available_bytes), m.usage_percent);
}

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)

void MemoryCollector::update(SystemMetrics& metrics)
{
    LOG_FUNCTION_TRACE
    auto& m = metrics.memory;

    uint64_t hw_physmem = 0;
    size_t len = sizeof(hw_physmem);
    ::sysctlbyname("hw.physmem", &hw_physmem, &len, nullptr, 0);
    m.total_bytes = hw_physmem;

    uint32_t pagesize = 4096;
    len = sizeof(pagesize);
    ::sysctlbyname("vm.stats.vm.v_page_size", &pagesize, &len, nullptr, 0);

    uint32_t active = 0, inactive = 0, free_cnt = 0, cache_cnt = 0, wire_cnt = 0;
    len = sizeof(uint32_t);
    ::sysctlbyname("vm.stats.vm.v_active_count", &active, &len, nullptr, 0);
    ::sysctlbyname("vm.stats.vm.v_inactive_count", &inactive, &len, nullptr, 0);
    ::sysctlbyname("vm.stats.vm.v_free_count", &free_cnt, &len, nullptr, 0);
    ::sysctlbyname("vm.stats.vm.v_cache_count", &cache_cnt, &len, nullptr, 0);
    ::sysctlbyname("vm.stats.vm.v_wire_count", &wire_cnt, &len, nullptr, 0);

    m.free_bytes = static_cast<uint64_t>(free_cnt) * pagesize;
    m.available_bytes = static_cast<uint64_t>(free_cnt + inactive + cache_cnt) * pagesize;
    m.cached_bytes = static_cast<uint64_t>(cache_cnt) * pagesize;
    m.used_bytes = static_cast<uint64_t>(active + wire_cnt) * pagesize;
    m.usage_percent = m.total_bytes > 0 ? 100.0 * static_cast<double>(m.used_bytes) / m.total_bytes : 0.0;

    // Swap via xswdev is complex; use swapinfo sysctl where available
    uint64_t swap_total = 0, swap_used = 0;
    len = sizeof(swap_total);
    if (::sysctlbyname("vm.swap_total", &swap_total, &len, nullptr, 0) == 0)
    {
        len = sizeof(swap_used);
        ::sysctlbyname("vm.swap_reserved", &swap_used, &len, nullptr, 0);
    }
    m.swap_total_bytes = swap_total;
    m.swap_used_bytes = swap_used;
    m.swap_free_bytes = swap_total - swap_used;
    LOGF_TRACE("Memory collector updated total=%llu used=%llu available=%llu usage=%.2f",
               static_cast<unsigned long long>(m.total_bytes), static_cast<unsigned long long>(m.used_bytes),
               static_cast<unsigned long long>(m.available_bytes), m.usage_percent);
}

#endif

} // namespace thewatcher::agent
