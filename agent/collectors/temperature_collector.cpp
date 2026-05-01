#include "temperature_collector.hpp"

#ifdef __linux__
#include <filesystem>
#include <fstream>
#include <string>
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#include <cstdio>
#include <sys/sysctl.h>
#endif

namespace thewatcher::agent
{

#ifdef __linux__

void TemperatureCollector::update(SystemMetrics& metrics)
{
    namespace fs = std::filesystem;
    metrics.temperatures.clear();

    // /sys/class/thermal/thermal_zone*/
    std::error_code ec;
    for (const auto& zone : fs::directory_iterator("/sys/class/thermal", ec))
    {
        auto name = zone.path().filename().string();
        if (name.rfind("thermal_zone", 0) != 0)
            continue;

        std::ifstream temp_f(zone.path() / "temp");
        int raw = 0;
        if (!(temp_f >> raw))
            continue;

        std::string type = "thermal";
        std::ifstream type_f(zone.path() / "type");
        if (type_f)
            type_f >> type;

        TemperatureMetrics tm;
        tm.sensor_name = type;
        tm.sensor_label = name;
        tm.temperature_celsius = raw / 1000.0;
        metrics.temperatures.push_back(std::move(tm));
    }

    // /sys/class/hwmon/hwmon*/temp*_input
    for (const auto& hwmon : fs::directory_iterator("/sys/class/hwmon", ec))
    {
        std::string hw_name;
        std::ifstream name_f(hwmon.path() / "name");
        if (name_f)
            name_f >> hw_name;

        for (int n = 1; n <= 16; ++n)
        {
            auto input_path = hwmon.path() / ("temp" + std::to_string(n) + "_input");
            if (!fs::exists(input_path, ec))
                break;
            std::ifstream inp(input_path);
            int raw = 0;
            if (!(inp >> raw))
                continue;

            std::string label;
            std::ifstream lf(hwmon.path() / ("temp" + std::to_string(n) + "_label"));
            if (lf)
                lf >> label;

            TemperatureMetrics tm;
            tm.sensor_name = hw_name;
            tm.sensor_label = label.empty() ? ("temp" + std::to_string(n)) : label;
            tm.temperature_celsius = raw / 1000.0;
            metrics.temperatures.push_back(std::move(tm));
        }
    }
}

#elif defined(_WIN32)

// Temperature collection on Windows requires a kernel driver (e.g. LibreHardwareMonitor).
// Skipped in this implementation.
void TemperatureCollector::update(SystemMetrics& metrics)
{
    metrics.temperatures.clear();
}

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)

void TemperatureCollector::update(SystemMetrics& metrics)
{
    metrics.temperatures.clear();

#ifdef __FreeBSD__
    // FreeBSD: dev.cpu.N.temperature in deci-kelvin
    int ncpu = 0;
    size_t len = sizeof(ncpu);
    ::sysctlbyname("hw.ncpu", &ncpu, &len, nullptr, 0);

    for (int i = 0; i < ncpu && i < 64; ++i)
    {
        char mib[64];
        std::snprintf(mib, sizeof(mib), "dev.cpu.%d.temperature", i);
        int raw = 0;
        len = sizeof(raw);
        if (::sysctlbyname(mib, &raw, &len, nullptr, 0) == 0)
        {
            TemperatureMetrics tm;
            tm.sensor_name = "cpu";
            tm.sensor_label = "cpu" + std::to_string(i);
            tm.temperature_celsius = (raw - 2732) / 10.0; // decikelvin → celsius
            metrics.temperatures.push_back(std::move(tm));
        }
    }
#endif
}

#endif

} // namespace thewatcher::agent
