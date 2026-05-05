#include "../agent/agent.hpp"
#include "../server/server.hpp"
#include "../server/startup_signal.hpp"
#include "common/crypto.hpp"
#include "common/protocol.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <zmq.hpp>

using namespace std::chrono_literals;
using json = nlohmann::json;

namespace
{

int64_t now_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string free_zmq_endpoint()
{
    zmq::context_t ctx{1};
    zmq::socket_t socket{ctx, ZMQ_REP};
    socket.bind("tcp://127.0.0.1:*");
    auto endpoint = socket.get(zmq::sockopt::last_endpoint);
    socket.close();
    return endpoint;
}

int free_http_port()
{
    return 20000 + static_cast<int>(now_ms() % 20000);
}

template <typename Predicate>
bool eventually(Predicate predicate, std::chrono::milliseconds timeout, std::chrono::milliseconds interval = 100ms)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (predicate())
            return true;
        std::this_thread::sleep_for(interval);
    }
    return predicate();
}

std::optional<json> get_json(httplib::Client& client, const std::string& path)
{
    auto res = client.Get(path);
    if (!res || res->status != 200)
        return std::nullopt;
    return json::parse(res->body);
}

bool login_as_default_admin(httplib::Client& client)
{
    auto res = client.Post("/api/login", R"({"username":"thewatcher","password":"look_at_me"})", "application/json");
    if (!res || res->status != 200)
        return false;

    const auto set_cookie = res->get_header_value("Set-Cookie");
    const auto end = set_cookie.find(';');
    if (set_cookie.empty() || end == std::string::npos)
        return false;

    client.set_default_headers({
        {"Cookie", set_cookie.substr(0, end)}
    });
    return true;
}

bool post_ok(httplib::Client& client, const std::string& path)
{
    auto res = client.Post(path);
    return res && res->status == 200;
}

std::optional<json> post_json(httplib::Client& client, const std::string& path, const json& body)
{
    auto res = client.Post(path, body.dump(), "application/json");
    if (!res || res->status != 200)
        return std::nullopt;
    return json::parse(res->body);
}

bool put_json_ok(httplib::Client& client, const std::string& path, const json& body)
{
    auto res = client.Put(path, body.dump(), "application/json");
    return res && res->status == 200;
}

bool put_json_error(httplib::Client& client, const std::string& path, const json& body)
{
    auto res = client.Put(path, body.dump(), "application/json");
    return res && res->status == 400;
}

bool delete_ok(httplib::Client& client, const std::string& path)
{
    auto res = client.Delete(path);
    return res && res->status == 200;
}

std::optional<json> find_agent(httplib::Client& client, const std::string& agent_id)
{
    auto body = get_json(client, "/api/agents");
    if (!body)
        return std::nullopt;

    for (const auto& agent : *body)
    {
        if (agent.value("agent_id", "") == agent_id)
            return agent;
    }
    return std::nullopt;
}

std::optional<json> find_pending_agent(httplib::Client& client, const std::string& agent_id)
{
    auto body = get_json(client, "/api/pending-enrollments");
    if (!body)
        return std::nullopt;

    for (const auto& agent : *body)
    {
        if (agent.value("agent_id", "") == agent_id)
            return agent;
    }
    return std::nullopt;
}

std::optional<json> find_metrics(httplib::Client& client, const std::string& agent_id)
{
    auto body = get_json(client, "/api/metrics");
    if (!body)
        return std::nullopt;

    for (const auto& row : *body)
    {
        if (row.value("agent_id", "") == agent_id)
            return row.at("metrics");
    }
    return std::nullopt;
}

