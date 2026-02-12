#pragma once

#include "valorant/types.hpp"
#include <vector>

namespace valorant {

std::vector<HourlyPerformance> performance_by_hour(
    const std::vector<PlayerMatchSummary>& matches);

std::vector<SessionPerformance> performance_by_session(
    const std::vector<Session>& sessions);

std::vector<SessionPerformance> rr_by_session(
    const std::vector<Session>& sessions);

std::vector<RollingMetric> rolling_kda(
    const std::vector<PlayerMatchSummary>& matches, int window = 20);

std::vector<RollingMetric> rolling_win_rate(
    const std::vector<PlayerMatchSummary>& matches, int window = 20);

DecayCurveModel decay_curve(
    const std::vector<Session>& sessions, int min_session_length = 3);

} // namespace valorant
