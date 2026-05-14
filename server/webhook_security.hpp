#pragma once

#include <optional>
#include <string>

namespace thewatcher::server
{

struct WebhookTarget
{
    std::string host;
    int port = 80;
    std::string path = "/";
};

std::optional<WebhookTarget> parse_webhook_url(const std::string& url);

} // namespace thewatcher::server