thewatcher::proto::EnrollResponse send_enrollment_request(const thewatcher::agent::AgentConfig& agent_config)
{
    zmq::context_t ctx{1};
    zmq::socket_t req{ctx, ZMQ_REQ};
    req.set(zmq::sockopt::linger, 0);
    req.set(zmq::sockopt::rcvtimeo, 2000);
    req.connect(agent_config.enrollment_address);

    thewatcher::proto::EnrollRequest enroll;
    enroll.agent_id = agent_config.agent_id;
    enroll.hostname = "integration-host";
    enroll.platform = "integration-platform";
    enroll.curve_public_key_z85 = agent_config.agent_public_key;

    thewatcher::proto::Frame frame;
    frame.type = static_cast<uint8_t>(thewatcher::proto::FrameType::ENROLL_REQUEST);
    frame.agent_id = agent_config.agent_id;
    frame.timestamp_ms = now_ms();
    frame.payload = thewatcher::proto::pack(enroll);

    const auto encoded = thewatcher::proto::encode_frame(frame);
    req.send(zmq::message_t(encoded.data(), encoded.size()), zmq::send_flags::none);

    zmq::message_t reply;
    auto received = req.recv(reply, zmq::recv_flags::none);
    if (!received)
        throw std::runtime_error("server did not respond to enrollment request");

    const auto reply_frame = thewatcher::proto::decode_frame(reply.data(), reply.size());
    return thewatcher::proto::unpack<thewatcher::proto::EnrollResponse>(reply_frame.payload);
}

struct ServerThreadGuard
{
    thewatcher::server::Server& server;
    std::thread& thread;

    ~ServerThreadGuard()
    {
        server.stop();
        if (thread.joinable())
            thread.join();
    }
};

struct AgentThreadGuard
{
    thewatcher::agent::Agent& agent;
    std::thread& thread;

    ~AgentThreadGuard()
    {
        agent.stop();
        if (thread.joinable())
            thread.join();
    }
};

} // namespace

