#pragma once

#include <shared_mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_set>
#include <zmq.hpp>

namespace thewatcher::server
{

// Runs the ZAP (ZeroMQ Authentication Protocol) handler on a background thread.
// Approves CURVE connections whose z85 public key is in the approved set.
class ZapHandler
{
public:
    explicit ZapHandler(zmq::context_t& ctx);
    ~ZapHandler();

    void add_key(const std::string& z85_key);
    void remove_key(const std::string& z85_key);
    bool has_key(const std::string& z85_key) const;

private:
    void run(std::stop_token st);

    zmq::context_t& ctx_;
    mutable std::shared_mutex keys_mutex_;
    std::unordered_set<std::string> approved_keys_;
    std::jthread thread_;
};

} // namespace thewatcher::server
