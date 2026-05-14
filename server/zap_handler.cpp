#include "zap_handler.hpp"

#include "common/SingleLog.hpp"

#include <array>
#include <chrono>

#include <sodium.h>

namespace thewatcher::server
{

ZapHandler::ZapHandler(zmq::context_t& ctx)
    : ctx_(ctx)
    , thread_([this](std::stop_token st) {
        run(st);
    }){LOG_FUNCTION_TRACE}

    ZapHandler::~ZapHandler()
{
    LOG_FUNCTION_TRACE
    thread_.request_stop();
}

void ZapHandler::add_key(const std::string& z85_key)
{
    std::unique_lock lock(keys_mutex_);
    approved_keys_.insert(z85_key);
    LOGF_DEBUG("ZAP key added approved_key_count=%zu", approved_keys_.size());
}

void ZapHandler::remove_key(const std::string& z85_key)
{
    std::unique_lock lock(keys_mutex_);
    approved_keys_.erase(z85_key);
    LOGF_DEBUG("ZAP key removed approved_key_count=%zu", approved_keys_.size());
}

bool ZapHandler::has_key(const std::string& z85_key) const
{
    std::shared_lock lock(keys_mutex_);
    return approved_keys_.count(z85_key) > 0;
}

void ZapHandler::run(std::stop_token st)
{
    LOG_FUNCTION_TRACE
    zmq::socket_t zap(ctx_, ZMQ_REP);
    zap.bind("inproc://zeromq.zap.01");
    LOG_DEBUG("ZAP handler bound to inproc://zeromq.zap.01");

    zmq::pollitem_t items[] = {
        {static_cast<void*>(zap), 0, ZMQ_POLLIN, 0}
    };

    while (!st.stop_requested())
    {
        zmq::poll(items, 1, std::chrono::milliseconds{100});
        if (!(items[0].revents & ZMQ_POLLIN))
            continue;

        // ZAP request: version, request_id, domain, address, identity, mechanism, credentials...
        std::vector<zmq::message_t> parts;
        bool more = true;
        while (more)
        {
            parts.emplace_back();
            if (!zap.recv(parts.back(), zmq::recv_flags::none))
            {
                parts.pop_back();
                break;
            }
            more = zap.get(zmq::sockopt::rcvmore) != 0;
        }

        // Minimum 7 frames for CURVE: version,req_id,domain,addr,identity,mechanism,key
        bool allow = false;
        if (parts.size() >= 7)
        {
            auto& mech_frame = parts[5];
            std::string mechanism(static_cast<char*>(mech_frame.data()), mech_frame.size());
            LOGF_TRACE("ZAP request mechanism=%s part_count=%zu", mechanism.c_str(), parts.size());
            if (mechanism == "CURVE" && parts[6].size() == 32)
            {
                // Raw 32-byte key — encode to z85 for comparison
                std::array<char, 41> z85buf{};
                zmq_z85_encode(z85buf.data(), static_cast<const uint8_t*>(parts[6].data()), 32);
                std::string z85(z85buf.data(), 40);
                std::shared_lock lock(keys_mutex_);
                allow = approved_keys_.count(z85) > 0;
                LOGF_TRACE("ZAP CURVE lookup approved=%d", allow ? 1 : 0);
            }
            else if (mechanism == "NULL")
            {
                // The enrollment REP socket deliberately uses the NULL security
                // mechanism so unenrolled agents can reach it without a key.
                // ZMQ enforces curve_server=true on the ROUTER (data plane) socket
                // at the transport layer before ZAP is consulted, so NULL-mechanism
                // clients cannot reach the data plane regardless of this allow here.
                allow = true;
                LOG_TRACE("ZAP NULL mechanism allowed");
            }
        }
        else
        {
            LOGF_WARNING("Malformed ZAP request part_count=%zu", parts.size());
        }

        // ZAP reply: version, request_id, status_code, status_text, user_id, metadata
        auto& req_id = parts[1];
        zap.send(zmq::message_t("1.0", 3), zmq::send_flags::sndmore);
        zap.send(zmq::message_t(req_id.data(), req_id.size()), zmq::send_flags::sndmore);
        if (allow)
        {
            zap.send(zmq::message_t("200", 3), zmq::send_flags::sndmore);
            zap.send(zmq::message_t("OK", 2), zmq::send_flags::sndmore);
        }
        else
        {
            LOG_WARNING("Rejected unapproved CURVE key");
            zap.send(zmq::message_t("400", 3), zmq::send_flags::sndmore);
            zap.send(zmq::message_t("Not authorized", 14), zmq::send_flags::sndmore);
        }
        zap.send(zmq::message_t("", 0), zmq::send_flags::sndmore); // user_id
        zap.send(zmq::message_t("", 0), zmq::send_flags::none);    // metadata
    }
}

} // namespace thewatcher::server