SCENARIO("A real agent enrolls, sends metrics, disconnects, and the server records the lifecycle")
{
    GIVEN("a server configured with isolated local endpoints and a real agent using generated CURVE keys")
    {
        thewatcher::crypto::init();
        const auto server_keys = thewatcher::crypto::generate_curve_keypair();
        const auto agent_keys = thewatcher::crypto::generate_curve_keypair();
        const std::string agent_id = "integration-agent-" + std::to_string(now_ms());

        const auto db_base =
            std::filesystem::temp_directory_path() / ("thewatcher-integration-" + std::to_string(now_ms()));

        thewatcher::server::ServerConfig server_config;
        server_config.bind_address = free_zmq_endpoint();
        server_config.enrollment_address = free_zmq_endpoint();
        server_config.api_host = "127.0.0.1";
        server_config.api_port = free_http_port();
        server_config.db_path = db_base.string() + ".db";
        server_config.server_public_key = server_keys.public_key_z85;
        server_config.server_secret_key = server_keys.secret_key_z85;

        thewatcher::agent::AgentConfig agent_config;
        agent_config.agent_id = agent_id;
        agent_config.server_address = server_config.bind_address;
        agent_config.enrollment_address = server_config.enrollment_address;
        agent_config.agent_public_key = agent_keys.public_key_z85;
        agent_config.agent_secret_key = agent_keys.secret_key_z85;
        agent_config.collection_interval = 1;
        agent_config.process_limit = 5;

        thewatcher::server::Server server(server_config);
        thewatcher::server::StartupSignal startup;
        std::exception_ptr server_error;

        std::thread server_thread([&] {
            try
            {
                server.run(&startup);
            }
            catch (...)
            {
                server_error = std::current_exception();
                startup.fail(server_error);
            }
        });
        ServerThreadGuard server_guard{server, server_thread};

        const auto startup_result = startup.wait();
        REQUIRE(startup_result.started);
        REQUIRE(server_error == nullptr);

        httplib::Client client("http://127.0.0.1:" + std::to_string(server_config.api_port));
        client.set_connection_timeout(2, 0);
        client.set_read_timeout(2, 0);

        WHEN("the agent enrollment is approved through the server API")
        {
            REQUIRE(eventually(
                [&] {
                    return login_as_default_admin(client);
                },
                5s));

            REQUIRE(eventually(
                [&] {
                    return get_json(client, "/api/agents").has_value();
                },
                5s));

            const auto pending_response = send_enrollment_request(agent_config);
            REQUIRE_FALSE(pending_response.approved);
            REQUIRE(pending_response.message == "pending approval");

            REQUIRE(eventually(
                [&] {
                    const auto pending = find_pending_agent(client, agent_id);
                    return pending && !pending->value("approved", true) && !pending->value("rejected", true);
                },
                5s));

            REQUIRE(post_ok(client, "/api/agents/" + agent_id + "/approve"));
            const auto approved_response = send_enrollment_request(agent_config);
            REQUIRE(approved_response.approved);
            REQUIRE(approved_response.message == "approved");
            REQUIRE(approved_response.server_public_key_z85 == server_config.server_public_key);
            REQUIRE(approved_response.server_public_key_fingerprint ==
                    thewatcher::crypto::server_public_key_fingerprint(server_config.server_public_key));

            REQUIRE(eventually(
                [&] {
                    const auto approved = find_agent(client, agent_id);
                    return approved && approved->value("approved", false);
                },
                5s));

            thewatcher::agent::Agent agent(agent_config);
            std::thread agent_thread([&] {
                agent.start();
            });
            AgentThreadGuard agent_guard{agent, agent_thread};

            if (agent_thread.joinable())
                agent_thread.join();

            THEN("the server receives collector data and records the agent disconnect")
            {
                std::optional<json> metrics;
                REQUIRE(eventually(
                    [&] {
                        metrics = find_metrics(client, agent_id);
                        return metrics.has_value();
                    },
                    15s, 200ms));

                REQUIRE(metrics->contains("cpu"));
                REQUIRE(metrics->contains("memory"));
                REQUIRE(metrics->contains("disks"));
                REQUIRE(metrics->contains("temperatures"));
                REQUIRE(metrics->contains("top_processes"));
                REQUIRE(metrics->contains("networks"));
                REQUIRE(metrics->contains("os_name"));
                REQUIRE(metrics->contains("os_version"));
                REQUIRE(metrics->contains("hostname"));
                REQUIRE(metrics->contains("platform"));
                REQUIRE(metrics->contains("uptime_seconds"));
                REQUIRE(metrics->at("memory").value("total_bytes", 0ULL) > 0);
                REQUIRE_FALSE(metrics->at("disks").empty());

                REQUIRE(eventually(
                    [&] {
                        const auto connected_agent = find_agent(client, agent_id);
                        return connected_agent && connected_agent->value("connected", false);
                    },
                    5s, 100ms));

                REQUIRE(post_ok(client, "/api/agents/" + agent_id + "/disconnect"));

                REQUIRE(eventually(
                    [&] {
                        const auto disconnected_agent = find_agent(client, agent_id);
                        return disconnected_agent && !disconnected_agent->value("connected", true);
                    },
                    5s, 100ms));

                // Status history is recorded as the agent transitions through states
                REQUIRE(eventually(
                    [&] {
                        const auto history = get_json(client, "/api/agents/" + agent_id + "/history?limit=20");
                        return history.has_value() && !history->empty();
                    },
                    5s, 200ms));

                const auto history = get_json(client, "/api/agents/" + agent_id + "/history?limit=20");
                REQUIRE(history.has_value());
                REQUIRE_FALSE(history->empty());

                // Each row must carry all required fields
                for (const auto& row : *history)
                {
                    REQUIRE(row.contains("id"));
                    REQUIRE(row.contains("agent_id"));
                    REQUIRE(row.contains("indicator"));
                    REQUIRE(row.contains("old_status"));
                    REQUIRE(row.contains("new_status"));
                    REQUIRE(row.contains("message"));
                    REQUIRE(row.contains("created_at"));
                    REQUIRE(row["agent_id"].get<std::string>() == agent_id);
                    REQUIRE(row["created_at"].get<int64_t>() > 0);
                }

                // The limit parameter is respected
                const auto limited = get_json(client, "/api/agents/" + agent_id + "/history?limit=1");
                REQUIRE(limited.has_value());
                REQUIRE(limited->size() <= 1);
            }
        }

        server.stop();
        if (server_thread.joinable())
            server_thread.join();

        std::error_code ec;
        std::filesystem::remove(server_config.db_path, ec);
        std::filesystem::remove(server_config.db_path + "-shm", ec);
        std::filesystem::remove(server_config.db_path + "-wal", ec);
    }
}

