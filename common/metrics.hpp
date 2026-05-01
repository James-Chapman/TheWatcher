#pragma once

#include <cstdint>
#include <msgpack.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace thewatcher
{

// ── CPU ───────────────────────────────────────────────────────────────────────

struct CpuMetrics
{
    double usage_percent = 0.0;
    double user_percent = 0.0;
    double system_percent = 0.0;
    double idle_percent = 0.0;
    double iowait_percent = 0.0; // Linux only, 0 elsewhere
    std::vector<double> per_core_usage;
    double frequency_mhz = 0.0;
    int num_physical_cores = 0;
    int num_logical_cores = 0;
    double load_avg_1m = 0.0; // Unix only, 0 on Windows
    double load_avg_5m = 0.0;
    double load_avg_15m = 0.0;

    MSGPACK_DEFINE_ARRAY(usage_percent, user_percent, system_percent, idle_percent, iowait_percent, per_core_usage,
                         frequency_mhz, num_physical_cores, num_logical_cores, load_avg_1m, load_avg_5m, load_avg_15m)
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CpuMetrics, usage_percent, user_percent, system_percent, idle_percent,
                                   iowait_percent, per_core_usage, frequency_mhz, num_physical_cores, num_logical_cores,
                                   load_avg_1m, load_avg_5m, load_avg_15m)

// ── Memory ────────────────────────────────────────────────────────────────────

struct MemoryMetrics
{
    uint64_t total_bytes = 0;
    uint64_t used_bytes = 0;
    uint64_t free_bytes = 0;
    uint64_t available_bytes = 0;
    uint64_t cached_bytes = 0;
    uint64_t buffers_bytes = 0;
    double usage_percent = 0.0;
    uint64_t swap_total_bytes = 0;
    uint64_t swap_used_bytes = 0;
    uint64_t swap_free_bytes = 0;

    MSGPACK_DEFINE_ARRAY(total_bytes, used_bytes, free_bytes, available_bytes, cached_bytes, buffers_bytes,
                         usage_percent, swap_total_bytes, swap_used_bytes, swap_free_bytes)
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MemoryMetrics, total_bytes, used_bytes, free_bytes, available_bytes, cached_bytes,
                                   buffers_bytes, usage_percent, swap_total_bytes, swap_used_bytes, swap_free_bytes)

// ── Disk ──────────────────────────────────────────────────────────────────────

struct DiskMetrics
{
    std::string device;
    std::string mount_point;
    std::string filesystem;
    uint64_t total_bytes = 0;
    uint64_t used_bytes = 0;
    uint64_t free_bytes = 0;
    double usage_percent = 0.0;
    uint64_t read_bytes_per_sec = 0;
    uint64_t write_bytes_per_sec = 0;

    MSGPACK_DEFINE_ARRAY(device, mount_point, filesystem, total_bytes, used_bytes, free_bytes, usage_percent,
                         read_bytes_per_sec, write_bytes_per_sec)
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(DiskMetrics, device, mount_point, filesystem, total_bytes, used_bytes, free_bytes,
                                   usage_percent, read_bytes_per_sec, write_bytes_per_sec)

// ── Temperature ───────────────────────────────────────────────────────────────

struct TemperatureMetrics
{
    std::string sensor_name;
    std::string sensor_label;
    double temperature_celsius = 0.0;

    MSGPACK_DEFINE_ARRAY(sensor_name, sensor_label, temperature_celsius)
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TemperatureMetrics, sensor_name, sensor_label, temperature_celsius)

// ── Process ───────────────────────────────────────────────────────────────────

struct ProcessInfo
{
    uint32_t pid = 0;
    std::string name;
    std::string status;
    double cpu_percent = 0.0;
    uint64_t memory_rss_bytes = 0;
    uint64_t memory_vms_bytes = 0;
    std::string username;
    int num_threads = 0;

    MSGPACK_DEFINE_ARRAY(pid, name, status, cpu_percent, memory_rss_bytes, memory_vms_bytes, username, num_threads)
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ProcessInfo, pid, name, status, cpu_percent, memory_rss_bytes, memory_vms_bytes,
                                   username, num_threads)

// ── Network ───────────────────────────────────────────────────────────────────

struct NetworkMetrics
{
    std::string interface_name;
    uint64_t bytes_sent_per_sec = 0;
    uint64_t bytes_recv_per_sec = 0;
    uint64_t packets_sent_per_sec = 0;
    uint64_t packets_recv_per_sec = 0;
    uint64_t errors_in = 0;
    uint64_t errors_out = 0;
    uint64_t drops_in = 0;
    uint64_t drops_out = 0;
    std::string ip_address;
    std::string mac_address;
    bool is_up = false;

    MSGPACK_DEFINE_ARRAY(interface_name, bytes_sent_per_sec, bytes_recv_per_sec, packets_sent_per_sec,
                         packets_recv_per_sec, errors_in, errors_out, drops_in, drops_out, ip_address, mac_address,
                         is_up)
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NetworkMetrics, interface_name, bytes_sent_per_sec, bytes_recv_per_sec,
                                   packets_sent_per_sec, packets_recv_per_sec, errors_in, errors_out, drops_in,
                                   drops_out, ip_address, mac_address, is_up)

// ── System (top-level) ────────────────────────────────────────────────────────

struct SystemMetrics
{
    CpuMetrics cpu;
    MemoryMetrics memory;
    std::vector<DiskMetrics> disks;
    std::vector<TemperatureMetrics> temperatures;
    std::vector<ProcessInfo> top_processes;
    std::vector<NetworkMetrics> networks;
    std::string os_name;
    std::string os_version;
    std::string hostname;
    std::string platform; // "linux" | "windows" | "bsd"
    double uptime_seconds = 0.0;

    MSGPACK_DEFINE_ARRAY(cpu, memory, disks, temperatures, top_processes, networks, os_name, os_version, hostname,
                         platform, uptime_seconds)
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SystemMetrics, cpu, memory, disks, temperatures, top_processes, networks, os_name,
                                   os_version, hostname, platform, uptime_seconds)

} // namespace thewatcher
