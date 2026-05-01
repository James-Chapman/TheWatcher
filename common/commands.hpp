#pragma once

#include <cstdint>
#include <msgpack.hpp>
#include <string>
#include <vector>

namespace thewatcher
{

enum class CommandType : uint8_t
{
    SET_INTERVAL = 0x01,       // Change collection interval
    SET_PROCESS_LIMIT = 0x02,  // Change top-N process count
    RESTART_COLLECTORS = 0x03, // Restart all collectors
    PAUSE = 0x04,              // Pause metric collection
    RESUME = 0x05,             // Resume metric collection
    GET_STATUS = 0x06,         // Request an immediate status report
    DISCONNECT = 0x07,         // Graceful agent disconnect
};

// Sent from server → agent inside Frame::payload.
struct CommandMessage
{
    uint8_t command_type;      // CommandType cast to uint8_t
    std::string command_id;    // UUID, echoed in AckMessage
    std::vector<uint8_t> args; // command-specific msgpack payload

    MSGPACK_DEFINE_ARRAY(command_type, command_id, args)
};

// Sent from agent → server inside Frame::payload after handling a command.
struct AckMessage
{
    std::string command_id;
    bool success = false;
    std::string message;

    MSGPACK_DEFINE_ARRAY(command_id, success, message)
};

// Payload for SET_INTERVAL.
struct SetIntervalArgs
{
    int interval_seconds = 30;
    MSGPACK_DEFINE_ARRAY(interval_seconds)
};

// Payload for SET_PROCESS_LIMIT.
struct SetProcessLimitArgs
{
    int limit = 25;
    MSGPACK_DEFINE_ARRAY(limit)
};

// Sent from server → agent after admin changes agent config.
// Carried inside a CONFIG_UPDATE frame::payload.
struct ConfigUpdate
{
    int interval_seconds = 30;
    int process_limit = 25;

    MSGPACK_DEFINE_ARRAY(interval_seconds, process_limit)
};

} // namespace thewatcher