// ── Helpers shared by the API-only scenarios ───────────────────────────────────

namespace
{

struct ServerFixture
{
    thewatcher::server::ServerConfig config;
    thewatcher::server::Server server;
    thewatcher::server::StartupSignal startup;
    std::exception_ptr error;
    std::thread thread;
    httplib::Client client;

    explicit ServerFixture()
        : server(make_config())
        , client("http://127.0.0.1:" + std::to_string(config.api_port))
    {
        client.set_connection_timeout(2, 0);
        client.set_read_timeout(2, 0);

        thread = std::thread([this] {
            try
            {
                server.run(&startup);
            }
            catch (...)
            {
                error = std::current_exception();
                startup.fail(error);
            }
        });
    }

    ~ServerFixture()
    {
        server.stop();
        if (thread.joinable())
            thread.join();
        std::error_code ec;
        std::filesystem::remove(config.db_path, ec);
        std::filesystem::remove(config.db_path + "-shm", ec);
        std::filesystem::remove(config.db_path + "-wal", ec);
    }

    bool login()
    {
        return eventually([this] { return login_as_default_admin(client); }, 5s);
    }

private:
    thewatcher::server::ServerConfig make_config()
    {
        thewatcher::crypto::init();
        const auto keys = thewatcher::crypto::generate_curve_keypair();
        const auto ts   = std::to_string(now_ms());
        const auto base = std::filesystem::temp_directory_path() / ("thewatcher-api-test-" + ts);

        thewatcher::server::ServerConfig cfg;
        cfg.bind_address       = free_zmq_endpoint();
        cfg.enrollment_address = free_zmq_endpoint();
        cfg.api_host           = "127.0.0.1";
        cfg.api_port           = free_http_port();
        cfg.db_path            = base.string() + ".db";
        cfg.server_public_key  = keys.public_key_z85;
        cfg.server_secret_key  = keys.secret_key_z85;
        config = cfg;
        return cfg;
    }
};

} // namespace

// ── Settings API ──────────────────────────────────────────────────────────────

