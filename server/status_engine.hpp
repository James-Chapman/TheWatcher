#pragma once

#include "common/metrics.hpp"
#include "store.hpp"

#include <cstdint>
#include <string>

namespace thewatcher::server
{

enum class IndicatorStatus
{
    Green = 0,
    Grey = 1,
    Yellow = 2,
    Amber = 3,
    Red = 4,
    Blue = 5
};

std::string status_to_string(IndicatorStatus status);
IndicatorStatus status_from_string(const std::string& value);
bool is_worse_status(IndicatorStatus previous, IndicatorStatus current);

class StatusEngine
{
public:
    explicit StatusEngine(Store& store);

    void evaluate_metrics(const std::string& agent_id, const SystemMetrics& metrics, int64_t timestamp_ms);
    void enter_maintenance(const std::string& agent_id, const std::string& reason, int64_t until_ms, int64_t now_ms);
    void exit_maintenance(const std::string& agent_id);

private:
    IndicatorStatus classify_percent(double value, const PercentThresholds& thresholds) const;
    IndicatorStatus classify_network_mbps(double value, const NetworkThresholds& thresholds) const;
    IndicatorStatus confirm_numeric_status(const std::string& agent_id, const std::string& indicator,
                                           IndicatorStatus previous, IndicatorStatus raw, int required_readings);
    IndicatorStatus process_status_for_count(int missing_count, int readings_to_red) const;
    void record_transition(const std::string& agent_id, const std::string& indicator, IndicatorStatus next,
                           const std::string& message, int64_t timestamp_ms);

    Store& store_;
};

} // namespace thewatcher::server
