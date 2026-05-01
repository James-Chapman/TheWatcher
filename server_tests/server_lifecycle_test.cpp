#include "../server/server.hpp"
#include "../server/startup_signal.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <exception>
#include <stdexcept>
#include <thread>
#include <zmq.h>
#include <zmq.hpp>

using namespace thewatcher::server;
using namespace thewatcher::proto;

namespace
{

std::string free_zmq_tcp_endpoint()
{
    zmq::context_t ctx{1};
    zmq::socket_t socket{ctx, ZMQ_REP};
    socket.bind("tcp://127.0.0.1:*");
    auto endpoint = socket.get(zmq::sockopt::last_endpoint);
    socket.close();
    return endpoint;
}

int64_t test_now_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

} // namespace

SCENARIO("The server build provides ZeroMQ CURVE support")
{
    GIVEN("the linked ZeroMQ library")
    {
        WHEN("CURVE support is queried")
        {
            int has_curve = zmq_has("curve");

            THEN("CURVE is available for encrypted agent traffic")
            {
                REQUIRE(has_curve == 1);
            }
        }
    }
}

SCENARIO("Pending enrollment responses do not approve agents before operator approval")
{
    GIVEN("a running server with an empty store")
    {
        ServerConfig config;
        config.bind_address = free_zmq_tcp_endpoint();
        config.enrollment_address = free_zmq_tcp_endpoint();
        config.api_host = "127.0.0.1";
        config.api_port = 0;
        config.db_path = ":memory:";
        config.server_public_key.clear();
        config.server_secret_key.clear();

        Server server(config);
        StartupSignal startup;
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

        auto result = startup.wait();
        REQUIRE(result.started);
        REQUIRE(server_error == nullptr);

        WHEN("an unknown agent sends its first enrollment request")
        {
            zmq::context_t ctx{1};
            zmq::socket_t req{ctx, ZMQ_REQ};
            req.set(zmq::sockopt::linger, 0);
            req.set(zmq::sockopt::rcvtimeo, 2000);
            req.connect(config.enrollment_address);

            EnrollRequest enroll;
            enroll.agent_id = "pending-agent";
            enroll.hostname = "pending-host";
            enroll.platform = "windows";
            enroll.curve_public_key_z85 = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

            Frame frame;
            frame.type = static_cast<uint8_t>(FrameType::ENROLL_REQUEST);
            frame.agent_id = enroll.agent_id;
            frame.timestamp_ms = test_now_ms();
            frame.payload = pack(enroll);

            auto encoded = encode_frame(frame);
            req.send(zmq::message_t(encoded.data(), encoded.size()), zmq::send_flags::none);

            zmq::message_t reply;
            auto received = req.recv(reply, zmq::recv_flags::none);

            THEN("the response keeps the agent pending")
            {
                REQUIRE(received.has_value());
                auto reply_frame = decode_frame(reply.data(), reply.size());
                auto response = unpack<EnrollResponse>(reply_frame.payload);
                REQUIRE(response.approved == false);
                REQUIRE(response.message == "pending approval");
            }
        }

        server.stop();
        if (server_thread.joinable())
            server_thread.join();
    }
}

SCENARIO("Server startup failures are reported before foreground mode is declared running")
{
    GIVEN("a server configured with an invalid ZMQ bind address")
    {
        ServerConfig config;
        config.bind_address = "tcp://127.0.0.1:not-a-port";
        config.enrollment_address = "tcp://127.0.0.1:5556";
        config.api_host = "127.0.0.1";
        config.api_port = 0;
        config.db_path = ":memory:";
        config.server_public_key.clear();
        config.server_secret_key.clear();

        Server server(config);
        StartupSignal startup;
        std::exception_ptr server_error;

        WHEN("the server is started on a worker thread")
        {
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

            auto result = startup.wait();
            if (server_thread.joinable())
                server_thread.join();

            THEN("the startup signal reports failure instead of success")
            {
                REQUIRE_FALSE(result.started);
                REQUIRE(server_error != nullptr);
            }

            AND_THEN("the failure message is available to the foreground runner")
            {
                REQUIRE_FALSE(result.message.empty());
            }
        }
    }
}
