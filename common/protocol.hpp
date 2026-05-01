#pragma once

#include <cstdint>
#include <msgpack.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace thewatcher::proto
{

enum class FrameType : uint8_t
{
    HEARTBEAT = 0x01,
    METRICS = 0x02,
    COMMAND = 0x03,
    COMMAND_ACK = 0x04,
    CONFIG_UPDATE = 0x05,
    ENROLL_REQUEST = 0x06,
    ENROLL_RESPONSE = 0x07,
    CONFIG_REQUEST = 0x08,
};

// Wire frame sent over every ZMQ message (msgpack-encoded).
struct Frame
{
    uint8_t type; // FrameType cast to uint8_t
    std::string agent_id;
    int64_t timestamp_ms = 0;
    std::vector<uint8_t> payload; // type-specific msgpack payload

    MSGPACK_DEFINE_ARRAY(type, agent_id, timestamp_ms, payload)
};

// Enrollment sub-types (carried in Frame::payload).
struct EnrollRequest
{
    std::string agent_id;
    std::string hostname;
    std::string platform;
    std::string curve_public_key_z85; // 40-char z85

    MSGPACK_DEFINE_ARRAY(agent_id, hostname, platform, curve_public_key_z85)
};

struct EnrollResponse
{
    bool approved = false;
    std::string message;

    MSGPACK_DEFINE_ARRAY(approved, message)
};

// ── helpers ───────────────────────────────────────────────────────────────────

template <typename T>
std::vector<uint8_t> pack(const T& obj)
{
    msgpack::sbuffer buf;
    msgpack::pack(buf, obj);
    return {reinterpret_cast<const uint8_t*>(buf.data()), reinterpret_cast<const uint8_t*>(buf.data()) + buf.size()};
}

template <typename T>
T unpack(const std::vector<uint8_t>& data)
{
    auto handle = msgpack::unpack(reinterpret_cast<const char*>(data.data()), data.size());
    return handle.get().as<T>();
}

template <typename T>
T unpack(const void* data, std::size_t size)
{
    auto handle = msgpack::unpack(static_cast<const char*>(data), size);
    return handle.get().as<T>();
}

inline std::vector<uint8_t> encode_frame(const Frame& f)
{
    return pack(f);
}

inline Frame decode_frame(const void* data, std::size_t size)
{
    return unpack<Frame>(data, size);
}

} // namespace thewatcher::proto
