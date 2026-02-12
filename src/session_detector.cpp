#include "valorant/session_detector.hpp"

namespace valorant {

std::vector<Session> detect_sessions(
    const std::vector<PlayerMatchSummary>& matches,
    std::chrono::minutes gap_threshold) {

    if (matches.empty()) return {};

    std::vector<Session> sessions;
    Session current;
    current.index = 0;
    current.matches.push_back(matches[0]);

    for (size_t i = 1; i < matches.size(); ++i) {
        auto& prev = current.matches.back();
        auto prev_end = prev.game_start + std::chrono::seconds(prev.game_length_secs);
        auto gap = matches[i].game_start - prev_end;

        if (gap > gap_threshold) {
            sessions.push_back(std::move(current));
            current = Session{};
            current.index = static_cast<int>(sessions.size());
        }

        current.matches.push_back(matches[i]);
    }

    sessions.push_back(std::move(current));
    return sessions;
}

} // namespace valorant
