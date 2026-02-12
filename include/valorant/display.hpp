#pragma once

#include "valorant/types.hpp"
#include <string>
#include <vector>

namespace valorant {

enum class OutputFormat { Table, Csv };

void display_hourly(const std::vector<HourlyPerformance>& data, OutputFormat fmt);
void display_sessions(const std::vector<SessionPerformance>& data, OutputFormat fmt);
void display_rr_sessions(const std::vector<SessionPerformance>& data, OutputFormat fmt);
void display_rolling_kda(const std::vector<RollingMetric>& data, OutputFormat fmt);
void display_rolling_wr(const std::vector<RollingMetric>& data, OutputFormat fmt);
void display_decay(const DecayCurveModel& model, OutputFormat fmt);
void display_player_header(const PlayerIdentity& player, int match_count);
void display_progress(int current, int total);

} // namespace valorant
