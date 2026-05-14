#include "webhook_security.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <initializer_list>
#include <string_view>

namespace thewatcher::server
{
namespace
{
    constexpr std::size_t max_webhook_url_bytes = 2048;

    std::string lowercase_ascii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    }

    bool has_forbidden_authority_char(const std::string& value)
    {
        return std::any_of(value.begin(), value.end(), [](unsigned char ch) {
            return std::isspace(ch) || ch == '/' || ch == '\\' || ch == '@';
        });
    }

    bool parse_port(std::string_view text, int& port)
    {
        if (text.empty() || !std::all_of(text.begin(), text.end(), [](unsigned char ch) {
                return std::isdigit(ch);
            }))
        {
            return false;
        }

        int parsed = 0;
        const auto* first = text.data();
        const auto* last = text.data() + text.size();
        const auto [ptr, ec] = std::from_chars(first, last, parsed);
        if (ec != std::errc{} || ptr != last || parsed < 1 || parsed > 65535)
            return false;

        port = parsed;
        return true;
    }

    std::optional<std::array<int, 4>> parse_ipv4_literal(const std::string& host)
    {
        std::array<int, 4> octets{};
        std::size_t start = 0;
        for (std::size_t index = 0; index < octets.size(); ++index)
        {
            const auto end = index + 1 == octets.size() ? host.size() : host.find('.', start);
            if (end == std::string::npos || end == start)
                return std::nullopt;
            if (!std::all_of(host.begin() + static_cast<std::ptrdiff_t>(start),
                             host.begin() + static_cast<std::ptrdiff_t>(end), [](unsigned char ch) {
                                 return std::isdigit(ch);
                             }))
            {
                return std::nullopt;
            }

            int octet = 0;
            const auto* first = host.data() + start;
            const auto* last = host.data() + end;
            const auto [ptr, ec] = std::from_chars(first, last, octet);
            if (ec != std::errc{} || ptr != last || octet < 0 || octet > 255)
                return std::nullopt;
            octets[index] = octet;
            start = end + 1;
        }

        if (start != host.size() + 1)
            return std::nullopt;
        return octets;
    }

    bool is_restricted_ipv4_literal(const std::array<int, 4>& ip)
    {
        if (ip[0] == 0 || ip[0] == 10 || ip[0] == 127)
            return true;
        if (ip[0] == 100 && ip[1] >= 64 && ip[1] <= 127)
            return true;
        if (ip[0] == 169 && ip[1] == 254)
            return true;
        if (ip[0] == 172 && ip[1] >= 16 && ip[1] <= 31)
            return true;
        if (ip[0] == 192 && ip[1] == 168)
            return true;
        if (ip[0] == 198 && (ip[1] == 18 || ip[1] == 19))
            return true;
        if (ip[0] >= 224)
            return true;
        if (ip[0] == 192 && ip[1] == 0 && ip[2] == 2)
            return true;
        if (ip[0] == 198 && ip[1] == 51 && ip[2] == 100)
            return true;
        if (ip[0] == 203 && ip[1] == 0 && ip[2] == 113)
            return true;
        return ip[0] == 255 && ip[1] == 255 && ip[2] == 255 && ip[3] == 255;
    }

    bool starts_with_any(std::string_view value, std::initializer_list<std::string_view> prefixes)
    {
        return std::any_of(prefixes.begin(), prefixes.end(), [&](std::string_view prefix) {
            return value.rfind(prefix, 0) == 0;
        });
    }

    bool is_restricted_ipv6_literal(const std::string& host)
    {
        const auto value = lowercase_ascii(host);
        if (value == "::" || value == "::1")
            return true;
        if (starts_with_any(value, {"fc", "fd", "ff"}))
            return true;
        if (starts_with_any(value, {"fe8", "fe9", "fea", "feb"}))
            return true;

        const auto mapped = value.rfind(':');
        if (mapped != std::string::npos)
        {
            const auto ipv4_tail = value.substr(mapped + 1);
            if (const auto ipv4 = parse_ipv4_literal(ipv4_tail))
                return is_restricted_ipv4_literal(*ipv4);
        }

        return false;
    }

    bool ends_with(std::string_view value, std::string_view suffix)
    {
        return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
    }

    bool is_restricted_hostname(const std::string& host)
    {
        return host == "localhost" || host == "localhost." || ends_with(host, ".localhost") ||
               ends_with(host, ".localhost.");
    }

    bool is_restricted_host(const std::string& host)
    {
        if (const auto ipv4 = parse_ipv4_literal(host))
            return is_restricted_ipv4_literal(*ipv4);
        if (host.find(':') != std::string::npos)
            return is_restricted_ipv6_literal(host);
        return is_restricted_hostname(host);
    }

    bool valid_hostname_shape(const std::string& host)
    {
        if (host.empty() || host.size() > 253 || has_forbidden_authority_char(host))
            return false;
        return std::all_of(host.begin(), host.end(), [](unsigned char ch) {
            return std::isalnum(ch) || ch == '-' || ch == '.' || ch == ':';
        });
    }

    std::optional<WebhookTarget> parse_authority(const std::string& authority, std::string path)
    {
        if (authority.empty() || has_forbidden_authority_char(authority))
            return std::nullopt;

        WebhookTarget target;
        target.path = std::move(path);

        if (authority.front() == '[')
        {
            const auto close = authority.find(']');
            if (close == std::string::npos)
                return std::nullopt;
            target.host = lowercase_ascii(authority.substr(1, close - 1));
            if (close + 1 < authority.size())
            {
                if (authority[close + 1] != ':' ||
                    !parse_port(std::string_view(authority).substr(close + 2), target.port))
                {
                    return std::nullopt;
                }
            }
        }
        else
        {
            const auto first_colon = authority.find(':');
            const auto last_colon = authority.rfind(':');
            if (first_colon != last_colon)
                return std::nullopt;
            if (last_colon != std::string::npos)
            {
                target.host = lowercase_ascii(authority.substr(0, last_colon));
                if (!parse_port(std::string_view(authority).substr(last_colon + 1), target.port))
                    return std::nullopt;
            }
            else
            {
                target.host = lowercase_ascii(authority);
            }
        }

        if (!valid_hostname_shape(target.host) || is_restricted_host(target.host))
            return std::nullopt;
        return target;
    }
} // namespace

std::optional<WebhookTarget> parse_webhook_url(const std::string& url)
{
    constexpr std::string_view prefix = "http://";
    if (url.empty() || url.size() > max_webhook_url_bytes || url.rfind(prefix, 0) != 0)
        return std::nullopt;

    const auto rest = url.substr(prefix.size());
    const auto path_start = rest.find_first_of("/?#");
    const auto authority = rest.substr(0, path_start);
    std::string path = "/";
    if (path_start != std::string::npos)
        path = rest[path_start] == '/' ? rest.substr(path_start) : "/" + rest.substr(path_start);

    return parse_authority(authority, std::move(path));
}

} // namespace thewatcher::server
