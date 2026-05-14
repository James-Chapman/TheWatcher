#include "cpu_collector.hpp"

#include "common/SingleLog.hpp"

#include <algorithm>
#include <thread>
#include <utility>

#ifdef __linux__
#include <fstream>
#include <sstream>
#include <string>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace thewatcher::agent
{

namespace
{

#ifdef __linux__

} // namespace

CpuCollector::Sample CpuCollector::read_sample() const
{
    CpuCollector::Sample sample;
    sample.sampled_at = std::chrono::steady_clock::now();

    std::ifstream stat("/proc/stat");
    std::string line;
    while (std::getline(stat, line))
    {
        if (line.rfind("cpu", 0) != 0)
        {
            break;
        }

        std::istringstream row(line);
        std::string label;
        uint64_t user = 0;
        uint64_t nice = 0;
        uint64_t system = 0;
        uint64_t idle = 0;
        uint64_t iowait = 0;
        uint64_t irq = 0;
        uint64_t softirq = 0;
        uint64_t steal = 0;
        row >> label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

        const uint64_t idle_all = idle + iowait;
        const uint64_t total = user + nice + system + idle + iowait + irq + softirq + steal;
        if (label == "cpu")
        {
            sample.idle = idle_all;
            sample.total = total;
        }
        else
        {
            sample.core_idle.push_back(idle_all);
            sample.core_total.push_back(total);
        }
    }

    return sample;
}

namespace
{

    void read_load_average(CpuMetrics& cpu)
    {
        std::ifstream loadavg("/proc/loadavg");
        loadavg >> cpu.load_avg_1m >> cpu.load_avg_5m >> cpu.load_avg_15m;
    }

#elif defined(_WIN32)

    uint64_t filetime_to_u64(const FILETIME& ft)
    {
        return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    }

} // namespace

CpuCollector::Sample CpuCollector::read_sample() const
{
    CpuCollector::Sample sample;
    sample.sampled_at = std::chrono::steady_clock::now();

    FILETIME idle_time{};
    FILETIME kernel_time{};
    FILETIME user_time{};
    if (::GetSystemTimes(&idle_time, &kernel_time, &user_time))
    {
        sample.idle = filetime_to_u64(idle_time);
        sample.total = filetime_to_u64(kernel_time) + filetime_to_u64(user_time);
    }

    return sample;
}

namespace
{

#endif

#if defined(__linux__) || defined(_WIN32)

    double percent_from_delta(uint64_t previous_idle, uint64_t previous_total, uint64_t current_idle,
                              uint64_t current_total)
    {
        if (current_total <= previous_total || current_idle < previous_idle)
        {
            return 0.0;
        }

        const auto total_delta = static_cast<double>(current_total - previous_total);
        const auto idle_delta = static_cast<double>(current_idle - previous_idle);
        return total_delta > 0.0 ? 100.0 * (total_delta - idle_delta) / total_delta : 0.0;
    }

#endif

} // namespace

void CpuCollector::update(SystemMetrics& metrics)
{
    LOG_FUNCTION_TRACE
    auto& cpu = metrics.cpu;
    cpu.num_logical_cores = static_cast<int>(std::thread::hardware_concurrency());

#if defined(__linux__) || defined(_WIN32)
    auto sample = read_sample();
    if (sample.total == 0)
    {
        LOG_DEBUG("CPU collector could not read a valid CPU sample");
        return;
    }

    if (!first_sample_)
    {
        cpu.usage_percent = percent_from_delta(prev_.idle, prev_.total, sample.idle, sample.total);

        const auto count = std::min(prev_.core_total.size(), sample.core_total.size());
        cpu.per_core_usage.clear();
        cpu.per_core_usage.reserve(count);
        for (std::size_t index = 0; index < count; ++index)
        {
            cpu.per_core_usage.push_back(percent_from_delta(prev_.core_idle[index], prev_.core_total[index],
                                                            sample.core_idle[index], sample.core_total[index]));
        }
    }

#ifdef __linux__
    read_load_average(cpu);
#endif

    prev_ = std::move(sample);
    first_sample_ = false;
    LOGF_TRACE("CPU collector updated usage=%.2f cores=%d per_core_samples=%zu", cpu.usage_percent,
               cpu.num_logical_cores, cpu.per_core_usage.size());
#else
    LOG_DEBUG("CPU collector has no implementation for this platform");
#endif
}

} // namespace thewatcher::agent
