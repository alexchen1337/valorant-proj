#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace valorant {

using TimePoint = std::chrono::system_clock::time_point;

struct PlayerIdentity {
    std::string name;
    std::string tag;
    std::string puuid;
    std::string region;
    std::string card_small;
};

struct PlayerMatchSummary {
    std::string match_id;
    std::string map;
    std::string mode;
    std::string agent;
    TimePoint game_start;
    int game_length_secs = 0;
    int kills = 0;
    int deaths = 0;
    int assists = 0;
    int score = 0;
    int damage_made = 0;
    int rounds_played = 0;
    bool won = false;
    int rr_change = 0;
    bool rr_available = false;

    double kda() const {
        return deaths == 0 ? static_cast<double>(kills + assists)
                           : static_cast<double>(kills + assists) / deaths;
    }

    double damage_per_round() const {
        return rounds_played == 0 ? 0.0
                                  : static_cast<double>(damage_made) / rounds_played;
    }
};

struct Session {
    int index = 0;
    std::vector<PlayerMatchSummary> matches;

    TimePoint start() const { return matches.front().game_start; }
    TimePoint end() const {
        auto& last = matches.back();
        return last.game_start + std::chrono::seconds(last.game_length_secs);
    }
    int game_count() const { return static_cast<int>(matches.size()); }
};

struct MmrHistoryEntry {
    std::string match_id;
    int rr_change = 0;
    int rr_after = 0;
    int tier_after = 0;
    TimePoint timestamp;
};

// Analytics output types

struct HourlyPerformance {
    int hour = 0;
    double avg_kda = 0.0;
    double win_rate = 0.0;
    int match_count = 0;
};

struct SessionGameMetric {
    int game_number = 0;
    double kda = 0.0;
    double damage_per_round = 0.0;
    int rr_change = 0;
};

struct SessionPerformance {
    int session_index = 0;
    int game_count = 0;
    int total_rr = 0;
    double avg_rr_per_game = 0.0;
    double avg_kda = 0.0;
    std::vector<SessionGameMetric> games;
};

struct RollingMetric {
    int match_index = 0;
    std::string match_id;
    double value = 0.0;
};

struct DecayCurveModel {
    double slope = 0.0;
    double intercept = 0.0;
    double r_squared = 0.0;
    std::vector<std::pair<int, double>> points; // (game_number, avg_kda)
};

struct AgentPerformance {
    std::string agent;
    int games = 0;
    double avg_kda = 0.0;
    double win_rate = 0.0;
    double avg_damage_per_round = 0.0;
    double pick_rate = 0.0;
};

struct MapPerformance {
    std::string map;
    int games = 0;
    double avg_kda = 0.0;
    double win_rate = 0.0;
    double avg_score = 0.0;
};

struct OverviewStats {
    int total_games = 0;
    int wins = 0;
    int losses = 0;
    double overall_kda = 0.0;
    double win_rate = 0.0;
    int total_kills = 0;
    int total_deaths = 0;
    int total_assists = 0;
    double avg_damage_per_round = 0.0;
    int total_rr = 0;
    std::string best_agent;
    double best_agent_kda = 0.0;
    std::string worst_map;
    double worst_map_wr = 1.0;
    int longest_win_streak = 0;
    int longest_loss_streak = 0;
    int current_streak = 0; // positive = wins, negative = losses
    double headshot_pct = 0.0;
};

struct ApiError {
    int status_code = 0;
    std::string message;
};

struct ClientConfig {
    std::string api_key;
    std::string base_url = "api.henrikdev.xyz";
};

} // namespace valorant
