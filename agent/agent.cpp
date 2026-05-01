#include "agent.hpp"

#include "collectors/cpu_collector.hpp"
#include "collectors/disk_collector.hpp"
#include "collectors/memory_collector.hpp"
#include "collectors/network_collector.hpp"
#include "collectors/process_collector.hpp"
#include "collectors/temperature_collector.hpp"
#include "common/SingleLog.hpp"
#include "enrollment.hpp"

#include <chrono>
#include <stdexcept>
#include <thread>
#include <utility>

namespace thewatcher::agent
{

namespace proto = thewatcher::proto; // file-scope alias is C++11-compatible

// ── Construction ──────────────────────────────────────────────────────────────

Agent::Agent(AgentConfig config)
    : config_(std::move(config)), interval_seconds_(config_.collection_interval), process_limit_(config_.process_limit)
{
    LOG_FUNCTION_TRACE

    os_name_ = get_os_name();
    os_version_ = get_os_version();
    hostname_ = get_hostname();
    platform_ = get_platform_string();
    LOGF_DEBUG("Agent constructed id=%s host=%s platform=%s os=%s version=%s interval=%d process_limit=%d",
               config_.agent_id.c_str(), hostname_.c_str(), platform_.c_str(), os_name_.c_str(), os_version_.c_str(),
               interval_seconds_.load(), process_limit_.load());

    collectors_.emplace_back(std::make_unique<CpuCollector>());
    collectors_.emplace_back(std::make_unique<MemoryCollector>());
    collectors_.emplace_back(std::make_unique<DiskCollector>());
    collectors_.emplace_back(std::make_unique<TemperatureCollector>());
    collectors_.emplace_back(std::make_unique<ProcessCollector>(process_limit_.load()));
    collectors_.emplace_back(std::make_unique<NetworkCollector>());
    LOGF_DEBUG("Registered %zu collectors", collectors_.size());
}

Agent::~Agent()
{
    stop();
}

// ── Threading ─────────────────────────────────────────────────────────────────

void Agent::start()
{
    LOG_FUNCTION_TRACE
    {
        std::atomic<bool> stop_enroll{false};
        LOGF_INFO("Starting enrollment with %s", config_.enrollment_address.c_str());
        enroll(config_, ctx_, stop_enroll);
        LOGF_INFO("Enrollment approved - connecting to %s", config_.server_address.c_str());
    }

    dealer_.set(zmq::sockopt::linger, 0);
    dealer_.set(zmq::sockopt::routing_id, config_.agent_id);
    if (!config_.server_public_key.empty())
    {
        LOG_DEBUG("Configuring DEALER socket with CURVE keys");
        dealer_.set(zmq::sockopt::curve_serverkey, std::string_view(config_.server_public_key));
        dealer_.set(zmq::sockopt::curve_publickey, std::string_view(config_.agent_public_key));
        dealer_.set(zmq::sockopt::curve_secretkey, std::string_view(config_.agent_secret_key));
    }
    else
    {
        LOG_WARNING("Server public key is empty; data socket CURVE encryption is disabled");
    }
    dealer_.connect(config_.server_address);
    connected_.store(true);
    LOGF_DEBUG("Dealer connected with routing_id=%s", config_.agent_id.c_str());

    io_thread_ = std::jthread([this](std::stop_token st) {
        io_loop(st);
    });
    collection_thread_ = std::jthread([this](std::stop_token st) {
        collection_loop(st);
    });
    heartbeat_thread_ = std::jthread([this](std::stop_token st) {
        heartbeat_loop(st);
    });
}

void Agent::stop()
{
    LOG_FUNCTION_TRACE
    io_thread_.request_stop();
    collection_thread_.request_stop();
    heartbeat_thread_.request_stop();
    connected_.store(false);
    LOG_DEBUG("Stop requested for IO, collection, and heartbeat threads");
}

// ── IO thread ─────────────────────────────────────────────────────────────────

void Agent::io_loop(std::stop_token st)
{
    zmq::pollitem_t items[] = {
        {static_cast<void*>(dealer_), 0, ZMQ_POLLIN, 0}
    };

    while (!st.stop_requested())
    {
        {
            std::lock_guard<std::mutex> lock(outbox_mutex_);
            while (!outbox_.empty())
            {
                auto payload = std::move(outbox_.front());
                outbox_.pop();
                dealer_.send(zmq::message_t{}, zmq::send_flags::sndmore);
                dealer_.send(zmq::message_t(payload.data(), payload.size()), zmq::send_flags::none);
            }
        }

        zmq::poll(items, 1, std::chrono::milliseconds{100});
        if (items[0].revents & ZMQ_POLLIN)
        {
            zmq::message_t delim, payload;
            dealer_.recv(delim, zmq::recv_flags::none);
            dealer_.recv(payload, zmq::recv_flags::none);
            try
            {
                auto frame = proto::decode_frame(payload.data(), payload.size());
                handle_frame(frame);
            }
            catch (const std::exception& e)
            {
                LOGF_WARNING("Frame decode error: %s", e.what());
            }
        }
    }
}

// ── Collection thread ─────────────────────────────────────────────────────────

void Agent::collection_loop(std::stop_token st)
{
    while (!st.stop_requested())
    {
        if (connected_ && !paused_)
        {
            try
            {
                auto metrics = collect_metrics();

                proto::Frame f;
                f.type = static_cast<uint8_t>(proto::FrameType::METRICS);
                f.agent_id = config_.agent_id;
                f.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::system_clock::now().time_since_epoch())
                                     .count();
                f.payload = proto::pack(metrics);
                enqueue(proto::encode_frame(f));
                LOGF_TRACE("Queued metrics frame timestamp=%lld", static_cast<long long>(f.timestamp_ms));

                proto::Frame cfg_request;
                cfg_request.type = static_cast<uint8_t>(proto::FrameType::CONFIG_REQUEST);
                cfg_request.agent_id = config_.agent_id;
                cfg_request.timestamp_ms = f.timestamp_ms;
                enqueue(proto::encode_frame(cfg_request));
                LOGF_TRACE("Queued config request timestamp=%lld", static_cast<long long>(cfg_request.timestamp_ms));
            }
            catch (const std::exception& e)
            {
                LOGF_WARNING("Collection error: %s", e.what());
            }
        }

        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{interval_seconds_.load()};
        while (!st.stop_requested() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds{500});
    }
}

