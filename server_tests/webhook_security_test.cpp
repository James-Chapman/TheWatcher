#include "../server/webhook_security.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using thewatcher::server::parse_webhook_url;

SCENARIO("Webhook URL parsing accepts only public HTTP webhook targets")
{
    GIVEN("a public HTTP webhook URL")
    {
        WHEN("the URL uses the default port")
        {
            const auto target = parse_webhook_url("http://hooks.example.com/alert");

            THEN("the target host, port, and path are parsed")
            {
                REQUIRE(target.has_value());
                REQUIRE(target->host == "hooks.example.com");
                REQUIRE(target->port == 80);
                REQUIRE(target->path == "/alert");
            }
        }

        WHEN("the URL includes an explicit port and query string")
        {
            const auto target = parse_webhook_url("http://hooks.example.com:8080/a?severity=red");

            THEN("the explicit port and full request path are preserved")
            {
                REQUIRE(target.has_value());
                REQUIRE(target->host == "hooks.example.com");
                REQUIRE(target->port == 8080);
                REQUIRE(target->path == "/a?severity=red");
            }
        }
    }

    GIVEN("webhook URLs that point at restricted or malformed targets")
    {
        const std::vector<std::string> rejected = {
            "https://hooks.example.com/alert",
            "http://localhost/alert",
            "http://127.0.0.1/alert",
            "http://10.0.0.5/alert",
            "http://100.64.0.1/alert",
            "http://169.254.169.254/latest/meta-data",
            "http://172.20.10.2/alert",
            "http://192.168.1.2/alert",
            "http://198.18.0.1/alert",
            "http://224.0.0.1/alert",
            "http://[::1]/alert",
            "http://[0:0:0:0:0:0:0:1]/alert",
            "http://[::]/alert",
            "http://[0:0:0:0:0:0:0:0]/alert",
            "http://[::ffff:127.0.0.1]/alert",
            "http://[fd00::1]/alert",
            "http://[fe80::1]/alert",
            "http://[2001:db8::1]/alert",
            "http://[2001:2::1]/alert",
            "http://user@hooks.example.com/alert",
            "http://hooks.example.com:0/alert",
            "http://hooks.example.com:70000/alert",
            "http://hooks.example.com:notaport/alert",
        };

        WHEN("each URL is parsed")
        {
            THEN("the parser rejects it")
            {
                for (const auto& url : rejected)
                {
                    REQUIRE_FALSE(parse_webhook_url(url).has_value());
                }
            }
        }
    }
}
