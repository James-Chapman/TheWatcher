#include "common/SingleLog.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

using namespace std::chrono_literals;

namespace
{

bool file_contains(const std::filesystem::path& path, const std::string& needle)
{
    std::ifstream input(path);
    if (!input)
        return false;
    const std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    return content.find(needle) != std::string::npos;
}

template <typename Predicate>
bool eventually(Predicate predicate, std::chrono::milliseconds timeout, std::chrono::milliseconds interval = 20ms)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (predicate())
            return true;
        std::this_thread::sleep_for(interval);
    }
    return predicate();
}

} // namespace

SCENARIO("SingleLog flushes low severity file output in batches")
{
    GIVEN("a configured log file and one hundred INFO messages")
    {
        const auto path = std::filesystem::temp_directory_path() / "thewatcher-singlelog-batch-test.log";
        std::error_code ec;
        std::filesystem::remove(path, ec);

        auto& logger = thewatcher::logging::SingleLog::GetInstance();
        logger.SetConsoleLogLevel(thewatcher::logging::LogLevel::L_OFF);
        logger.SetFileLogLevel(thewatcher::logging::LogLevel::L_TRACE);
        logger.SetLogFilePath(path.string());

        WHEN("the logger writes enough INFO entries to reach the batch threshold")
        {
            for (int i = 0; i < 100; ++i)
            {
                LOG_INFO(std::string("singlelog batched flush probe ") + std::to_string(i));
            }

            THEN("the batch is visible before process shutdown")
            {
                REQUIRE(eventually(
                    [&] {
                        return file_contains(path, "singlelog batched flush probe 99");
                    },
                    2s));
            }
        }

        std::filesystem::remove(path, ec);
    }
}

SCENARIO("SingleLog flushes notice and higher severity file output immediately")
{
    GIVEN("a configured log file and a NOTICE message")
    {
        const auto path = std::filesystem::temp_directory_path() / "thewatcher-singlelog-notice-test.log";
        std::error_code ec;
        std::filesystem::remove(path, ec);

        auto& logger = thewatcher::logging::SingleLog::GetInstance();
        logger.SetConsoleLogLevel(thewatcher::logging::LogLevel::L_OFF);
        logger.SetFileLogLevel(thewatcher::logging::LogLevel::L_TRACE);
        logger.SetLogFilePath(path.string());

        WHEN("the logger writes one NOTICE entry")
        {
            LOG_NOTICE("singlelog immediate notice flush probe");

            THEN("the entry is visible before process shutdown")
            {
                REQUIRE(eventually(
                    [&] {
                        return file_contains(path, "singlelog immediate notice flush probe");
                    },
                    2s));
            }
        }

        std::filesystem::remove(path, ec);
    }
}
