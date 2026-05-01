#pragma once

#include "common/commands.hpp"
#include "store.hpp"
#include "zap_handler.hpp"

#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace httplib
{
class Server;
}

namespace thewatcher::server
{

struct PendingCommand
{
    std::string agent_id;
    CommandMessage cmd;
};

// REST API server (runs on its own thread via httplib).
// Commands posted to the API are placed in a thread-safe queue; the ZMQ
// server thread drains that queue and forwards them to agents.
class ApiServer
{
public:
    ApiServer(Store& store, ZapHandler& zap, const std::string& host, int port);

    ~ApiServer();

    // Non-blocking start.
    void start();

    void stop();

    // Drain pending commands into caller's vector (called from ZMQ thread).
    void drain_commands(std::vector<PendingCommand>& out);

private:
    void setup_routes();

    Store& store_;
    ZapHandler& zap_;
    std::string host_;
    int port_;

    std::unique_ptr<httplib::Server> http_;
    std::thread thread_;

    std::queue<PendingCommand> cmd_queue_;
    std::mutex cmd_mutex_;
};

} // namespace thewatcher::server
