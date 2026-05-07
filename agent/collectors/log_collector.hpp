#pragma once

#include "collector.hpp"
#include "common/collector_config.hpp"
#include "common/protocol.hpp"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace thewatcher::agent
{

class LogCollector final : public Collector
{
public:
    std::string_view name() const noexcept override
    {
        return "log";
    }

    // update() satisfies the Collector interface but does nothing here;
    // log tailing happens in tick() which is called explicitly by the agent
    // after collect_metrics() so matches can be sent as separate LOG_MATCH frames.
    void update(SystemMetrics& /*metrics*/) override
    {
    }

    void set_configs(std::vector<LogMonitorConfig> configs);

    // Poll each configured file for new lines, apply patterns, accumulate matches.
    void tick();

    // Drain and return all accumulated matches since the last call.
    std::vector<proto::LogMatch> take_matches();

private:
    struct FileState
    {
        int64_t inode = -1; // -1 = file not yet seen
        int64_t offset = 0;
    };

    std::vector<LogMonitorConfig> configs_;
    std::unordered_map<std::string, FileState> file_states_;
    std::vector<proto::LogMatch> pending_;
    std::mutex mutex_;

    void tail_file(const LogMonitorConfig& cfg);
};

} // namespace thewatcher::agent
