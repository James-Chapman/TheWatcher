#include "common/commands.hpp"
#include "common/metrics.hpp"
#include "common/protocol.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace thewatcher;
using namespace thewatcher::proto;

// ── Frame round-trip ──────────────────────────────────────────────────────────

SCENARIO("Frame round-trip encoding via msgpack")
{
    GIVEN("a populated Frame with all fields set")
    {
        Frame original;
        original.type = static_cast<uint8_t>(FrameType::HEARTBEAT);
        original.agent_id = "agent-abc-123";
        original.timestamp_ms = 1'700'000'000'000LL;
        original.payload = {0xAA, 0xBB, 0xCC};

        WHEN("the frame is encoded and decoded")
        {
            auto encoded = encode_frame(original);
            auto decoded = decode_frame(encoded.data(), encoded.size());

            THEN("all fields are preserved exactly")
            {
                REQUIRE(decoded.type == original.type);
                REQUIRE(decoded.agent_id == original.agent_id);
                REQUIRE(decoded.timestamp_ms == original.timestamp_ms);
                REQUIRE(decoded.payload == original.payload);
            }
        }
    }

    GIVEN("a Frame with an empty payload")
    {
        Frame f;
        f.type = static_cast<uint8_t>(FrameType::METRICS);
        f.agent_id = "agent-001";
        f.timestamp_ms = 0;
        f.payload = {};

        WHEN("encoded and decoded")
        {
            auto encoded = encode_frame(f);
            auto decoded = decode_frame(encoded.data(), encoded.size());

            THEN("the empty payload survives")
            {
                REQUIRE(decoded.payload.empty());
            }
        }
    }
}

// ── FrameType enum stability ──────────────────────────────────────────────────

SCENARIO("FrameType enum values match the wire protocol specification")
{
    GIVEN("the FrameType enum")
    {
        THEN("each constant has the correct wire value")
        {
            CHECK(static_cast<uint8_t>(FrameType::HEARTBEAT) == 0x01);
            CHECK(static_cast<uint8_t>(FrameType::METRICS) == 0x02);
            CHECK(static_cast<uint8_t>(FrameType::COMMAND) == 0x03);
            CHECK(static_cast<uint8_t>(FrameType::COMMAND_ACK) == 0x04);
            CHECK(static_cast<uint8_t>(FrameType::CONFIG_UPDATE) == 0x05);
            CHECK(static_cast<uint8_t>(FrameType::ENROLL_REQUEST) == 0x06);
            CHECK(static_cast<uint8_t>(FrameType::ENROLL_RESPONSE) == 0x07);
        }
    }
}

// ── Enrollment payloads ───────────────────────────────────────────────────────

SCENARIO("EnrollRequest is serialized and deserialized correctly")
{
    GIVEN("a fully populated EnrollRequest")
    {
        EnrollRequest req;
        req.agent_id = "agent-xyz";
        req.hostname = "my-host";
        req.platform = "linux";
        req.curve_public_key_z85 = "abcdefghijklmnopqrstuvwxyz0123456789ABCD";

        WHEN("packed into bytes and unpacked")
        {
            auto packed = pack(req);
            auto unpacked = unpack<EnrollRequest>(packed);

            THEN("all enrollment fields match the original")
            {
                REQUIRE(unpacked.agent_id == req.agent_id);
                REQUIRE(unpacked.hostname == req.hostname);
                REQUIRE(unpacked.platform == req.platform);
                REQUIRE(unpacked.curve_public_key_z85 == req.curve_public_key_z85);
            }
        }
    }
}

SCENARIO("EnrollResponse carries approval status correctly")
{
    GIVEN("an approved EnrollResponse")
    {
        EnrollResponse resp;
        resp.approved = true;
        resp.message = "approved";

        WHEN("packed and unpacked")
        {
            auto unpacked = unpack<EnrollResponse>(pack(resp));

            THEN("approved is true and message is preserved")
            {
                REQUIRE(unpacked.approved == true);
                REQUIRE(unpacked.message == "approved");
            }
        }
    }

    GIVEN("a pending EnrollResponse")
    {
        EnrollResponse resp;
        resp.approved = false;
        resp.message = "pending approval";

        WHEN("packed and unpacked")
        {
            auto unpacked = unpack<EnrollResponse>(pack(resp));

            THEN("approved is false")
            {
                REQUIRE(unpacked.approved == false);
                REQUIRE(unpacked.message == "pending approval");
            }
        }
    }
}

// ── CommandMessage ────────────────────────────────────────────────────────────

