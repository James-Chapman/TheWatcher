#include "config.hpp"

#include "common/SingleLog.hpp"
#include "common/crypto.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

#ifdef _WIN32
#include <shlobj.h>
#include <windows.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

namespace thewatcher::server
{

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{
    std::string resolve_sqlite_db_path(const fs::path& config_path, const std::string& db_path)
    {
        fs::path raw = db_path.empty() ? fs::path("thewatcher.db") : fs::path(db_path);
        if (raw.is_absolute())
            return raw.lexically_normal().string();

        const auto base = config_path.has_parent_path() ? config_path.parent_path() : fs::current_path();
        return (base / raw).lexically_normal().string();
    }

    void resolve_runtime_paths(ServerConfig& cfg, const fs::path& config_path)
    {
        if (cfg.db_type == "sqlite")
            cfg.db_path = resolve_sqlite_db_path(config_path, cfg.db_path);
    }
} // namespace

void to_json(json& j, const ServerConfig& cfg)
{
    j = json{
        {"bind_address",          cfg.bind_address         },
        {"enrollment_address",    cfg.enrollment_address   },
        {"api_host",              cfg.api_host             },
        {"api_port",              cfg.api_port             },
        {"db_path",               cfg.db_path              },
        {"db_type",               cfg.db_type              },
        {"postgres_dsn",          cfg.postgres_dsn         },
        {"offline_after_seconds", cfg.offline_after_seconds},
        {"server_public_key",     cfg.server_public_key    },
        {"server_secret_key",     cfg.server_secret_key    }
    };
}

void from_json(const json& j, ServerConfig& cfg)
{
    cfg.bind_address = j.value("bind_address", cfg.bind_address);
    cfg.enrollment_address = j.value("enrollment_address", cfg.enrollment_address);
    cfg.api_host = j.value("api_host", cfg.api_host);
    cfg.api_port = j.value("api_port", cfg.api_port);
    cfg.db_path = j.value("db_path", cfg.db_path);
    cfg.db_type = j.value("db_type", cfg.db_type);
    cfg.postgres_dsn = j.value("postgres_dsn", cfg.postgres_dsn);
    cfg.offline_after_seconds = j.value("offline_after_seconds", cfg.offline_after_seconds);
    cfg.server_public_key = j.value("server_public_key", cfg.server_public_key);
    cfg.server_secret_key = j.value("server_secret_key", cfg.server_secret_key);
}

fs::path ServerConfig::default_path()
{
    LOG_FUNCTION_TRACE
#ifdef _WIN32
    char buf[MAX_PATH];
    SHGetFolderPathA(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, buf);
    return fs::path(buf) / "TheWatcher" / "server.json";
#else
    return fs::path("/etc/thewatcher/server.json");
#endif
}

ServerConfig ServerConfig::load_or_create(const fs::path& path)
{
    LOG_FUNCTION_TRACE
    LOGF_DEBUG("Loading server config from %s", path.string().c_str());
    if (fs::exists(path))
    {
        LOG_DEBUG("Server config exists");
        std::ifstream f(path);
        if (!f)
            throw std::runtime_error("Cannot open " + path.string());
        json j;
        f >> j;
        auto cfg = j.get<ServerConfig>();
        resolve_runtime_paths(cfg, path);
        LOGF_DEBUG("Loaded server config bind=%s enrollment=%s api=%s:%d db_type=%s db_path=%s offline_after=%d",
                   cfg.bind_address.c_str(), cfg.enrollment_address.c_str(), cfg.api_host.c_str(), cfg.api_port,
                   cfg.db_type.c_str(), cfg.db_path.c_str(), cfg.offline_after_seconds);
        return cfg;
    }

    LOG_INFO("Server config does not exist; creating a new config with generated CURVE keys");
    ServerConfig cfg;
    thewatcher::crypto::init();
    auto kp = thewatcher::crypto::generate_curve_keypair();
    cfg.server_public_key = kp.public_key_z85;
    cfg.server_secret_key = kp.secret_key_z85;
    resolve_runtime_paths(cfg, path);

    if (path.has_parent_path())
        fs::create_directories(path.parent_path());
    cfg.save(path);
    return cfg;
}

void ServerConfig::save(const fs::path& path) const
{
    LOG_FUNCTION_TRACE
    LOGF_DEBUG("Saving server config to %s", path.string().c_str());
    std::ofstream f(path);
    if (!f)
        throw std::runtime_error("Cannot write " + path.string());
    json j = *this;
    f << j.dump(4) << "\n";
}

} // namespace thewatcher::server
