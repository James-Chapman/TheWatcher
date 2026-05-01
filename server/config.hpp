#pragma once

#include <filesystem>
#include <string>

namespace thewatcher::server
{

struct ServerConfig
{
    std::string bind_address = "tcp://*:5555";
    std::string enrollment_address = "tcp://*:5556";
    std::string api_host = "0.0.0.0";
    int api_port = 8080;
    std::string db_path = "thewatcher.db";
    std::string db_type = "sqlite"; // "sqlite" or "postgres"
    std::string postgres_dsn;       // only used when db_type == "postgres"
    int offline_after_seconds = 60;

    // CURVE keypair (z85-encoded). Empty = disable encryption.
    std::string server_public_key;
    std::string server_secret_key;

    static std::filesystem::path default_path();
    static ServerConfig load_or_create(const std::filesystem::path& path);
    void save(const std::filesystem::path& path) const;
};

} // namespace thewatcher::server
