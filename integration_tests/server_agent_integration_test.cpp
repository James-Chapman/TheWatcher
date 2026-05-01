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

bool post_ok(httplib::Client& client, const std::string& path)
{
    auto res = client.Post(path);
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
        agent_config.server_public_key = server_config.server_public_key;
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
                    return get_json(client, "/api/agents").has_value();
                },
                5s));

            const auto pending_response = send_enrollment_request(agent_config);
            REQUIRE_FALSE(pending_response.approved);
            REQUIRE(pending_response.message == "pending approval");

            REQUIRE(eventually(
                [&] {
                    const auto pending = find_agent(client, agent_id);
                    return pending && !pending->value("approved", true) && !pending->value("rejected", true);
                },
                5s));

            REQUIRE(post_ok(client, "/api/agents/" + agent_id + "/approve"));
            const auto approved_response = send_enrollment_request(agent_config);
            REQUIRE(approved_response.approved);
            REQUIRE(approved_response.message == "approved");

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
