#pragma once

#include <atomic>
#include <string>
#include <zmq.hpp>

namespace thewatcher::agent
{

struct AgentConfig;

// Sends an EnrollRequest to the server enrollment port and polls until approved.
// Throws std::runtime_error if rejected or on unrecoverable errors.
// Updates config.server_public_key once received in the approval response.
void enroll(AgentConfig& config, zmq::context_t& ctx, const std::atomic<bool>& stop_flag,
            int poll_interval_seconds = 10);

std::string get_hostname();
std::string get_platform_string();
std::string get_os_name();
std::string get_os_version();
double get_uptime_seconds();

} // namespace thewatcher::agent
