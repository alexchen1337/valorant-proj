#include "valorant/display.hpp"
#include <algorithm>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace valorant {

namespace {

std::string fmt_pct(double v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (v * 100.0) << "%";
    return oss.str();
}

std::string fmt_f2(double v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << v;
    return oss.str();
}

std::string fmt_rr(int rr) {
    if (rr > 0) return "+" + std::to_string(rr);
    return std::to_string(rr);
}

void print_sep(const std::vector<int>& widths) {
    std::cout << "+";
    for (int w : widths) {
        for (int i = 0; i < w + 2; ++i) std::cout << "-";
        std::cout << "+";
    }
    std::cout << "\n";
}

void print_row(const std::vector<std::string>& cols, const std::vector<int>& widths) {
    std::cout << "|";
    for (size_t i = 0; i < cols.size(); ++i) {
        std::cout << " " << std::left << std::setw(widths[i]) << cols[i] << " |";
    }
    std::cout << "\n";
}

} // namespace

void display_player_header(const PlayerIdentity& player, int match_count) {
    std::cout << "\n  Player: " << player.name << "#" << player.tag
              << "  |  Region: " << player.region
              << "  |  Matches: " << match_count << "\n\n";
}

void display_progress(int current, int total) {
    std::cerr << "\rFetching match " << current << "/" << total << "..." << std::flush;
    if (current == total) std::cerr << "\n";
}

void display_hourly(const std::vector<HourlyPerformance>& data, OutputFormat fmt) {
    if (data.empty()) {
        std::cout << "  No hourly data available.\n";
        return;
    }

    if (fmt == OutputFormat::Csv) {
        std::cout << "hour,avg_kda,win_rate,match_count\n";
        for (auto& h : data) {
            std::cout << h.hour << "," << fmt_f2(h.avg_kda) << ","
                      << fmt_f2(h.win_rate) << "," << h.match_count << "\n";
        }
        return;
    }

    auto best = std::ranges::max_element(data, {}, &HourlyPerformance::avg_kda);
    auto worst = std::ranges::min_element(data, {}, &HourlyPerformance::avg_kda);

    std::cout << "  === Performance by Time of Day ===\n\n";

    std::vector<int> widths = {6, 9, 9, 7, 5};
    std::vector<std::string> headers = {"Hour", "Avg KDA", "Win Rate", "Matches", "Note"};
    print_sep(widths);
    print_row(headers, widths);
    print_sep(widths);

    for (auto& h : data) {
        std::string note;
        if (&h == &*best) note = "BEST";
        if (&h == &*worst) note = "WORST";

        std::string hour_str = std::to_string(h.hour) + ":00";
        print_row({hour_str, fmt_f2(h.avg_kda), fmt_pct(h.win_rate),
                   std::to_string(h.match_count), note}, widths);
    }
    print_sep(widths);
    std::cout << "\n";
}

void display_sessions(const std::vector<SessionPerformance>& data, OutputFormat fmt) {
    if (data.empty()) {
        std::cout << "  No session data available.\n";
        return;
    }

    if (fmt == OutputFormat::Csv) {
        std::cout << "session,game_number,kda,damage_per_round,rr_change\n";
        for (auto& sp : data) {
            for (auto& g : sp.games) {
                std::cout << sp.session_index << "," << g.game_number << ","
                          << fmt_f2(g.kda) << "," << fmt_f2(g.damage_per_round) << ","
                          << g.rr_change << "\n";
            }
        }
        return;
    }

    std::cout << "  === Performance After Consecutive Games ===\n\n";

    for (auto& sp : data) {
        std::cout << "  Session " << sp.session_index + 1
                  << " (" << sp.game_count << " games, avg KDA: "
                  << fmt_f2(sp.avg_kda) << ")\n";

        std::vector<int> widths = {5, 7, 8, 4};
        std::vector<std::string> headers = {"Game", "KDA", "DMG/Rnd", "RR"};
        print_sep(widths);
        print_row(headers, widths);
        print_sep(widths);

        for (auto& g : sp.games) {
            print_row({std::to_string(g.game_number), fmt_f2(g.kda),
                       fmt_f2(g.damage_per_round), fmt_rr(g.rr_change)}, widths);
        }
        print_sep(widths);
        std::cout << "\n";
    }
}

