#include "valorant/analytics.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_map>

namespace valorant {

std::vector<HourlyPerformance> performance_by_hour(
    const std::vector<PlayerMatchSummary>& matches) {

    struct Bucket {
        double total_kda = 0.0;
        int wins = 0;
        int count = 0;
    };

    std::array<Bucket, 24> buckets{};

    for (auto& m : matches) {
        auto tt = std::chrono::system_clock::to_time_t(m.game_start);
        auto* lt = std::localtime(&tt);
        int hour = lt->tm_hour;

        buckets[hour].total_kda += m.kda();
        buckets[hour].wins += m.won ? 1 : 0;
        buckets[hour].count++;
    }

    std::vector<HourlyPerformance> result;
    for (int h = 0; h < 24; ++h) {
        if (buckets[h].count == 0) continue;
        result.push_back({
            .hour = h,
            .avg_kda = buckets[h].total_kda / buckets[h].count,
            .win_rate = static_cast<double>(buckets[h].wins) / buckets[h].count,
            .match_count = buckets[h].count,
        });
    }

    return result;
}

std::vector<SessionPerformance> performance_by_session(
    const std::vector<Session>& sessions) {

    std::vector<SessionPerformance> result;
    for (auto& session : sessions) {
        SessionPerformance sp;
        sp.session_index = session.index;
        sp.game_count = session.game_count();

        double total_kda = 0.0;
        int total_rr = 0;

        for (int i = 0; i < static_cast<int>(session.matches.size()); ++i) {
            auto& m = session.matches[i];
            total_kda += m.kda();
            total_rr += m.rr_change;

            sp.games.push_back({
                .game_number = i + 1,
                .kda = m.kda(),
                .damage_per_round = m.damage_per_round(),
                .rr_change = m.rr_change,
            });
        }

        sp.total_rr = total_rr;
        sp.avg_rr_per_game = sp.game_count > 0
            ? static_cast<double>(total_rr) / sp.game_count : 0.0;
        sp.avg_kda = sp.game_count > 0
            ? total_kda / sp.game_count : 0.0;

        result.push_back(std::move(sp));
    }

    return result;
}

std::vector<SessionPerformance> rr_by_session(
    const std::vector<Session>& sessions) {
    return performance_by_session(sessions);
}

std::vector<RollingMetric> rolling_kda(
    const std::vector<PlayerMatchSummary>& matches, int window) {

    std::vector<RollingMetric> result;
    int n = static_cast<int>(matches.size());
    int effective_window = std::min(window, n);

    for (int i = effective_window - 1; i < n; ++i) {
        double sum = 0.0;
        for (int j = i - effective_window + 1; j <= i; ++j) {
            sum += matches[j].kda();
        }
        result.push_back({
            .match_index = i,
            .match_id = matches[i].match_id,
            .value = sum / effective_window,
        });
    }

    return result;
}

std::vector<RollingMetric> rolling_win_rate(
    const std::vector<PlayerMatchSummary>& matches, int window) {

    std::vector<RollingMetric> result;
    int n = static_cast<int>(matches.size());
    int effective_window = std::min(window, n);

    for (int i = effective_window - 1; i < n; ++i) {
        int wins = 0;
        for (int j = i - effective_window + 1; j <= i; ++j) {
            wins += matches[j].won ? 1 : 0;
        }
        result.push_back({
            .match_index = i,
            .match_id = matches[i].match_id,
            .value = static_cast<double>(wins) / effective_window,
        });
    }

    return result;
}

DecayCurveModel decay_curve(
    const std::vector<Session>& sessions, int min_session_length) {

    std::unordered_map<int, std::vector<double>> by_position;

    for (auto& session : sessions) {
        if (session.game_count() < min_session_length) continue;
        for (int i = 0; i < static_cast<int>(session.matches.size()); ++i) {
            by_position[i + 1].push_back(session.matches[i].kda());
        }
    }

    DecayCurveModel model;
    for (auto& [pos, kdas] : by_position) {
        double avg = std::accumulate(kdas.begin(), kdas.end(), 0.0) / kdas.size();
        model.points.emplace_back(pos, avg);
    }

    std::ranges::sort(model.points, {}, &std::pair<int, double>::first);

    if (model.points.size() < 2) return model;

    // Least squares linear regression
    double n = static_cast<double>(model.points.size());
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
    for (auto& [x, y] : model.points) {
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_xx += x * x;
    }

    double denom = n * sum_xx - sum_x * sum_x;
    if (std::abs(denom) < 1e-10) return model;

    model.slope = (n * sum_xy - sum_x * sum_y) / denom;
    model.intercept = (sum_y - model.slope * sum_x) / n;

    // R²
    double y_mean = sum_y / n;
    double ss_tot = 0, ss_res = 0;
    for (auto& [x, y] : model.points) {
        double y_pred = model.slope * x + model.intercept;
        ss_tot += (y - y_mean) * (y - y_mean);
        ss_res += (y - y_pred) * (y - y_pred);
    }

    model.r_squared = ss_tot > 1e-10 ? 1.0 - ss_res / ss_tot : 0.0;
    return model;
}

std::vector<AgentPerformance> performance_by_agent(
    const std::vector<PlayerMatchSummary>& matches) {

    struct Acc {
        double total_kda = 0.0;
        double total_dpr = 0.0;
        int wins = 0;
        int count = 0;
    };

    std::unordered_map<std::string, Acc> by_agent;
    for (auto& m : matches) {
        auto& a = by_agent[m.agent];
        a.total_kda += m.kda();
        a.total_dpr += m.damage_per_round();
        a.wins += m.won ? 1 : 0;
        a.count++;
    }

    int total = static_cast<int>(matches.size());
    std::vector<AgentPerformance> result;
    for (auto& [agent, acc] : by_agent) {
        result.push_back({
            .agent = agent,
            .games = acc.count,
            .avg_kda = acc.total_kda / acc.count,
            .win_rate = static_cast<double>(acc.wins) / acc.count,
            .avg_damage_per_round = acc.total_dpr / acc.count,
            .pick_rate = total > 0 ? static_cast<double>(acc.count) / total : 0.0,
        });
    }

    std::ranges::sort(result, std::greater{}, &AgentPerformance::games);
    return result;
}

std::vector<MapPerformance> performance_by_map(
    const std::vector<PlayerMatchSummary>& matches) {

    struct Acc {
        double total_kda = 0.0;
        double total_score = 0.0;
        int wins = 0;
        int count = 0;
    };

    std::unordered_map<std::string, Acc> by_map;
    for (auto& m : matches) {
        auto& a = by_map[m.map];
        a.total_kda += m.kda();
        a.total_score += m.score;
        a.wins += m.won ? 1 : 0;
        a.count++;
    }

    std::vector<MapPerformance> result;
    for (auto& [map, acc] : by_map) {
        result.push_back({
            .map = map,
            .games = acc.count,
            .avg_kda = acc.total_kda / acc.count,
            .win_rate = static_cast<double>(acc.wins) / acc.count,
            .avg_score = acc.total_score / acc.count,
        });
    }

    std::ranges::sort(result, std::greater{}, &MapPerformance::games);
    return result;
}

OverviewStats compute_overview(
    const std::vector<PlayerMatchSummary>& matches,
    const std::vector<AgentPerformance>& agents,
    const std::vector<MapPerformance>& maps) {

    OverviewStats stats;
    stats.total_games = static_cast<int>(matches.size());

    int total_headshots = 0, total_bodyshots = 0, total_legshots = 0;

    for (auto& m : matches) {
        stats.total_kills += m.kills;
        stats.total_deaths += m.deaths;
        stats.total_assists += m.assists;
        stats.total_rr += m.rr_change;
        if (m.won) stats.wins++;
        else stats.losses++;
    }

    if (stats.total_deaths > 0) {
        stats.overall_kda = static_cast<double>(stats.total_kills + stats.total_assists)
                            / stats.total_deaths;
    } else {
        stats.overall_kda = static_cast<double>(stats.total_kills + stats.total_assists);
    }

    if (stats.total_games > 0) {
        stats.win_rate = static_cast<double>(stats.wins) / stats.total_games;
    }

    double total_dpr = 0.0;
    for (auto& m : matches) total_dpr += m.damage_per_round();
    if (stats.total_games > 0) stats.avg_damage_per_round = total_dpr / stats.total_games;

    // Best agent by KDA (min 3 games)
    for (auto& a : agents) {
        if (a.games >= 3 && a.avg_kda > stats.best_agent_kda) {
            stats.best_agent = a.agent;
            stats.best_agent_kda = a.avg_kda;
        }
    }

    // Worst map by WR (min 3 games)
    for (auto& m : maps) {
        if (m.games >= 3 && m.win_rate < stats.worst_map_wr) {
            stats.worst_map = m.map;
            stats.worst_map_wr = m.win_rate;
        }
    }

    // Streaks
    int cur_streak = 0;
    int max_win = 0, max_loss = 0;
    for (auto& m : matches) {
        if (m.won) {
            cur_streak = cur_streak > 0 ? cur_streak + 1 : 1;
            max_win = std::max(max_win, cur_streak);
        } else {
            cur_streak = cur_streak < 0 ? cur_streak - 1 : -1;
            max_loss = std::max(max_loss, std::abs(cur_streak));
        }
    }
    stats.longest_win_streak = max_win;
    stats.longest_loss_streak = max_loss;
    stats.current_streak = cur_streak;

    return stats;
}

} // namespace valorant
