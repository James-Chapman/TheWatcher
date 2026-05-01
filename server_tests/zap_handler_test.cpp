#include "../server/zap_handler.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace thewatcher::server;

// The ZAP handler background thread polls inproc://zeromq.zap.01.
// These tests exercise only the in-memory key-set API, which is safe to call
// from the test thread without triggering ZMQ authentication traffic.

SCENARIO("A new ZapHandler has an empty approved-key set")
{
    GIVEN("a ZMQ context and a freshly constructed ZapHandler")
    {
        zmq::context_t ctx{1};
        ZapHandler zap{ctx};

        WHEN("has_key is called for a key that was never added")
        {
            bool result = zap.has_key("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");

            THEN("it returns false")
            {
                REQUIRE(result == false);
            }
        }
    }
}

SCENARIO("Adding a key makes it visible to has_key")
{
    GIVEN("a ZapHandler with no keys")
    {
        zmq::context_t ctx{1};
        ZapHandler zap{ctx};

        WHEN("a key is added")
        {
            const std::string key = "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB";
            zap.add_key(key);

            THEN("has_key returns true for that key")
            {
                REQUIRE(zap.has_key(key) == true);
            }
        }
    }
}

SCENARIO("Removing a key makes it invisible to has_key")
{
    GIVEN("a ZapHandler with one key in its set")
    {
        zmq::context_t ctx{1};
        ZapHandler zap{ctx};
        const std::string key = "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC";
        zap.add_key(key);

        WHEN("remove_key is called for that key")
        {
            zap.remove_key(key);

            THEN("has_key returns false")
            {
                REQUIRE(zap.has_key(key) == false);
            }
        }
    }
}

SCENARIO("Multiple keys are tracked independently")
{
    GIVEN("a ZapHandler")
    {
        zmq::context_t ctx{1};
        ZapHandler zap{ctx};

        WHEN("two distinct keys are added")
        {
            const std::string k1 = "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD";
            const std::string k2 = "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE";
            zap.add_key(k1);
            zap.add_key(k2);

            THEN("both keys are independently present")
            {
                REQUIRE(zap.has_key(k1) == true);
                REQUIRE(zap.has_key(k2) == true);
            }

            AND_WHEN("only the first key is removed")
            {
                zap.remove_key(k1);

                THEN("the first key is absent but the second remains")
                {
                    REQUIRE(zap.has_key(k1) == false);
                    REQUIRE(zap.has_key(k2) == true);
                }
            }
        }
    }
}

SCENARIO("Removing a key that was never added is harmless")
{
    GIVEN("a ZapHandler with no keys")
    {
        zmq::context_t ctx{1};
        ZapHandler zap{ctx};

        WHEN("remove_key is called for an absent key")
        {
            const std::string key = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF";

            THEN("no exception is thrown")
            {
                REQUIRE_NOTHROW(zap.remove_key(key));
            }

            AND_THEN("has_key still returns false")
            {
                REQUIRE(zap.has_key(key) == false);
            }
        }
    }
}

SCENARIO("Adding the same key twice then removing it once leaves it absent (set semantics)")
{
    GIVEN("a ZapHandler")
    {
        zmq::context_t ctx{1};
        ZapHandler zap{ctx};
        const std::string key = "GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG";

        WHEN("the key is added twice and removed once")
        {
            zap.add_key(key);
            zap.add_key(key);
            zap.remove_key(key);

            THEN("has_key returns false — the set has no reference counting")
            {
                REQUIRE(zap.has_key(key) == false);
            }
        }
    }
}
