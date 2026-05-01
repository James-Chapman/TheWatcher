#include "api.hpp"

#include "common/SingleLog.hpp"
#include "common/protocol.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <sodium.h>
#include <stdexcept>
#include <unordered_set>

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
            {"agent_id",             a.agent_id                          },
            {"hostname",             a.hostname                          },
            {"platform",             a.platform                          },
            {"curve_public_key_z85", a.curve_public_key_z85              },
            {"approved",             a.approved                          },
            {"rejected",             a.rejected                          },
            {"connected",            a.connected                         },
            {"maintenance",          a.maintenance                       },
            {"maintenance_reason",   a.maintenance_reason                },
            {"maintenance_until",    a.maintenance_until                 },
            {"collection_interval",  a.collection_interval               },
            {"process_limit",        a.process_limit                     },
            {"first_seen",           a.first_seen                        },
            {"last_seen",            a.last_seen                         },
            {"thresholds",
             {{"cpu",
               {{"warning_pct_of_avg", a.cpu_warning_pct_of_avg},
                {"degraded_pct_of_avg", a.cpu_degraded_pct_of_avg},
                {"critical_pct_of_avg", a.cpu_critical_pct_of_avg}}},
              {"memory",
               {{"warning_pct_of_avg", a.memory_warning_pct_of_avg},
                {"degraded_pct_of_avg", a.memory_degraded_pct_of_avg},
                {"critical_pct_of_avg", a.memory_critical_pct_of_avg}}},
              {"disk",
               {{"warning_pct_of_avg", a.disk_warning_pct_of_avg},
                {"degraded_pct_of_avg", a.disk_degraded_pct_of_avg},
                {"critical_pct_of_avg", a.disk_critical_pct_of_avg}}},
              {"network",
               {{"warning_pct_of_avg", a.network_warning_pct_of_avg},
                {"degraded_pct_of_avg", a.network_degraded_pct_of_avg},
                {"critical_pct_of_avg", a.network_critical_pct_of_avg}}}}}
        };
    }

    void validate_thresholds(double warning, double degraded, double critical, const std::string& indicator)
    {
        if (!std::isfinite(warning) || !std::isfinite(degraded) || !std::isfinite(critical))
            throw std::runtime_error(indicator + " thresholds must be finite numbers");
        if (warning <= 0.0 || degraded <= 0.0 || critical <= 0.0)
            throw std::runtime_error(indicator + " thresholds must be greater than zero");
        if (!(warning < degraded && degraded < critical))
            throw std::runtime_error(indicator + " thresholds must be ordered warning < degraded < critical");
    }

    void read_indicator_thresholds(const json& thresholds, const std::string& indicator, double& warning,
                                   double& degraded, double& critical)
    {
        const auto& value = thresholds.at(indicator);
        warning = value.at("warning_pct_of_avg").get<double>();
        degraded = value.at("degraded_pct_of_avg").get<double>();
        critical = value.at("critical_pct_of_avg").get<double>();
        validate_thresholds(warning, degraded, critical, indicator);
    }

    AgentRecord apply_threshold_payload(AgentRecord rec, const json& body)
    {
        const auto& thresholds = body.at("thresholds");
        read_indicator_thresholds(thresholds, "cpu", rec.cpu_warning_pct_of_avg, rec.cpu_degraded_pct_of_avg,
                                  rec.cpu_critical_pct_of_avg);
        read_indicator_thresholds(thresholds, "memory", rec.memory_warning_pct_of_avg, rec.memory_degraded_pct_of_avg,
                                  rec.memory_critical_pct_of_avg);
        read_indicator_thresholds(thresholds, "disk", rec.disk_warning_pct_of_avg, rec.disk_degraded_pct_of_avg,
                                  rec.disk_critical_pct_of_avg);
        read_indicator_thresholds(thresholds, "network", rec.network_warning_pct_of_avg,
                                  rec.network_degraded_pct_of_avg, rec.network_critical_pct_of_avg);
        return rec;
    }

    json group_to_json(const GroupRecord& group)
    {
        return {
            {"group_id", group.group_id},
            {"name",     group.name    },
            {"built_in", group.built_in}
        };
    }

    json user_to_json(const UserRecord& user, const std::vector<int64_t>& group_ids)
    {
        return {
            {"user_id",   user.user_id },
            {"username",  user.username},
            {"role",      user.role    },
            {"built_in",  user.built_in},
            {"disabled",  user.disabled},
            {"group_ids", group_ids    }
        };
    }

    json alert_to_json(const AlertRecord& alert)
    {
        return {
            {"alert_id",        alert.alert_id       },
            {"agent_id",        alert.agent_id       },
            {"indicator",       alert.indicator      },
            {"old_status",      alert.old_status     },
            {"new_status",      alert.new_status     },
            {"message",         alert.message        },
            {"created_at",      alert.created_at     },
            {"acknowledged_by", alert.acknowledged_by},
            {"acknowledged_at", alert.acknowledged_at},
            {"deleted_at",      alert.deleted_at     }
        };
    }

    std::string cookie_value(const httplib::Request& req, const std::string& name)
    {
        auto cookie = req.get_header_value("Cookie");
        const auto needle = name + "=";
        auto pos = cookie.find(needle);
        if (pos == std::string::npos)
            return {};
        pos += needle.size();
        auto end = cookie.find(';', pos);
        return cookie.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    }

    std::string random_token()
    {
        std::array<unsigned char, 32> bytes{};
        randombytes_buf(bytes.data(), bytes.size());
        std::array<char, 65> token{};
        sodium_bin2hex(token.data(), token.size(), bytes.data(), bytes.size());
        return token.data();
    }

    std::string hash_password(const std::string& password)
    {
        char hash[crypto_pwhash_STRBYTES] = {};
        if (crypto_pwhash_str(hash, password.c_str(), password.size(), crypto_pwhash_OPSLIMIT_INTERACTIVE,
                              crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
            throw std::runtime_error("password hashing failed");
        return hash;
    }

    void set_json(httplib::Response& res, const json& body)
    {
        res.set_content(body.dump(), "application/json");
    }

} // namespace

std::optional<SessionRecord> ApiServer::current_session(const httplib::Request& req)
{
    auto token = cookie_value(req, "tw_session");
    if (token.empty())
        return std::nullopt;
    return store_.get_session(token, now_ms());
}

std::optional<SessionRecord> ApiServer::require_role(const httplib::Request& req, httplib::Response& res,
                                                     const std::string& role)
{
    auto role_rank = [](const std::string& value) {
        if (value == "admin")
            return 3;
        if (value == "operator")
            return 2;
        if (value == "viewer")
            return 1;
        return 0;
    };

    auto session = current_session(req);
    if (!session)
    {
        res.status = 401;
        set_json(res, json{
                          {"error", "authentication required"}
        });
        return std::nullopt;
    }
    if (role_rank(session->role) < role_rank(role))
    {
        res.status = 403;
        set_json(res, json{
                          {"error", "permission denied"}
        });
        return std::nullopt;
    }
    return session;
}

bool ApiServer::can_access_agent(const SessionRecord& session, const std::string& agent_id)
{
    if (session.role == "admin")
        return true;

    std::unordered_set<int64_t> user_groups;
    for (auto group_id : store_.get_user_groups(session.user_id))
        user_groups.insert(group_id);
    if (user_groups.empty())
        return false;

    for (auto group_id : store_.get_agent_groups(agent_id))
    {
        if (user_groups.contains(group_id))
            return true;
    }
    return false;
}

std::optional<SessionRecord> ApiServer::require_agent_access(const httplib::Request& req, httplib::Response& res,
                                                             const std::string& role, const std::string& agent_id)
{
    auto session = require_role(req, res, role);
    if (!session)
        return std::nullopt;
    if (!can_access_agent(*session, agent_id))
    {
        res.status = 403;
        set_json(res, json{
                          {"error", "agent is outside the user's groups"}
        });
        return std::nullopt;
    }
    return session;
}

std::string ApiServer::enqueue_simple_command(const std::string& id, CommandType ct)
{
    CommandMessage cmd;
    cmd.command_id = std::to_string(static_cast<uint32_t>(now_ms() & 0xFFFFFFFF));
    cmd.command_type = static_cast<uint8_t>(ct);
    std::lock_guard<std::mutex> lk(cmd_mutex_);
    cmd_queue_.push({id, cmd});
    return cmd.command_id;
}

// ── Routes ────────────────────────────────────────────────────────────────────

void ApiServer::setup_routes()
{
    LOG_FUNCTION_TRACE
    http_->set_default_headers({
        {"Access-Control-Allow-Origin",      "*"                      },
        {"Access-Control-Allow-Credentials", "true"                   },
        {"Access-Control-Allow-Headers",     "Content-Type"           },
        {"Access-Control-Allow-Methods",     "GET,POST,DELETE,OPTIONS"}
    });
    LOG_DEBUG("Default CORS headers configured");

    http_->Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    http_->Post("/api/login", [this](const httplib::Request& req, httplib::Response& res) {
        try
        {
            auto body = json::parse(req.body);
            const auto username = body.at("username").get<std::string>();
            const auto password = body.at("password").get<std::string>();
            auto user = store_.get_user_by_username(username);
            if (!user || user->disabled ||
                crypto_pwhash_str_verify(user->password_hash.c_str(), password.c_str(), password.size()) != 0)
            {
                res.status = 401;
                set_json(res, json{
                                  {"error", "invalid credentials"}
                });
                return;
            }

            const auto now = now_ms();
            SessionRecord session;
            session.token = random_token();
            session.user_id = user->user_id;
            session.username = user->username;
            session.role = user->role;
            session.created_at = now;
            session.expires_at = now + (8LL * 60LL * 60LL * 1000LL);
            store_.create_session(session);
            res.set_header("Set-Cookie",
                           "tw_session=" + session.token + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=28800");
            set_json(res, json{
                              {"username", user->username},
                              {"role",     user->role    }
            });
        }
        catch (const std::exception& e)
        {
            res.status = 400;
            set_json(res, json{
                              {"error", e.what()}
            });
        }
    });

    http_->Post("/api/logout", [this](const httplib::Request& req, httplib::Response& res) {
        auto token = cookie_value(req, "tw_session");
        if (!token.empty())
            store_.delete_session(token);
        res.set_header("Set-Cookie", "tw_session=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
        set_json(res, json{
                          {"ok", true}
        });
    });

    http_->Get("/api/session", [this](const httplib::Request& req, httplib::Response& res) {
        auto session = current_session(req);
        if (!session)
        {
            res.status = 401;
            set_json(res, json{
                              {"error", "authentication required"}
            });
            return;
        }
        set_json(res, json{
                          {"username", session->username},
                          {"role",     session->role    }
        });
    });

    // ── Agents ────────────────────────────────────────────────────────────────

    http_->Get("/api/groups", [this](const httplib::Request& req, httplib::Response& res) {
        if (!require_role(req, res, "viewer"))
            return;
        auto groups = store_.list_groups();
        json arr = json::array();
        for (const auto& group : groups)
            arr.push_back(group_to_json(group));
        set_json(res, arr);
    });

    http_->Post("/api/groups", [this](const httplib::Request& req, httplib::Response& res) {
        if (!require_role(req, res, "admin"))
            return;
        try
        {
            auto body = json::parse(req.body);
            auto group_id = store_.create_group(body.at("name").get<std::string>());
            set_json(res, json{
                              {"group_id", group_id}
            });
        }
        catch (const std::exception& e)
        {
            res.status = 400;
            set_json(res, json{
                              {"error", e.what()}
            });
        }
    });

    http_->Get("/api/users", [this](const httplib::Request& req, httplib::Response& res) {
        if (!require_role(req, res, "admin"))
            return;
        auto users = store_.list_users();
        json arr = json::array();
        for (const auto& user : users)
            arr.push_back(user_to_json(user, store_.get_user_groups(user.user_id)));
        set_json(res, arr);
    });

    http_->Post("/api/users", [this](const httplib::Request& req, httplib::Response& res) {
        if (!require_role(req, res, "admin"))
            return;
        try
        {
            auto body = json::parse(req.body);
            const auto username = body.at("username").get<std::string>();
            const auto password = body.at("password").get<std::string>();
            const auto role = body.value("role", "viewer");
            if (username.empty() || password.empty())
                throw std::runtime_error("username and password are required");
            if (role != "admin" && role != "operator" && role != "viewer")
                throw std::runtime_error("role must be admin, operator, or viewer");

            std::vector<int64_t> group_ids;
            if (body.contains("group_ids"))
                group_ids = body.at("group_ids").get<std::vector<int64_t>>();

            const auto user_id = store_.create_user(username, hash_password(password), role);
            store_.set_user_groups(user_id, group_ids);
            LOGF_INFO("Created user username=%s role=%s group_count=%zu", username.c_str(), role.c_str(),
                      group_ids.size());
            set_json(res, json{
                              {"user_id", user_id}
            });
        }
        catch (const std::exception& e)
        {
            res.status = 400;
            set_json(res, json{
                              {"error", e.what()}
            });
        }
    });

    http_->Get("/api/alerts", [this](const httplib::Request& req, httplib::Response& res) {
        auto session = require_role(req, res, "viewer");
        if (!session)
            return;
        auto alerts = store_.list_alerts(false);
        json arr = json::array();
        for (const auto& alert : alerts)
        {
            if (can_access_agent(*session, alert.agent_id))
                arr.push_back(alert_to_json(alert));
        }
        set_json(res, arr);
    });

    http_->Get("/api/alerts/unacknowledged", [this](const httplib::Request& req, httplib::Response& res) {
        auto session = require_role(req, res, "viewer");
        if (!session)
            return;
        auto alerts = store_.list_unacknowledged_alerts();
        json arr = json::array();
        for (const auto& alert : alerts)
        {
            if (can_access_agent(*session, alert.agent_id))
                arr.push_back(alert_to_json(alert));
        }
        set_json(res, arr);
    });

    http_->Post("/api/alerts/:id/ack", [this](const httplib::Request& req, httplib::Response& res) {
        auto session = require_role(req, res, "operator");
        if (!session)
            return;
        const auto alert_id = std::stoll(req.path_params.at("id"));
        auto allowed = false;
        for (const auto& alert : store_.list_alerts(false))
            allowed = allowed || (alert.alert_id == alert_id && can_access_agent(*session, alert.agent_id));
        if (!allowed)
        {
            res.status = 403;
            set_json(res, json{
                              {"error", "alert is outside the user's groups"}
            });
            return;
        }
        store_.acknowledge_alert(alert_id, session->username, now_ms());
        set_json(res, json{
                          {"ok", true}
        });
    });

    http_->Delete("/api/alerts/:id", [this](const httplib::Request& req, httplib::Response& res) {
        auto session = require_role(req, res, "operator");
        if (!session)
            return;
        const auto alert_id = std::stoll(req.path_params.at("id"));
        auto allowed = false;
        for (const auto& alert : store_.list_alerts(false))
            allowed = allowed || (alert.alert_id == alert_id && can_access_agent(*session, alert.agent_id));
        if (!allowed)
        {
            res.status = 403;
            set_json(res, json{
                              {"error", "alert is outside the user's groups"}
            });
            return;
        }
        store_.soft_delete_alert(alert_id, now_ms());
        set_json(res, json{
                          {"ok", true}
        });
    });

    http_->Get("/api/pending-enrollments", [this](const httplib::Request& req, httplib::Response& res) {
        if (!require_role(req, res, "admin"))
            return;
        auto agents = store_.list_pending_agents();
        json arr = json::array();
        for (auto& a : agents)
            arr.push_back(agent_to_json(a));
        set_json(res, arr);
    });

    // GET /api/agents — list approved agents
    http_->Get("/api/agents", [this](const httplib::Request& req, httplib::Response& res) {
        auto session = require_role(req, res, "viewer");
        if (!session)
            return;
        try
        {
            auto agents = store_.list_approved_agents();
            LOGF_TRACE("GET /api/agents returned %zu agent(s)", agents.size());
            json arr = json::array();
            for (auto& a : agents)
            {
                if (!can_access_agent(*session, a.agent_id))
                    continue;
                auto item = agent_to_json(a);
                item["group_ids"] = store_.get_agent_groups(a.agent_id);
                arr.push_back(item);
            }
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
        if (!require_role(req, res, "admin"))
            return;
        try
        {
            auto id = req.path_params.at("id");
            LOGF_INFO("Approving agent id=%s", id.c_str());
            store_.approve_agent(id);
            std::vector<int64_t> group_ids;
            if (!req.body.empty())
            {
                auto body = json::parse(req.body);
                if (body.contains("group_ids"))
                    group_ids = body["group_ids"].get<std::vector<int64_t>>();
            }
            if (group_ids.empty())
            {
                auto groups = store_.list_groups();
                for (const auto& group : groups)
                {
                    if (group.name == "Admins")
                    {
                        group_ids.push_back(group.group_id);
                        break;
                    }
                }
            }
            store_.set_agent_groups(id, group_ids);
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
        if (!require_role(req, res, "admin"))
            return;
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
    http_->Post("/api/agents/:id/groups", [this](const httplib::Request& req, httplib::Response& res) {
        if (!require_role(req, res, "admin"))
            return;
        try
        {
            auto id = req.path_params.at("id");
            auto body = json::parse(req.body);
            auto group_ids = body.at("group_ids").get<std::vector<int64_t>>();
            store_.set_agent_groups(id, group_ids);
            LOGF_INFO("Updated agent groups id=%s group_count=%zu", id.c_str(), group_ids.size());
            set_json(res, json{
                              {"ok", true}
            });
        }
        catch (const std::exception& e)
        {
            LOGF_WARNING("POST /api/agents/:id/groups failed: %s", e.what());
            res.status = 400;
            set_json(res, json{
                              {"error", e.what()}
            });
        }
    });

    http_->Delete("/api/agents/:id", [this](const httplib::Request& req, httplib::Response& res) {
        try
        {
            auto id = req.path_params.at("id");
            if (!require_agent_access(req, res, "operator", id))
                return;
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
    http_->Get("/api/metrics", [this](const httplib::Request& req, httplib::Response& res) {
        auto session = require_role(req, res, "viewer");
        if (!session)
            return;
        try
        {
            auto rows = store_.latest_metrics();
            LOGF_TRACE("GET /api/metrics returned %zu latest row(s)", rows.size());
            json arr = json::array();
            for (auto& r : rows)
            {
                if (!can_access_agent(*session, r.agent_id))
                    continue;
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
            if (!require_agent_access(req, res, "viewer", id))
                return;
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
            if (!require_agent_access(req, res, "operator", id))
                return;
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
            if (!require_agent_access(req, res, "operator", id))
                return;
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
            if (!require_agent_access(req, res, "operator", id))
                return;
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

    http_->Post("/api/agents/:id/thresholds", [this](const httplib::Request& req, httplib::Response& res) {
        try
        {
            auto id = req.path_params.at("id");
            if (!require_agent_access(req, res, "operator", id))
                return;

            auto rec = store_.get_agent(id);
            if (!rec)
            {
                res.status = 404;
                set_json(res, json{
                                  {"error", "agent not found"}
                });
                return;
            }

            auto body = json::parse(req.body);
            auto updated = apply_threshold_payload(*rec, body);
            store_.set_agent_thresholds(updated);
            LOGF_INFO("Updated per-agent thresholds id=%s", id.c_str());
            set_json(res, json{
                              {"ok", true}
            });
        }
        catch (const std::exception& e)
        {
            LOGF_WARNING("POST /api/agents/:id/thresholds failed: %s", e.what());
            res.status = 400;
            set_json(res, json{
                              {"error", e.what()}
            });
        }
    });

    http_->Post("/api/agents/:id/maintenance", [this](const httplib::Request& req, httplib::Response& res) {
        try
        {
            auto id = req.path_params.at("id");
            if (!require_agent_access(req, res, "operator", id))
                return;
            auto body = req.body.empty() ? json::object() : json::parse(req.body);
            const auto reason = body.value("reason", "");
            const auto until = body.value("until_ms", static_cast<int64_t>(0));
            store_.set_agent_maintenance(id, true, reason, until);
            store_.clear_active_alerts_for_agent(id, now_ms());
            enqueue_simple_command(id, CommandType::PAUSE);
            set_json(res, json{
                              {"ok", true}
            });
        }
        catch (const std::exception& e)
        {
            res.status = 400;
            set_json(res, json{
                              {"error", e.what()}
            });
        }
    });

    // Simple action commands (pause, resume, restart_collectors, disconnect, get_status)
    auto simple_cmd = [this](CommandType ct) {
        return [this, ct](const httplib::Request& req, httplib::Response& res) {
            try
            {
                auto id = req.path_params.at("id");
                if (!require_agent_access(req, res, "operator", id))
                    return;
                if (ct == CommandType::PAUSE)
                {
                    store_.set_agent_maintenance(id, true, "command", 0);
                    store_.clear_active_alerts_for_agent(id, now_ms());
                }
                else if (ct == CommandType::RESUME)
                {
                    store_.set_agent_maintenance(id, false, "", 0);
                }
                auto command_id = enqueue_simple_command(id, ct);
                LOGF_DEBUG("Queued simple command id=%s agent=%s type=%u", command_id.c_str(), id.c_str(),
                           static_cast<unsigned>(ct));
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