SCENARIO("CommandMessage with SET_INTERVAL args survives serialization")
{
    GIVEN("a SET_INTERVAL command with a 60-second interval")
    {
        SetIntervalArgs args{60};
        CommandMessage cmd;
        cmd.command_type = static_cast<uint8_t>(CommandType::SET_INTERVAL);
        cmd.command_id = "cmd-001";
        cmd.args = pack(args);

        WHEN("the CommandMessage is packed and unpacked")
        {
            auto unpacked = unpack<CommandMessage>(pack(cmd));

            THEN("command type and id are preserved")
            {
                REQUIRE(unpacked.command_type == cmd.command_type);
                REQUIRE(unpacked.command_id == cmd.command_id);
            }

            AND_THEN("the nested args decode to the correct interval")
            {
                auto decoded_args = unpack<SetIntervalArgs>(unpacked.args);
                REQUIRE(decoded_args.interval_seconds == 60);
            }
        }
    }
}

SCENARIO("CommandType enum values match the wire protocol specification")
{
    GIVEN("the CommandType enum")
    {
        THEN("each constant has the correct wire value")
        {
            CHECK(static_cast<uint8_t>(CommandType::SET_INTERVAL) == 0x01);
            CHECK(static_cast<uint8_t>(CommandType::SET_PROCESS_LIMIT) == 0x02);
            CHECK(static_cast<uint8_t>(CommandType::RESTART_COLLECTORS) == 0x03);
            CHECK(static_cast<uint8_t>(CommandType::PAUSE) == 0x04);
            CHECK(static_cast<uint8_t>(CommandType::RESUME) == 0x05);
            CHECK(static_cast<uint8_t>(CommandType::GET_STATUS) == 0x06);
            CHECK(static_cast<uint8_t>(CommandType::DISCONNECT) == 0x07);
        }
    }
}

// ── SystemMetrics ─────────────────────────────────────────────────────────────

SCENARIO("SystemMetrics survives a full Frame payload round-trip")
{
    GIVEN("a SystemMetrics with representative values in all sub-structs")
    {
        SystemMetrics m;
        m.hostname = "test-host";
        m.platform = "linux";
        m.os_name = "Ubuntu 22.04";
        m.uptime_seconds = 3600.5;

        m.cpu.usage_percent = 42.7;
        m.cpu.num_logical_cores = 8;
        m.cpu.load_avg_1m = 1.5;

        m.memory.total_bytes = 8ULL * 1024 * 1024 * 1024;
        m.memory.used_bytes = 4ULL * 1024 * 1024 * 1024;

        DiskMetrics disk;
        disk.device = "/dev/sda1";
        disk.mount_point = "/";
        disk.usage_percent = 65.0;
        m.disks.push_back(disk);

        NetworkMetrics net;
        net.interface_name = "eth0";
        net.bytes_recv_per_sec = 1024;
        net.is_up = true;
        m.networks.push_back(net);

        WHEN("packed into a Frame payload, encoded, decoded, and unpacked")
        {
            Frame f;
            f.type = static_cast<uint8_t>(FrameType::METRICS);
            f.agent_id = "agent-001";
            f.timestamp_ms = 9999;
            f.payload = pack(m);

            auto encoded = encode_frame(f);
            auto decoded_f = decode_frame(encoded.data(), encoded.size());
            auto decoded_m = unpack<SystemMetrics>(decoded_f.payload);

            THEN("static system fields are preserved")
            {
                REQUIRE(decoded_m.hostname == m.hostname);
                REQUIRE(decoded_m.platform == m.platform);
                REQUIRE(decoded_m.os_name == m.os_name);
                REQUIRE(decoded_m.uptime_seconds == Catch::Approx(3600.5));
            }

            AND_THEN("CPU metrics are preserved")
            {
                REQUIRE(decoded_m.cpu.usage_percent == Catch::Approx(42.7));
                REQUIRE(decoded_m.cpu.num_logical_cores == 8);
                REQUIRE(decoded_m.cpu.load_avg_1m == Catch::Approx(1.5));
            }

            AND_THEN("memory metrics are preserved")
            {
                REQUIRE(decoded_m.memory.total_bytes == m.memory.total_bytes);
                REQUIRE(decoded_m.memory.used_bytes == m.memory.used_bytes);
            }

            AND_THEN("disk entries are preserved")
            {
                REQUIRE(decoded_m.disks.size() == 1);
                REQUIRE(decoded_m.disks[0].device == "/dev/sda1");
                REQUIRE(decoded_m.disks[0].usage_percent == Catch::Approx(65.0));
            }

            AND_THEN("network entries are preserved")
            {
                REQUIRE(decoded_m.networks.size() == 1);
                REQUIRE(decoded_m.networks[0].interface_name == "eth0");
                REQUIRE(decoded_m.networks[0].bytes_recv_per_sec == 1024u);
                REQUIRE(decoded_m.networks[0].is_up == true);
            }
        }
    }
}
