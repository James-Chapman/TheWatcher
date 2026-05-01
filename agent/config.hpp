#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>

namespace thewatcher::agent
{

struct AgentConfig
{
    std::string agent_id;
    std::string server_address = "tcp://127.0.0.1:5555";
    std::string enrollment_address = "tcp://127.0.0.1:5556";
    std::string server_public_key; // z85, 40 chars — set after enrollment
    std::string agent_public_key;  // z85, 40 chars
    std::string agent_secret_key;  // z85, 40 chars
    int collection_interval = 30;
    int process_limit = 25;

    // Load TheWatcherAgent.conf KEY=VALUE config, creating a new config with
    // fresh identity keys if absent. Existing JSON configs remain readable.
    static AgentConfig load_or_create(const std::filesystem::path& path);
    void save(const std::filesystem::path& path) const;

    static std::filesystem::path default_path();
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AgentConfig, agent_id, server_address, enrollment_address, server_public_key,
                                   agent_public_key, agent_secret_key, collection_interval, process_limit)

} // namespace thewatcher::agent
