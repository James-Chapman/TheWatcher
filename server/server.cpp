#include "server.hpp"

#include "common/SingleLog.hpp"
#include "common/commands.hpp"
#include "common/metrics.hpp"

#include <chrono>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace thewatcher::server
{

using namespace thewatcher::proto;
using json = nlohmann::json;

namespace
{

    int64_t now_ms()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

} // namespace

// ── Construction ──────────────────────────────────────────────────────────────

Server::Server(ServerConfig config) : config_(std::move(config))
{
    LOG_FUNCTION_TRACE
    LOGF_DEBUG("Creating server bind=%s enrollment=%s api=%s:%d db_type=%s db_path=%s offline_after_seconds=%d",
               config_.bind_address.c_str(), config_.enrollment_address.c_str(), config_.api_host.c_str(),
               config_.api_port, config_.db_type.c_str(), config_.db_path.c_str(), config_.offline_after_seconds);
    store_ = make_store(config_.db_type, config_.db_path);
    zap_ = std::make_unique<ZapHandler>(ctx_);
    api_ = std::make_unique<ApiServer>(*store_, *zap_, config_.api_host, config_.api_port);

    // Pre-load all already-approved agents into the ZAP key set so they can
    // reconnect after a server restart without needing re-approval.
    for (auto& agent : store_->list_agents())
    {
        if (agent.approved)
        {
            zap_->add_key(agent.curve_public_key_z85);
            LOGF_DEBUG("Loaded approved agent key into ZAP id=%s", agent.agent_id.c_str());
        }
    }
}

Server::~Server()
{
    stop();
}

// ── Main loop ─────────────────────────────────────────────────────────────────

void Server::run(StartupSignal* startup)
{
    LOG_FUNCTION_TRACE
    // ROUTER: encrypted data plane for metric/heartbeat/ack traffic
    zmq::socket_t router(ctx_, ZMQ_ROUTER);
    router.set(zmq::sockopt::linger, 0);
    // Silently drop frames addressed to an unknown identity rather than erroring.
    router.set(zmq::sockopt::router_mandatory, 0);

    if (!config_.server_public_key.empty())
    {
        LOG_DEBUG("Configuring ROUTER socket as CURVE server");
        router.set(zmq::sockopt::curve_server, true);
        router.set(zmq::sockopt::curve_secretkey, std::string_view(config_.server_secret_key));
    }
    else
    {
        LOG_WARNING("Server public key is empty; ROUTER socket CURVE encryption is disabled");
    }
    router.bind(config_.bind_address);
    LOGF_INFO("ROUTER bound to %s", config_.bind_address.c_str());

    // REP: plain-text enrollment handshake (no CURVE)
    zmq::socket_t enroll(ctx_, ZMQ_REP);
    enroll.set(zmq::sockopt::linger, 0);
    enroll.bind(config_.enrollment_address);
    LOGF_INFO("Enrollment REP bound to %s", config_.enrollment_address.c_str());

    api_->start();
    LOGF_INFO("REST API on %s:%d", config_.api_host.c_str(), config_.api_port);

    running_.store(true);
    if (startup)
        startup->succeed();
    auto next_offline_scan = std::chrono::steady_clock::now();

    zmq::pollitem_t items[] = {
        {static_cast<void*>(router), 0, ZMQ_POLLIN, 0},
        {static_cast<void*>(enroll), 0, ZMQ_POLLIN, 0},
    };

    while (running_.load())
    {
        zmq::poll(items, 2, std::chrono::milliseconds{100});

        if (items[0].revents & ZMQ_POLLIN)
            handle_router_frame(router);

        if (items[1].revents & ZMQ_POLLIN)
            handle_enrollment(enroll);

        dispatch_commands(router);
        if (std::chrono::steady_clock::now() >= next_offline_scan)
        {
            if (config_.offline_after_seconds > 0)
            {
                const auto cutoff = now_ms() - (static_cast<int64_t>(config_.offline_after_seconds) * 1000);
                LOGF_TRACE("Scanning for offline agents cutoff_ms=%lld", static_cast<long long>(cutoff));
                store_->mark_agents_offline_before(cutoff);
            }
            next_offline_scan = std::chrono::steady_clock::now() + std::chrono::seconds{1};
        }
    }

    api_->stop();
    LOG_INFO("Stopped.");
}

void Server::stop()
{
    LOG_FUNCTION_TRACE
    running_.store(false);
}

// ── ROUTER handler ────────────────────────────────────────────────────────────

void Server::handle_router_frame(zmq::socket_t& router)
{
    // ROUTER prepends the sender's routing-id (set by agent to its agent_id).
    // The agent DEALER explicitly sends [empty delimiter][payload], so the
    // server receives exactly: [identity][empty][payload].
    zmq::message_t identity_msg, delim_msg, payload_msg;
    if (!router.recv(identity_msg, zmq::recv_flags::none))
        return;
    if (!router.recv(delim_msg, zmq::recv_flags::none))
        return;
    if (!router.recv(payload_msg, zmq::recv_flags::none))
        return;

    // Drain unexpected extra frames so the socket state stays clean.
    while (router.get(zmq::sockopt::rcvmore))
    {
        zmq::message_t extra;
        router.recv(extra, zmq::recv_flags::none);
    }

    if (payload_msg.size() == 0)
        return;

    Frame frame;
    try
    {
        frame = decode_frame(payload_msg.data(), payload_msg.size());
    }
    catch (const std::exception& e)
    {
        LOGF_WARNING("Frame decode error: %s", e.what());
        return;
    }

    const std::string& agent_id = frame.agent_id;
    LOGF_TRACE("Received frame agent_id=%s type=%u payload_size=%zu", agent_id.c_str(),
               static_cast<unsigned>(frame.type), frame.payload.size());

    // Update last_seen timestamp for every inbound frame.
    auto rec = store_->get_agent(agent_id);
    if (rec)
    {
        rec->connected = true;
        rec->last_seen = now_ms();
        store_->upsert_agent(*rec);
        LOGF_TRACE("Updated last_seen for agent_id=%s", agent_id.c_str());
    }
    else
    {
        LOGF_DEBUG("Received frame from unknown agent_id=%s", agent_id.c_str());
    }

    auto ftype = static_cast<FrameType>(frame.type);

    if (ftype == FrameType::METRICS)
    {
        try
        {
            auto metrics = unpack<SystemMetrics>(frame.payload);
            json j = metrics; // NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE handles this
            MetricsRow row;
            row.agent_id = agent_id;
            row.timestamp_ms = frame.timestamp_ms;
            row.metrics_json = j.dump();
            store_->insert_metrics(row);
            LOGF_DEBUG("Stored metrics agent_id=%s timestamp_ms=%lld payload_size=%zu", agent_id.c_str(),
                       static_cast<long long>(frame.timestamp_ms), row.metrics_json.size());
        }
        catch (const std::exception& e)
        {
            LOGF_WARNING("Metrics error for %s: %s", agent_id.c_str(), e.what());
        }
    }
    else if (ftype == FrameType::HEARTBEAT)
    {
        // last_seen already updated; no further action needed.
        LOGF_TRACE("Heartbeat processed agent_id=%s", agent_id.c_str());
    }
    else if (ftype == FrameType::CONFIG_REQUEST)
    {
        auto agent = store_->get_agent(agent_id);
        if (agent)
        {
            ConfigUpdate cfg{agent->collection_interval, agent->process_limit};
            LOGF_DEBUG("Sending config update agent_id=%s interval_seconds=%d process_limit=%d", agent_id.c_str(),
                       cfg.interval_seconds, cfg.process_limit);
            Frame response;
            response.type = static_cast<uint8_t>(FrameType::CONFIG_UPDATE);
            response.agent_id = agent_id;
            response.timestamp_ms = now_ms();
            response.payload = pack(cfg);

            auto encoded = encode_frame(response);
            router.send(zmq::message_t(agent_id.data(), agent_id.size()), zmq::send_flags::sndmore);
            router.send(zmq::message_t{}, zmq::send_flags::sndmore);
            router.send(zmq::message_t(encoded.data(), encoded.size()), zmq::send_flags::none);
        }
        else
        {
            LOGF_DEBUG("Ignoring config request from unknown agent_id=%s", agent_id.c_str());
        }
    }
    else if (ftype == FrameType::COMMAND_ACK)
    {
        try
        {
            auto ack = unpack<AckMessage>(frame.payload);
            auto command = dispatched_commands_.find(ack.command_id);
            if (command != dispatched_commands_.end())
            {
                if (ack.success && (command->second == CommandType::DISCONNECT ||
                                    command->second == CommandType::PAUSE || command->second == CommandType::RESUME))
                {
                    auto command_agent = store_->get_agent(agent_id);
                    if (command_agent)
                    {
                        if (command->second == CommandType::DISCONNECT)
                            command_agent->connected = false;
                        else if (command->second == CommandType::PAUSE)
                            command_agent->maintenance = true;
                        else if (command->second == CommandType::RESUME)
                            command_agent->maintenance = false;
                        command_agent->last_seen = now_ms();
                        store_->upsert_agent(*command_agent);
                    }
                }
                dispatched_commands_.erase(command);
            }
            LOGF_INFO("ACK from %s cmd=%s ok=%d msg=%s", agent_id.c_str(), ack.command_id.c_str(), ack.success ? 1 : 0,
                      ack.message.c_str());
        }
        catch (const std::exception& e)
        {
            LOGF_WARNING("Failed to unpack command ACK from %s: %s", agent_id.c_str(), e.what());
        }
    }
    else
    {
        LOGF_DEBUG("Ignoring unsupported frame type=%u from agent_id=%s", static_cast<unsigned>(frame.type),
                   agent_id.c_str());
    }
}

// ── Enrollment handler ────────────────────────────────────────────────────────

void Server::handle_enrollment(zmq::socket_t& enroll)
{
    LOG_FUNCTION_TRACE
    zmq::message_t msg;
    if (!enroll.recv(msg, zmq::recv_flags::none))
        return;
    LOGF_TRACE("Enrollment message received size=%zu", msg.size());

    // Helper: send an error response and return.
    auto send_error = [&](const std::string& reason) {
        LOGF_DEBUG("Sending enrollment error response reason=%s", reason.c_str());
        EnrollResponse resp{false, reason};
        Frame f;
        f.type = static_cast<uint8_t>(FrameType::ENROLL_RESPONSE);
        f.agent_id = "";
        f.timestamp_ms = now_ms();
        f.payload = pack(resp);
        auto encoded = encode_frame(f);
        enroll.send(zmq::message_t(encoded.data(), encoded.size()), zmq::send_flags::none);
    };

    Frame frame;
    try
    {
        frame = decode_frame(msg.data(), msg.size());
    }
    catch (const std::exception& e)
    {
        LOGF_WARNING("Enrollment decode error: %s", e.what());
        send_error("bad frame");
        return;
    }

    EnrollRequest req;
    try
    {
        req = unpack<EnrollRequest>(frame.payload);
    }
    catch (const std::exception& e)
    {
        LOGF_WARNING("EnrollRequest unpack error: %s", e.what());
        send_error("bad payload");
        return;
    }
    LOGF_INFO("Enrollment request agent_id=%s hostname=%s platform=%s", req.agent_id.c_str(), req.hostname.c_str(),
              req.platform.c_str());

    auto existing = store_->get_agent(req.agent_id);
    bool approved = false;

    if (existing)
    {
        if (existing->rejected)
        {
            LOGF_NOTICE("Rejected enrollment retry agent_id=%s", req.agent_id.c_str());
            send_error("rejected");
            return;
        }

        approved = existing->approved;
        LOGF_DEBUG("Refreshing existing enrollment agent_id=%s approved=%d", req.agent_id.c_str(), approved ? 1 : 0);
        // Refresh mutable fields in case they changed (e.g. new keypair).
        existing->hostname = req.hostname;
        existing->platform = req.platform;
        existing->curve_public_key_z85 = req.curve_public_key_z85;
        existing->last_seen = now_ms();
        store_->upsert_agent(*existing);
    }
    else
    {
        AgentRecord rec;
        rec.agent_id = req.agent_id;
        rec.hostname = req.hostname;
        rec.platform = req.platform;
        rec.curve_public_key_z85 = req.curve_public_key_z85;
        rec.approved = false;
        rec.rejected = false;
        rec.first_seen = now_ms();
        rec.last_seen = rec.first_seen;
        store_->upsert_agent(rec);
        LOGF_NOTICE("New agent: %s (%s/%s) - pending approval", req.agent_id.c_str(), req.hostname.c_str(),
                    req.platform.c_str());
    }

    EnrollResponse resp;
    resp.approved = approved;
    resp.message = approved ? "approved" : "pending approval";

    Frame resp_frame;
    resp_frame.type = static_cast<uint8_t>(FrameType::ENROLL_RESPONSE);
    resp_frame.agent_id = req.agent_id;
    resp_frame.timestamp_ms = now_ms();
    resp_frame.payload = pack(resp);

    auto encoded = encode_frame(resp_frame);
    enroll.send(zmq::message_t(encoded.data(), encoded.size()), zmq::send_flags::none);
    LOGF_DEBUG("Enrollment response agent_id=%s approved=%d message=%s", req.agent_id.c_str(), resp.approved ? 1 : 0,
               resp.message.c_str());
}

// ── Command dispatch ──────────────────────────────────────────────────────────

void Server::dispatch_commands(zmq::socket_t& router)
{
    std::vector<PendingCommand> cmds;
    api_->drain_commands(cmds);
    if (!cmds.empty())
        LOGF_DEBUG("Dispatching %zu pending command(s)", cmds.size());

    for (auto& pc : cmds)
    {
        Frame f;
        f.type = static_cast<uint8_t>(FrameType::COMMAND);
        f.agent_id = pc.agent_id;
        f.timestamp_ms = now_ms();
        f.payload = pack(pc.cmd);

        auto encoded = encode_frame(f);

        // ROUTER send: [identity][empty delimiter][payload]
        // The agent DEALER receives [empty][payload] (identity is stripped).
        try
        {
            router.send(zmq::message_t(pc.agent_id.data(), pc.agent_id.size()), zmq::send_flags::sndmore);
            router.send(zmq::message_t{}, zmq::send_flags::sndmore);
            router.send(zmq::message_t(encoded.data(), encoded.size()), zmq::send_flags::none);
            dispatched_commands_[pc.cmd.command_id] = static_cast<CommandType>(pc.cmd.command_type);
            LOGF_DEBUG("Dispatched command agent_id=%s command_id=%s type=%u", pc.agent_id.c_str(),
                       pc.cmd.command_id.c_str(), static_cast<unsigned>(pc.cmd.command_type));
        }
        catch (const std::exception& e)
        {
            LOGF_ERROR("Failed to dispatch command to %s: %s", pc.agent_id.c_str(), e.what());
        }
    }
}

} // namespace thewatcher::server
