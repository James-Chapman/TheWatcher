#include "log_collector.hpp"

#include "common/SingleLog.hpp"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <regex>
#include <string>
#include <sys/stat.h>

namespace thewatcher::agent
{

void LogCollector::set_configs(std::vector<LogMonitorConfig> configs)
{
    std::lock_guard<std::mutex> lock(mutex_);
    configs_ = std::move(configs);
    // Remove file states for paths that are no longer configured.
    for (auto it = file_states_.begin(); it != file_states_.end();)
    {
        bool found = false;
        for (const auto& cfg : configs_)
        {
            if (cfg.path == it->first)
            {
                found = true;
                break;
            }
        }
        it = found ? std::next(it) : file_states_.erase(it);
    }
}

void LogCollector::tick()
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& cfg : configs_)
    {
        if (!cfg.enabled || cfg.path.empty() || cfg.pattern.empty() || cfg.indicator_name.empty())
            continue;
        try
        {
            tail_file(cfg);
        }
        catch (const std::exception& e)
        {
            LOGF_WARNING("LogCollector tail_file path=%s error=%s", cfg.path.c_str(), e.what());
        }
    }
}

std::vector<proto::LogMatch> LogCollector::take_matches()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return std::exchange(pending_, {});
}

void LogCollector::tail_file(const LogMonitorConfig& cfg)
{
    struct stat st {};
    if (::stat(cfg.path.c_str(), &st) != 0)
    {
        if (errno != ENOENT)
            LOGF_WARNING("LogCollector stat path=%s error=%s", cfg.path.c_str(), std::strerror(errno));
        // File doesn't exist yet; reset state so we start from byte 0 when it appears.
        file_states_.erase(cfg.path);
        return;
    }

    const auto inode = static_cast<int64_t>(st.st_ino);
    auto& state = file_states_[cfg.path];

    // Detect rotation: inode changed or file is shorter than our saved offset.
    if (state.inode != inode || static_cast<int64_t>(st.st_size) < state.offset)
    {
        LOGF_DEBUG("LogCollector rotation detected path=%s", cfg.path.c_str());
        state.inode = inode;
        state.offset = 0;
    }

    if (state.offset >= static_cast<int64_t>(st.st_size))
        return; // nothing new

    std::ifstream file(cfg.path, std::ios::binary);
    if (!file.is_open())
    {
        LOGF_WARNING("LogCollector open failed path=%s", cfg.path.c_str());
        return;
    }

    file.seekg(state.offset);

    std::regex pattern;
    try
    {
        pattern = std::regex(cfg.pattern, std::regex::ECMAScript | std::regex::optimize);
    }
    catch (const std::regex_error& e)
    {
        LOGF_WARNING("LogCollector invalid regex pattern=%s error=%s", cfg.pattern.c_str(), e.what());
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        if (std::regex_search(line, pattern))
        {
            proto::LogMatch m;
            m.indicator_name = cfg.indicator_name;
            m.path = cfg.path;
            m.matched_line = line;
            m.severity = cfg.severity;
            LOGF_DEBUG("LogCollector match indicator=%s path=%s line=%.80s", cfg.indicator_name.c_str(),
                       cfg.path.c_str(), line.c_str());
            pending_.push_back(std::move(m));
        }
    }

    // After getline reaches EOF the stream sets eofbit, causing tellg() to return -1.
    // Clear error state so we get the real file position; fall back to st_size if needed.
    file.clear();
    const auto pos = file.tellg();
    state.offset = (pos >= 0) ? static_cast<int64_t>(pos) : static_cast<int64_t>(st.st_size);
}

} // namespace thewatcher::agent
