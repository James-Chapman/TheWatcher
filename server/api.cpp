#include "api.hpp"

#include "common/SingleLog.hpp"
#include "common/protocol.hpp"

#include <chrono>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace thewatcher::server
{

using json = nlohmann::json;

// ── Construction ──────────────────────────────────────────────────────────────

ApiServer::ApiServer(Store& store, ZapHandler& zap, const std::string& host, int port)
    : store_(store), zap_(zap), host_(host), port_(port), http_(std::make_unique<httplib::Server>())
{
    LOG_FUNCTION_TRACE
    LOGF_DEBUG("Constructing API server host=%s port=%d", host_.c_str(), port_);
    setup_routes();
}

ApiServer::~ApiServer()
{
    stop();
}

void ApiServer::start()
{
    LOG_FUNCTION_TRACE
    thread_ = std::thread([this] {
        LOGF_INFO("Listening on %s:%d", host_.c_str(), port_);
        if (!http_->listen(host_.c_str(), port_))
            LOGF_ERROR("Failed to listen on %s:%d", host_.c_str(), port_);
    });
}

void ApiServer::stop()
{
    LOG_FUNCTION_TRACE
    http_->stop();
    if (thread_.joinable())
    {
        thread_.join();
        LOG_DEBUG("API server thread joined");
    }
}

void ApiServer::drain_commands(std::vector<PendingCommand>& out)
{
    std::lock_guard<std::mutex> lk(cmd_mutex_);
    const auto before = out.size();
    while (!cmd_queue_.empty())
    {
        out.push_back(std::move(cmd_queue_.front()));
        cmd_queue_.pop();
    }
    if (out.size() > before)
        LOGF_TRACE("Drained %zu pending API command(s)", out.size() - before);
}

// ── Route helpers ─────────────────────────────────────────────────────────────

namespace
{

    int64_t now_ms()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    json agent_to_json(const AgentRecord& a)
    {
        return {
            {"agent_id",             a.agent_id            },
            {"hostname",             a.hostname            },
            {"platform",             a.platform            },
            {"curve_public_key_z85", a.curve_public_key_z85},
            {"approved",             a.approved            },
            {"rejected",             a.rejected            },
            {"connected",            a.connected           },
            {"maintenance",          a.maintenance         },
            {"collection_interval",  a.collection_interval },
            {"process_limit",        a.process_limit       },
            {"first_seen",           a.first_seen          },
            {"last_seen",            a.last_seen           }
        };
    }

} // namespace

// ── Routes ────────────────────────────────────────────────────────────────────

void ApiServer::setup_routes()
{
    LOG_FUNCTION_TRACE
    http_->set_default_headers({
        {"Access-Control-Allow-Origin", "*"}
    });
    LOG_DEBUG("Default CORS headers configured");

    // ── Agents ────────────────────────────────────────────────────────────────

    // GET /api/agents — list all agents
    http_->Get("/api/agents", [this](const httplib::Request&, httplib::Response& res) {
        try
        {
            auto agents = store_.list_agents();
            LOGF_TRACE("GET /api/agents returned %zu agent(s)", agents.size());
            json arr = json::array();
            for (auto& a : agents)
                arr.push_back(agent_to_json(a));
            res.set_content(arr.dump(), "application/json");
        }
        catch (const std::exception& e)
        {
            LOGF_WARNING("GET /api/agents failed: %s", e.what());
            res.status = 500;
            res.set_content(
                json{
                    {"error", e.what()}
            }
                    .dump(),
                "application/json");
        }
    });

    // POST /api/agents/:id/approve — approve an agent
    http_->Post("/api/agents/:id/approve", [this](const httplib::Request& req, httplib::Response& res) {
        try
        {
            auto id = req.path_params.at("id");
            LOGF_INFO("Approving agent id=%s", id.c_str());
            store_.approve_agent(id);
            auto rec = store_.get_agent(id);
            if (rec)
            {
                zap_.add_key(rec->curve_public_key_z85);
                LOGF_DEBUG("Approved agent key added to ZAP id=%s", id.c_str());
            }
            res.set_content(
                json{
                    {"ok", true}
            }
                    .dump(),
                "application/json");
        }
        catch (const std::exception& e)
        {
            LOGF_WARNING("POST /api/agents/:id/approve failed: %s", e.what());
            res.status = 500;
            res.set_content(
                json{
                    {"error", e.what()}
            }
                    .dump(),
                "application/json");
        }
    });

    // POST /api/agents/:id/reject — reject an agent enrollment
    http_->Post("/api/agents/:id/reject", [this](const httplib::Request& req, httplib::Response& res) {
        try
        {
            auto id = req.path_params.at("id");
            LOGF_INFO("Rejecting agent id=%s", id.c_str());
            auto rec = store_.get_agent(id);
            if (rec)
            {
                zap_.remove_key(rec->curve_public_key_z85);
                LOGF_DEBUG("Rejected agent key removed from ZAP id=%s", id.c_str());
            }
            store_.reject_agent(id);
            res.set_content(
                json{
                    {"ok", true}
            }
                    .dump(),
                "application/json");
        }
        catch (const std::exception& e)
        {
            LOGF_WARNING("POST /api/agents/:id/reject failed: %s", e.what());
            res.status = 500;
            res.set_content(
                json{
                    {"error", e.what()}
            }
                    .dump(),
                "application/json");
        }
    });

    // DELETE /api/agents/:id — remove agent
    http_->Delete("/api/agents/:id", [this](const httplib::Request& req, httplib::Response& res) {
        try
        {
            auto id = req.path_params.at("id");
            LOGF_INFO("Deleting agent id=%s", id.c_str());
            auto rec = store_.get_agent(id);
            if (rec)
            {
                zap_.remove_key(rec->curve_public_key_z85);
                LOGF_DEBUG("Deleted agent key removed from ZAP id=%s", id.c_str());
            }
            store_.delete_agent(id);
            res.set_content(
                json{
                    {"ok", true}
            }
                    .dump(),
                "application/json");
        }
        catch (const std::exception& e)
        {
            LOGF_WARNING("DELETE /api/agents/:id failed: %s", e.what());
            res.status = 500;
            res.set_content(
                json{
                    {"error", e.what()}
            }
                    .dump(),
                "application/json");
        }
    });

    // ── Metrics ───────────────────────────────────────────────────────────────

    // GET /api/metrics — latest snapshot for every agent
    http_->Get("/api/metrics", [this](const httplib::Request&, httplib::Response& res) {
        try
        {
            auto rows = store_.latest_metrics();
            LOGF_TRACE("GET /api/metrics returned %zu latest row(s)", rows.size());
            json arr = json::array();
            for (auto& r : rows)
            {
                arr.push_back({
                    {"agent_id",     r.agent_id                 },
                    {"timestamp_ms", r.timestamp_ms             },
                    {"metrics",      json::parse(r.metrics_json)}
                });
            }
            res.set_content(arr.dump(), "application/json");
        }
        catch (const std::exception& e)
        {
            LOGF_WARNING("GET /api/metrics failed: %s", e.what());
            res.status = 500;
            res.set_content(
                json{
                    {"error", e.what()}
            }
                    .dump(),
                "application/json");
        }
    });

    // GET /api/metrics/:id?limit=N — history for one agent
    http_->Get("/api/metrics/:id", [this](const httplib::Request& req, httplib::Response& res) {
        try
        {
            auto id = req.path_params.at("id");
            int limit = 100;
            if (req.has_param("limit"))
                limit = std::stoi(req.get_param_value("limit"));
            auto rows = store_.get_metrics(id, limit);
            LOGF_TRACE("GET /api/metrics/%s returned %zu row(s) with limit=%d", id.c_str(), rows.size(), limit);
            json arr = json::array();
            for (auto& r : rows)
            {
                arr.push_back({
                    {"agent_id",     r.agent_id                 },
                    {"timestamp_ms", r.timestamp_ms             },
                    {"metrics",      json::parse(r.metrics_json)}
                });
            }
            res.set_content(arr.dump(), "application/json");
        }
        catch (const std::exception& e)
        {
            LOGF_WARNING("GET /api/metrics/:id failed: %s", e.what());
            res.status = 500;
            res.set_content(
                json{
                    {"error", e.what()}
            }
                    .dump(),
                "application/json");
        }
    });

    // ── Commands ──────────────────────────────────────────────────────────────

    // POST /api/agents/:id/command
    // Body: { "command_type": <int>, "args": <base64 or omit> }
    http_->Post("/api/agents/:id/command", [this](const httplib::Request& req, httplib::Response& res) {
        try
        {
            auto id = req.path_params.at("id");
            auto body = json::parse(req.body);

            CommandMessage cmd;
            cmd.command_id = std::to_string(static_cast<uint32_t>(now_ms() & 0xFFFFFFFF));
            cmd.command_type = body.at("command_type").get<uint8_t>();
            // args is optional raw bytes packed by caller; default empty
            if (body.contains("args"))
                cmd.args = body["args"].get<std::vector<uint8_t>>();

            {
                std::lock_guard<std::mutex> lk(cmd_mutex_);
                cmd_queue_.push({id, cmd});
            }
            LOGF_DEBUG("Queued command id=%s agent=%s type=%u args_size=%zu", cmd.command_id.c_str(), id.c_str(),
                       static_cast<unsigned>(cmd.command_type), cmd.args.size());
            res.set_content(
                json{
                    {"ok",         true          },
                    {"command_id", cmd.command_id}
            }
                    .dump(),
                "application/json");
        }
        catch (const std::exception& e)
        {
            LOGF_WARNING("POST /api/agents/:id/command failed: %s", e.what());
            res.status = 400;
            res.set_content(
                json{
                    {"error", e.what()}
            }
                    .dump(),
                "application/json");
        }
    });

    // POST /api/agents/:id/set_interval
    http_->Post("/api/agents/:id/set_interval", [this](const httplib::Request& req, httplib::Response& res) {
        try
        {
            auto id = req.path_params.at("id");
            auto body = json::parse(req.body);
            int secs = body.at("interval_seconds").get<int>();
            if (secs <= 0)
                throw std::runtime_error("interval_seconds must be greater than zero");
            LOGF_INFO("Setting collection interval id=%s interval_seconds=%d", id.c_str(), secs);

            auto rec = store_.get_agent(id);
            if (rec)
            {
                rec->collection_interval = secs;
                store_.upsert_agent(*rec);
            }

            SetIntervalArgs args{secs};
            CommandMessage cmd;
            cmd.command_id = std::to_string(static_cast<uint32_t>(now_ms() & 0xFFFFFFFF));
            cmd.command_type = static_cast<uint8_t>(CommandType::SET_INTERVAL);
            cmd.args = proto::pack(args);

            {
                std::lock_guard<std::mutex> lk(cmd_mutex_);
                cmd_queue_.push({id, cmd});
            }
            LOGF_DEBUG("Queued SET_INTERVAL command id=%s agent=%s interval_seconds=%d", cmd.command_id.c_str(),
                       id.c_str(), secs);
            res.set_content(
                json{
                    {"ok", true}
            }
                    .dump(),
                "application/json");
        }
        catch (const std::exception& e)
        {
            LOGF_WARNING("POST /api/agents/:id/set_interval failed: %s", e.what());
            res.status = 400;
            res.set_content(
                json{
                    {"error", e.what()}
            }
                    .dump(),
                "application/json");
        }
    });

    // POST /api/agents/:id/set_process_limit
    http_->Post("/api/agents/:id/set_process_limit", [this](const httplib::Request& req, httplib::Response& res) {
        try
        {
            auto id = req.path_params.at("id");
            auto body = json::parse(req.body);
            int limit = body.at("limit").get<int>();
            if (limit <= 0)
                throw std::runtime_error("limit must be greater than zero");
            LOGF_INFO("Setting process limit id=%s limit=%d", id.c_str(), limit);

            auto rec = store_.get_agent(id);
            if (rec)
            {
                rec->process_limit = limit;
                store_.upsert_agent(*rec);
            }

            SetProcessLimitArgs args{limit};
            CommandMessage cmd;
            cmd.command_id = std::to_string(static_cast<uint32_t>(now_ms() & 0xFFFFFFFF));
            cmd.command_type = static_cast<uint8_t>(CommandType::SET_PROCESS_LIMIT);
            cmd.args = proto::pack(args);

            {
                std::lock_guard<std::mutex> lk(cmd_mutex_);
                cmd_queue_.push({id, cmd});
            }
            LOGF_DEBUG("Queued SET_PROCESS_LIMIT command id=%s agent=%s limit=%d", cmd.command_id.c_str(), id.c_str(),
                       limit);
            res.set_content(
                json{
                    {"ok", true}
            }
                    .dump(),
                "application/json");
        }
        catch (const std::exception& e)
        {
            LOGF_WARNING("POST /api/agents/:id/set_process_limit failed: %s", e.what());
            res.status = 400;
            res.set_content(
                json{
                    {"error", e.what()}
            }
                    .dump(),
                "application/json");
        }
    });

    // Simple action commands (pause, resume, restart_collectors, disconnect, get_status)
    auto simple_cmd = [this](CommandType ct) {
        return [this, ct](const httplib::Request& req, httplib::Response& res) {
            try
            {
                auto id = req.path_params.at("id");
                CommandMessage cmd;
                cmd.command_id = std::to_string(static_cast<uint32_t>(now_ms() & 0xFFFFFFFF));
                cmd.command_type = static_cast<uint8_t>(ct);
                {
                    std::lock_guard<std::mutex> lk(cmd_mutex_);
                    cmd_queue_.push({id, cmd});
                }
                LOGF_DEBUG("Queued simple command id=%s agent=%s type=%u", cmd.command_id.c_str(), id.c_str(),
                           static_cast<unsigned>(cmd.command_type));
                res.set_content(
                    json{
                        {"ok", true}
                }
                        .dump(),
                    "application/json");
            }
            catch (const std::exception& e)
            {
                LOGF_WARNING("POST simple agent command failed: %s", e.what());
                res.status = 400;
                res.set_content(
                    json{
                        {"error", e.what()}
                }
                        .dump(),
                    "application/json");
            }
        };
    };

    http_->Post("/api/agents/:id/pause", simple_cmd(CommandType::PAUSE));
    http_->Post("/api/agents/:id/resume", simple_cmd(CommandType::RESUME));
    http_->Post("/api/agents/:id/restart_collectors", simple_cmd(CommandType::RESTART_COLLECTORS));
    http_->Post("/api/agents/:id/disconnect", simple_cmd(CommandType::DISCONNECT));
    http_->Post("/api/agents/:id/get_status", simple_cmd(CommandType::GET_STATUS));
}

} // namespace thewatcher::server
