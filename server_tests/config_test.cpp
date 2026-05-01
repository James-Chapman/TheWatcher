#include "../server/config.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace thewatcher::server;

namespace
{
std::filesystem::path unique_temp_dir(const char* name)
{
    auto dir = std::filesystem::temp_directory_path() / "thewatcher-tests" / name;
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}
} // namespace

SCENARIO("A first-run server config stores the SQLite database beside the config file")
{
    GIVEN("a config path in a clean directory")
    {
        auto dir = unique_temp_dir("server-config-first-run");
        auto path = dir / "server.json";

        WHEN("the server config is created")
        {
            auto cfg = ServerConfig::load_or_create(path);

            THEN("the db_path is absolute and points under the config directory")
            {
                REQUIRE(std::filesystem::path(cfg.db_path).is_absolute());
                REQUIRE(std::filesystem::path(cfg.db_path) == dir / "thewatcher.db");
            }

            AND_THEN("the persisted config contains the stable absolute database path")
            {
                std::ifstream f(path);
                nlohmann::json j;
                f >> j;
                REQUIRE(j.at("db_path").get<std::string>() == (dir / "thewatcher.db").string());
            }
        }
    }
}

SCENARIO("An existing relative server db_path resolves beside the config file")
{
    GIVEN("an existing config with a relative SQLite database path")
    {
        auto dir = unique_temp_dir("server-config-relative-db");
        auto path = dir / "server.json";
        std::ofstream f(path);
        f << R"({
  "bind_address": "tcp://*:5555",
  "enrollment_address": "tcp://*:5556",
  "api_host": "127.0.0.1",
  "api_port": 8080,
  "db_path": "custom.db",
  "db_type": "sqlite",
  "postgres_dsn": "",
  "offline_after_seconds": 60,
  "server_public_key": "",
  "server_secret_key": ""
})";
        f.close();

        WHEN("the server config is loaded")
        {
            auto cfg = ServerConfig::load_or_create(path);

            THEN("the runtime database path is resolved relative to the config directory")
            {
                REQUIRE(std::filesystem::path(cfg.db_path).is_absolute());
                REQUIRE(std::filesystem::path(cfg.db_path) == dir / "custom.db");
            }
        }
    }
}

SCENARIO("An existing absolute server db_path is preserved")
{
    GIVEN("an existing config with an absolute SQLite database path")
    {
        auto dir = unique_temp_dir("server-config-absolute-db");
        auto path = dir / "server.json";
        auto db_path = dir / "absolute.db";
        std::ofstream f(path);
        f << nlohmann::json{
                 {"bind_address", "tcp://*:5555"},
                 {"enrollment_address", "tcp://*:5556"},
                 {"api_host", "127.0.0.1"},
                 {"api_port", 8080},
                 {"db_path", db_path.string()},
                 {"db_type", "sqlite"},
                 {"postgres_dsn", ""},
                 {"offline_after_seconds", 60},
                 {"server_public_key", ""},
                 {"server_secret_key", ""}
             }.dump(2);
        f.close();

        WHEN("the server config is loaded")
        {
            auto cfg = ServerConfig::load_or_create(path);

            THEN("the runtime database path is unchanged")
            {
                REQUIRE(std::filesystem::path(cfg.db_path) == db_path);
            }
        }
    }
}