void display_rr_sessions(const std::vector<SessionPerformance>& data, OutputFormat fmt) {
    if (data.empty()) {
        std::cout << "  No RR session data available.\n";
        return;
    }

    if (fmt == OutputFormat::Csv) {
        std::cout << "session,game_count,total_rr,avg_rr_per_game\n";
        for (auto& sp : data) {
            std::cout << sp.session_index << "," << sp.game_count << ","
                      << sp.total_rr << "," << fmt_f2(sp.avg_rr_per_game) << "\n";
        }
        return;
    }

    std::cout << "  === RR Change by Session Length ===\n\n";

    std::vector<int> widths = {8, 6, 9, 11};
    std::vector<std::string> headers = {"Session", "Games", "Total RR", "Avg RR/Game"};
    print_sep(widths);
    print_row(headers, widths);
    print_sep(widths);

    for (auto& sp : data) {
        print_row({std::to_string(sp.session_index + 1),
                   std::to_string(sp.game_count),
                   fmt_rr(sp.total_rr),
                   fmt_f2(sp.avg_rr_per_game)}, widths);
    }
    print_sep(widths);
    std::cout << "\n";
}

void display_rolling_kda(const std::vector<RollingMetric>& data, OutputFormat fmt) {
    if (data.empty()) {
        std::cout << "  Not enough matches for rolling KDA.\n";
        return;
    }

    if (fmt == OutputFormat::Csv) {
        std::cout << "match_index,rolling_kda\n";
        for (auto& m : data) {
            std::cout << m.match_index << "," << fmt_f2(m.value) << "\n";
        }
        return;
    }

    std::cout << "  === Rolling KDA ===\n\n";

    auto [min_it, max_it] = std::ranges::minmax_element(data, {}, &RollingMetric::value);

    std::vector<int> widths = {6, 12};
    print_sep(widths);
    print_row({"Match", "Rolling KDA"}, widths);
    print_sep(widths);

    for (auto& m : data) {
        std::string val = fmt_f2(m.value);
        if (&m == &*max_it) val += " ^";
        if (&m == &*min_it) val += " v";
        print_row({std::to_string(m.match_index + 1), val}, widths);
    }
    print_sep(widths);

    std::cout << "  Range: " << fmt_f2(min_it->value) << " - " << fmt_f2(max_it->value) << "\n\n";
}

void display_rolling_wr(const std::vector<RollingMetric>& data, OutputFormat fmt) {
    if (data.empty()) {
        std::cout << "  Not enough matches for rolling win rate.\n";
        return;
    }

    if (fmt == OutputFormat::Csv) {
        std::cout << "match_index,rolling_wr\n";
        for (auto& m : data) {
            std::cout << m.match_index << "," << fmt_f2(m.value) << "\n";
        }
        return;
    }

    std::cout << "  === Rolling Win Rate ===\n\n";

    auto [min_it, max_it] = std::ranges::minmax_element(data, {}, &RollingMetric::value);

    std::vector<int> widths = {6, 14};
    print_sep(widths);
    print_row({"Match", "Rolling WR"}, widths);
    print_sep(widths);

    for (auto& m : data) {
        std::string val = fmt_pct(m.value);
        if (&m == &*max_it) val += " ^";
        if (&m == &*min_it) val += " v";
        print_row({std::to_string(m.match_index + 1), val}, widths);
    }
    print_sep(widths);

    std::cout << "  Range: " << fmt_pct(min_it->value) << " - " << fmt_pct(max_it->value) << "\n\n";
}

void display_decay(const DecayCurveModel& model, OutputFormat fmt) {
    if (model.points.empty()) {
        std::cout << "  Not enough session data for decay analysis.\n";
        return;
    }

    if (fmt == OutputFormat::Csv) {
        std::cout << "game_position,avg_kda\n";
        for (auto& [pos, kda] : model.points) {
            std::cout << pos << "," << fmt_f2(kda) << "\n";
        }
        std::cout << "\n# slope=" << fmt_f2(model.slope)
                  << ",intercept=" << fmt_f2(model.intercept)
                  << ",r_squared=" << fmt_f2(model.r_squared) << "\n";
        return;
    }

    std::cout << "  === Fatigue Decay Curve ===\n\n";

    std::vector<int> widths = {9, 8};
    print_sep(widths);
    print_row({"Game #", "Avg KDA"}, widths);
    print_sep(widths);

    for (auto& [pos, kda] : model.points) {
        print_row({std::to_string(pos), fmt_f2(kda)}, widths);
    }
    print_sep(widths);

    std::cout << "\n  Linear Regression: KDA = " << fmt_f2(model.slope)
              << " * game_number + " << fmt_f2(model.intercept) << "\n";
    std::cout << "  R-squared: " << fmt_f2(model.r_squared) << "\n";

    if (model.slope < -0.05) {
        std::cout << "  Interpretation: FATIGUE DETECTED - KDA drops "
                  << fmt_f2(std::abs(model.slope)) << " per game in session\n";
    } else if (model.slope < 0) {
        std::cout << "  Interpretation: Mild fatigue trend (slope near zero)\n";
    } else {
        std::cout << "  Interpretation: No fatigue detected - performance stable or improving\n";
    }
    std::cout << "\n";
}

} // namespace valorant