SCENARIO("Server settings can be read and updated via the HTTP API")
{
    GIVEN("a running server with default settings and an authenticated admin")
    {
        ServerFixture fx;
        REQUIRE(fx.startup.wait().started);
        REQUIRE(fx.login());

        WHEN("the admin reads the current settings")
        {
            const auto settings = get_json(fx.client, "/api/settings");

            THEN("the response contains all expected setting keys with sensible defaults")
            {
                REQUIRE(settings.has_value());
                REQUIRE(settings->contains("webhook_url"));
                REQUIRE(settings->contains("offline_after_seconds"));
                REQUIRE(settings->contains("escalation_timeout_seconds"));
                REQUIRE(settings->contains("metrics_retention_days"));
                REQUIRE((*settings)["offline_after_seconds"].get<int>() > 0);
                REQUIRE((*settings)["metrics_retention_days"].get<int>() == 30);
            }
        }

        WHEN("the admin updates metrics_retention_days to 7")
        {
            REQUIRE(put_json_ok(fx.client, "/api/settings", {{"metrics_retention_days", 7}}));

            THEN("subsequent GET /api/settings reflects the new value")
            {
                const auto updated = get_json(fx.client, "/api/settings");
                REQUIRE(updated.has_value());
                REQUIRE((*updated)["metrics_retention_days"].get<int>() == 7);
            }
        }

        WHEN("the admin submits an out-of-range metrics_retention_days value")
        {
            THEN("the server rejects it with a 400 response")
            {
                REQUIRE(put_json_error(fx.client, "/api/settings", {{"metrics_retention_days", 0}}));
                REQUIRE(put_json_error(fx.client, "/api/settings", {{"metrics_retention_days", 366}}));
            }
        }

        WHEN("the admin updates offline_after_seconds")
        {
            REQUIRE(put_json_ok(fx.client, "/api/settings", {{"offline_after_seconds", 60}}));

            THEN("the value is persisted")
            {
                const auto updated = get_json(fx.client, "/api/settings");
                REQUIRE(updated.has_value());
                REQUIRE((*updated)["offline_after_seconds"].get<int>() == 60);
            }
        }

        WHEN("an unauthenticated request is made to GET /api/settings")
        {
            httplib::Client unauth("http://127.0.0.1:" + std::to_string(fx.config.api_port));
            unauth.set_connection_timeout(2, 0);
            unauth.set_read_timeout(2, 0);
            auto res = unauth.Get("/api/settings");

            THEN("the server returns 401")
            {
                REQUIRE(res);
                REQUIRE(res->status == 401);
            }
        }
    }
}

// ── Silence rules API ─────────────────────────────────────────────────────────

SCENARIO("Alert silence rules can be created, listed, and deleted via the HTTP API")
{
    GIVEN("a running server and an authenticated operator")
    {
        ServerFixture fx;
        REQUIRE(fx.startup.wait().started);
        REQUIRE(fx.login());

        WHEN("the operator lists silence rules on a fresh server")
        {
            const auto list = get_json(fx.client, "/api/silences");

            THEN("the list is empty")
            {
                REQUIRE(list.has_value());
                REQUIRE(list->is_array());
                REQUIRE(list->empty());
            }
        }

        WHEN("the operator creates a global silence rule")
        {
            const int64_t until_ms = now_ms() + 3'600'000;
            const auto result = post_json(fx.client, "/api/silences", {
                {"agent_id",  "*"                  },
                {"indicator", "*"                  },
                {"reason",    "integration test"   },
                {"until_ms",  until_ms             },
            });

            THEN("the server returns a silence_id")
            {
                REQUIRE(result.has_value());
                REQUIRE(result->contains("silence_id"));
                const auto silence_id = (*result)["silence_id"].get<int64_t>();
                REQUIRE(silence_id > 0);

                AND_THEN("GET /api/silences returns the new rule with correct fields")
                {
                    const auto list = get_json(fx.client, "/api/silences");
                    REQUIRE(list.has_value());
                    REQUIRE(list->size() == 1);
                    const auto& rule = list->at(0);
                    REQUIRE(rule["agent_id"].get<std::string>() == "*");
                    REQUIRE(rule["indicator"].get<std::string>() == "*");
                    REQUIRE(rule["reason"].get<std::string>() == "integration test");
                    REQUIRE(rule["until_ms"].get<int64_t>() == until_ms);
                    REQUIRE(rule["created_by"].get<std::string>() == "thewatcher");
                }

                AND_THEN("deleting the rule removes it from the list")
                {
                    REQUIRE(delete_ok(fx.client, "/api/silences/" + std::to_string(silence_id)));

                    const auto after = get_json(fx.client, "/api/silences");
                    REQUIRE(after.has_value());
                    REQUIRE(after->empty());
                }
            }
        }

        WHEN("the operator creates a silence with until_ms in the past")
        {
            const auto result = post_json(fx.client, "/api/silences", {
                {"agent_id",  "*"           },
                {"indicator", "*"           },
                {"reason",    "stale"       },
                {"until_ms",  int64_t(1000) },
            });

            THEN("the server rejects it with a 400 response")
            {
                REQUIRE_FALSE(result.has_value());
            }
        }

        WHEN("a silence scoped to a specific agent and indicator is created")
        {
            const int64_t until_ms = now_ms() + 7'200'000;
            const auto result = post_json(fx.client, "/api/silences", {
                {"agent_id",  "my-agent"        },
                {"indicator", "cpu"             },
                {"reason",    "cpu spike window"},
                {"until_ms",  until_ms          },
            });

            THEN("the scoped rule appears in the list with correct fields")
            {
                REQUIRE(result.has_value());
                const auto list = get_json(fx.client, "/api/silences");
                REQUIRE(list.has_value());
                REQUIRE(list->size() == 1);
                REQUIRE(list->at(0)["agent_id"].get<std::string>() == "my-agent");
                REQUIRE(list->at(0)["indicator"].get<std::string>() == "cpu");
            }
        }
    }
}

