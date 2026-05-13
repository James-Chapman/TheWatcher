#include "agent/collectors/disk_collector.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <set>
#include <string>
#include <system_error>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

using namespace thewatcher;
using namespace thewatcher::agent;

SCENARIO("DiskCollector reports at least one mount with sane metrics")
{
    GIVEN("a fresh DiskCollector on the host")
    {
        DiskCollector collector;
        SystemMetrics metrics;

        WHEN("the collector runs once")
        {
            collector.update(metrics);

            THEN("at least one disk is reported")
            {
                REQUIRE_FALSE(metrics.disks.empty());
            }

            AND_THEN("each reported disk has a mount point and bounded usage")
            {
                for (const auto& disk : metrics.disks)
                {
                    REQUIRE_FALSE(disk.mount_point.empty());
                    REQUIRE(disk.total_bytes > 0);
                    REQUIRE(disk.usage_percent >= 0.0);
                    REQUIRE(disk.usage_percent <= 100.0);
                }
            }
        }
    }
}

#ifdef _WIN32

SCENARIO("On Windows, DiskCollector enumerates every fixed drive the system exposes")
{
    GIVEN("the set of fixed drives reported by GetLogicalDrives + GetDriveTypeA")
    {
        std::set<std::string> expected_mount_points;
        const DWORD mask = ::GetLogicalDrives();
        for (int i = 0; i < 26; ++i)
        {
            if ((mask & (1u << i)) == 0)
                continue;
            char root[4] = {static_cast<char>('A' + i), ':', '\\', '\0'};
            if (::GetDriveTypeA(root) != DRIVE_FIXED)
                continue;
            std::error_code ec;
            (void) std::filesystem::space(root, ec);
            if (ec)
                continue;
            expected_mount_points.insert(root);
        }

        WHEN("the disk collector runs")
        {
            DiskCollector collector;
            SystemMetrics metrics;
            collector.update(metrics);

            std::set<std::string> actual_mount_points;
            for (const auto& disk : metrics.disks)
                actual_mount_points.insert(disk.mount_point);

            THEN("the collector reports the same fixed-drive set")
            {
                REQUIRE(actual_mount_points == expected_mount_points);
            }
        }
    }
}

#endif // _WIN32