// ── Heartbeat thread ──────────────────────────────────────────────────────────

void Agent::heartbeat_loop(std::stop_token st)
{
    while (!st.stop_requested())
    {
        if (connected_)
        {
            proto::Frame f;
            f.type = static_cast<uint8_t>(proto::FrameType::HEARTBEAT);
            f.agent_id = config_.agent_id;
            f.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count();
            enqueue(proto::encode_frame(f));
        }

        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{10};
        while (!st.stop_requested() && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds{500});
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void Agent::enqueue(std::vector<uint8_t> frame)
{
    std::lock_guard<std::mutex> lock(outbox_mutex_);
    outbox_.push(std::move(frame));
}

SystemMetrics Agent::collect_metrics()
{
    LOG_TRACE("Collecting system metrics snapshot");
    SystemMetrics m;
    fill_static_info(m);
    for (auto& c : collectors_)
    {
        LOGF_TRACE("Updating collector: %s", std::string(c->name()).c_str());
        c->update(m);
    }
    return m;
}

void Agent::fill_static_info(SystemMetrics& m) const
{
    m.os_name = os_name_;
    m.os_version = os_version_;
    m.hostname = hostname_;
    m.platform = platform_;
    m.uptime_seconds = get_uptime_seconds();
}

// ── Command handling ──────────────────────────────────────────────────────────

void Agent::handle_frame(const proto::Frame& f)
{
    auto type = static_cast<proto::FrameType>(f.type);
    if (type == proto::FrameType::COMMAND)
    {
        LOGF_DEBUG("Received COMMAND frame payload_size=%zu", f.payload.size());
        auto cmd = proto::unpack<CommandMessage>(f.payload);
        handle_command(cmd);
    }
    else if (type == proto::FrameType::CONFIG_UPDATE)
    {
        LOGF_DEBUG("Received CONFIG_UPDATE payload_size=%zu", f.payload.size());
        auto cfg = proto::unpack<ConfigUpdate>(f.payload);
        interval_seconds_.store(cfg.interval_seconds);
        process_limit_.store(cfg.process_limit);
        LOGF_INFO("Applied config update interval_seconds=%d process_limit=%d", cfg.interval_seconds,
                  cfg.process_limit);
        for (auto& c : collectors_)
        {
            if (auto* process = dynamic_cast<ProcessCollector*>(c.get()))
            {
                process->set_limit(cfg.process_limit);
                break;
            }
        }
    }
}

void Agent::handle_command(const CommandMessage& cmd)
{
    LOGF_DEBUG("Handling command type=%u command_id=%s args_size=%zu", static_cast<unsigned>(cmd.command_type),
               cmd.command_id.c_str(), cmd.args.size());
    auto send_ack_now = [&](bool ok, const std::string& msg) {
        AckMessage ack_msg{cmd.command_id, ok, msg};
        proto::Frame f;
        f.type = static_cast<uint8_t>(proto::FrameType::COMMAND_ACK);
        f.agent_id = config_.agent_id;
        f.timestamp_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        f.payload = proto::pack(ack_msg);
        auto encoded = proto::encode_frame(f);
        dealer_.send(zmq::message_t{}, zmq::send_flags::sndmore);
        dealer_.send(zmq::message_t(encoded.data(), encoded.size()), zmq::send_flags::none);
        LOGF_DEBUG("Sent immediate ACK command_id=%s ok=%d message=%s", cmd.command_id.c_str(), ok ? 1 : 0,
                   msg.c_str());
    };

    auto ack = [&](bool ok, const std::string& msg) {
        AckMessage ack_msg{cmd.command_id, ok, msg};
        proto::Frame f;
        f.type = static_cast<uint8_t>(proto::FrameType::COMMAND_ACK);
        f.agent_id = config_.agent_id;
        f.timestamp_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        f.payload = proto::pack(ack_msg);
        enqueue(proto::encode_frame(f));
        LOGF_DEBUG("Queued ACK command_id=%s ok=%d message=%s", cmd.command_id.c_str(), ok ? 1 : 0, msg.c_str());
    };

    switch (static_cast<CommandType>(cmd.command_type))
    {
    case CommandType::SET_INTERVAL: {
        auto args = proto::unpack<SetIntervalArgs>(cmd.args);
        interval_seconds_.store(args.interval_seconds);
        LOGF_INFO("Set collection interval to %d seconds", args.interval_seconds);
        ack(true, "interval set to " + std::to_string(args.interval_seconds));
        break;
    }
    case CommandType::SET_PROCESS_LIMIT: {
        auto args = proto::unpack<SetProcessLimitArgs>(cmd.args);
        process_limit_.store(args.limit);
        LOGF_INFO("Set process limit to %d", args.limit);
        for (auto& c : collectors_)
        {
            if (auto* process = dynamic_cast<ProcessCollector*>(c.get()))
            {
                process->set_limit(args.limit);
                break;
            }
        }
        ack(true, "process limit set to " + std::to_string(args.limit));
        break;
    }
    case CommandType::RESTART_COLLECTORS:
        LOG_INFO("Restarting collectors");
        collectors_.clear();
        collectors_.emplace_back(std::make_unique<CpuCollector>());
        collectors_.emplace_back(std::make_unique<MemoryCollector>());
        collectors_.emplace_back(std::make_unique<DiskCollector>());
        collectors_.emplace_back(std::make_unique<TemperatureCollector>());
        collectors_.emplace_back(std::make_unique<ProcessCollector>(process_limit_.load()));
        collectors_.emplace_back(std::make_unique<NetworkCollector>());
        ack(true, "collectors restarted");
        break;
    case CommandType::PAUSE:
        LOG_INFO("Pausing metric collection");
        paused_.store(true);
        ack(true, "paused");
        break;
    case CommandType::RESUME:
        LOG_INFO("Resuming metric collection");
        paused_.store(false);
        ack(true, "resumed");
        break;
    case CommandType::GET_STATUS: {
        LOG_INFO("Immediate status requested");
        auto metrics = collect_metrics();
        proto::Frame f;
        f.type = static_cast<uint8_t>(proto::FrameType::METRICS);
        f.agent_id = config_.agent_id;
        f.timestamp_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
        f.payload = proto::pack(metrics);
        enqueue(proto::encode_frame(f));
        proto::Frame cfg_request;
        cfg_request.type = static_cast<uint8_t>(proto::FrameType::CONFIG_REQUEST);
        cfg_request.agent_id = config_.agent_id;
        cfg_request.timestamp_ms = f.timestamp_ms;
        enqueue(proto::encode_frame(cfg_request));
        ack(true, "status sent");
        break;
    }
    case CommandType::DISCONNECT:
        LOG_INFO("Disconnect command received");
        send_ack_now(true, "disconnecting");
        stop();
        break;
    default:
        LOGF_WARNING("Unknown command type=%u command_id=%s", static_cast<unsigned>(cmd.command_type),
                     cmd.command_id.c_str());
        ack(false, "unknown command");
        break;
    }
}

} // namespace thewatcher::agent