// ── Maintenance windows API ───────────────────────────────────────────────────

SCENARIO("Maintenance windows can be scheduled and cancelled via the HTTP API")
{
    GIVEN("a running server and an authenticated operator")
    {
        ServerFixture fx;
        REQUIRE(fx.startup.wait().started);
        REQUIRE(fx.login());

        WHEN("the operator lists maintenance windows on a fresh server")
        {
            const auto list = get_json(fx.client, "/api/maintenance-windows");

            THEN("the list is empty")
            {
                REQUIRE(list.has_value());
                REQUIRE(list->is_array());
                REQUIRE(list->empty());
            }
        }

        WHEN("a maintenance window is created for all agents")
        {
            const int64_t start_ms = now_ms() + 60'000;
            const int64_t end_ms   = start_ms + 3'600'000;
            const auto result = post_json(fx.client, "/api/maintenance-windows", {
                {"agent_id", "*"              },
                {"start_ms", start_ms         },
                {"end_ms",   end_ms           },
                {"reason",   "weekly patching"},
            });

            THEN("the server returns a window_id")
            {
                REQUIRE(result.has_value());
                REQUIRE(result->contains("window_id"));
                const auto window_id = (*result)["window_id"].get<int64_t>();
                REQUIRE(window_id > 0);

                AND_THEN("GET /api/maintenance-windows returns the scheduled window")
                {
                    const auto list = get_json(fx.client, "/api/maintenance-windows");
                    REQUIRE(list.has_value());
                    REQUIRE(list->size() == 1);
                    const auto& w = list->at(0);
                    REQUIRE(w["agent_id"].get<std::string>() == "*");
                    REQUIRE(w["start_ms"].get<int64_t>() == start_ms);
                    REQUIRE(w["end_ms"].get<int64_t>() == end_ms);
                    REQUIRE(w["reason"].get<std::string>() == "weekly patching");
                    REQUIRE(w["created_by"].get<std::string>() == "thewatcher");
                }

                AND_THEN("deleting the window removes it")
                {
                    REQUIRE(delete_ok(fx.client, "/api/maintenance-windows/" + std::to_string(window_id)));

                    const auto after = get_json(fx.client, "/api/maintenance-windows");
                    REQUIRE(after.has_value());
                    REQUIRE(after->empty());
                }
            }
        }

        WHEN("a window with end_ms before start_ms is submitted")
        {
            const int64_t start_ms = now_ms() + 3'600'000;
            const int64_t end_ms   = start_ms - 1;
            const auto result = post_json(fx.client, "/api/maintenance-windows", {
                {"agent_id", "*"      },
                {"start_ms", start_ms },
                {"end_ms",   end_ms   },
                {"reason",   "bad"    },
            });

            THEN("the server rejects it with a 400 response")
            {
                REQUIRE_FALSE(result.has_value());
            }
        }
    }
}
