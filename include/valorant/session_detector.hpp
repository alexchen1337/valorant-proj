#pragma once

#include "valorant/types.hpp"
#include <chrono>
#include <vector>

namespace valorant {

std::vector<Session> detect_sessions(
    const std::vector<PlayerMatchSummary>& matches,
    std::chrono::minutes gap_threshold = std::chrono::minutes(45));

} // namespace valorant
