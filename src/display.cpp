#include "valorant/display.hpp"
#include "valorant/analytics.hpp"
#include "valorant/session_detector.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/table.hpp>
#include <ftxui/dom/canvas.hpp>

namespace valorant {

namespace {

using namespace ftxui;

// -- Formatting helpers --

std::string f2(double v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << v;
    return oss.str();
}

std::string f1(double v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << v;
    return oss.str();
}

std::string fpct(double v) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << (v * 100.0) << "%";
    return oss.str();
}

std::string frr(int rr) {
    if (rr > 0) return "+" + std::to_string(rr);
    return std::to_string(rr);
}

std::string hour12(int h24) {
    if (h24 == 0) return "12 AM";
    if (h24 < 12) return std::to_string(h24) + " AM";
    if (h24 == 12) return "12 PM";
    return std::to_string(h24 - 12) + " PM";
}

std::string streak_str(int s) {
    if (s > 0) return std::to_string(s) + "W";
    if (s < 0) return std::to_string(std::abs(s)) + "L";
    return "-";
}

Color kda_color(double kda) {
    if (kda >= 2.0) return Color::Green;
    if (kda >= 1.0) return Color::Yellow;
    return Color::Red;
}

Color wr_color(double wr) {
    if (wr >= 0.55) return Color::Green;
    if (wr >= 0.45) return Color::Yellow;
    return Color::Red;
}

Color rr_color(int rr) {
    if (rr > 0) return Color::Green;
    if (rr == 0) return Color::Yellow;
    return Color::Red;
}

Color streak_color(int s) {
    if (s > 0) return Color::Green;
    if (s < 0) return Color::Red;
    return Color::GrayDark;
}

// -- Graph helpers --

Element make_line_graph(const std::vector<double>& values, const std::string& title,
                        int width = 80, int height = 15,
                        Color line_color = Color::Cyan) {
    if (values.empty()) return text("No data") | dim;

    auto [min_it, max_it] = std::ranges::minmax_element(values);
    double vmin = *min_it;
    double vmax = *max_it;
    double vspan = vmax - vmin;
    if (vspan < 0.001) vspan = 1.0;

    int canvas_w = width * 2;
    int canvas_h = height * 4;

    auto c = Canvas(canvas_w, canvas_h);
    int n = static_cast<int>(values.size());
    double x_step = n > 1 ? static_cast<double>(canvas_w - 4) / (n - 1) : 0;

    for (int i = 1; i < n; ++i) {
        int x0 = static_cast<int>((i - 1) * x_step) + 2;
        int y0 = canvas_h - 2 - static_cast<int>(((values[i - 1] - vmin) / vspan) * (canvas_h - 4));
        int x1 = static_cast<int>(i * x_step) + 2;
        int y1 = canvas_h - 2 - static_cast<int>(((values[i] - vmin) / vspan) * (canvas_h - 4));
        c.DrawPointLine(x0, y0, x1, y1, line_color);
    }

    return vbox({
        text(title) | bold | color(Color::Cyan),
        hbox({
            text(f2(vmax) + " ") | dim | size(WIDTH, EQUAL, 8),
            canvas(std::move(c)),
        }),
        hbox({
            text(f2(vmin) + " ") | dim | size(WIDTH, EQUAL, 8),
            text(std::string(width - 8, ' ')),
        }),
    });
}

Element make_bar_chart(const std::vector<std::pair<std::string, double>>& bars,
                       double max_val, Color bar_color = Color::Cyan) {
    if (bars.empty()) return text("No data") | dim;

    if (max_val <= 0) {
        for (auto& [_, v] : bars) max_val = std::max(max_val, v);
    }
    if (max_val <= 0) max_val = 1.0;

    Elements rows;
    for (auto& [label, val] : bars) {
        float pct = static_cast<float>(val / max_val);
        auto c = val >= max_val * 0.8 ? Color::Green :
                 val >= max_val * 0.4 ? Color::Yellow : Color::Red;
        rows.push_back(hbox({
            text(label) | size(WIDTH, EQUAL, 8),
            gauge(pct) | size(WIDTH, EQUAL, 25) | color(c),
            text(" " + f2(val)) | dim,
        }));
    }
    return vbox(rows);
}

// -- Render functions --

Element render_header(const ReportData& data) {
    return hbox({
        text(" " + data.player.name + "#" + data.player.tag) | bold,
        text("  |  "),
        text("Region: " + data.player.region),
        text("  |  "),
        text("Matches: " + std::to_string(data.match_count)),
    }) | borderLight | color(Color::Cyan);
}

Element render_overview(const OverviewStats& stats) {
    auto stat_box = [](const std::string& label, const std::string& value, Color c) {
        return vbox({
            text(value) | bold | color(c) | center,
            text(label) | dim | center,
        }) | size(WIDTH, EQUAL, 14) | borderLight;
    };

    auto streak_c = streak_color(stats.current_streak);

    return vbox({
        text("Overview") | bold | color(Color::Cyan),
        separator(),
        text(""),
        hbox({
            stat_box("Win Rate", fpct(stats.win_rate), wr_color(stats.win_rate)),
            stat_box("KDA", f2(stats.overall_kda), kda_color(stats.overall_kda)),
            stat_box("Avg DMG/Rnd", f1(stats.avg_damage_per_round), Color::White),
            stat_box("Total RR", frr(stats.total_rr), rr_color(stats.total_rr)),
        }),
        text(""),
        hbox({
            stat_box("Wins", std::to_string(stats.wins), Color::Green),
            stat_box("Losses", std::to_string(stats.losses), Color::Red),
            stat_box("Streak", streak_str(stats.current_streak), streak_c),
            stat_box("Games", std::to_string(stats.total_games), Color::White),
        }),
        text(""),
        hbox({
            stat_box("K/D/A",
                std::to_string(stats.total_kills) + "/" +
                std::to_string(stats.total_deaths) + "/" +
                std::to_string(stats.total_assists), Color::White),
            stat_box("Best Win Streak", std::to_string(stats.longest_win_streak), Color::Green),
            stat_box("Worst Loss Str", std::to_string(stats.longest_loss_streak), Color::Red),
        }),
        text(""),
        !stats.best_agent.empty() ?
            hbox({
                text("  Best Agent: ") | dim,
                text(stats.best_agent) | bold | color(Color::Green),
                text(" (" + f2(stats.best_agent_kda) + " KDA)") | dim,
                text("    Weakest Map: ") | dim,
                text(stats.worst_map) | bold | color(Color::Red),
                text(" (" + fpct(stats.worst_map_wr) + " WR)") | dim,
            }) : text(""),
    });
}

Element render_hourly(const std::vector<HourlyPerformance>& data) {
    if (data.empty()) return text("No hourly data available.") | dim;

    auto best = std::ranges::max_element(data, {}, &HourlyPerformance::avg_kda);
    auto worst = std::ranges::min_element(data, {}, &HourlyPerformance::avg_kda);

    // Table
    std::vector<std::vector<std::string>> rows;
    rows.push_back({"Hour", "Avg KDA", "Win Rate", "Matches", ""});
    for (auto& h : data) {
        std::string note;
        if (&h == &*best) note = "^ BEST";
        if (&h == &*worst) note = "v WORST";
        rows.push_back({
            hour12(h.hour), f2(h.avg_kda), fpct(h.win_rate),
            std::to_string(h.match_count), note,
        });
    }

    auto table = Table(rows);
    table.SelectRow(0).Decorate(bold);
    table.SelectRow(0).SeparatorVertical(LIGHT);
    table.SelectAll().Border(LIGHT);

    for (size_t i = 1; i < rows.size(); ++i) {
        auto& h = data[i - 1];
        table.SelectCell(1, i).Decorate(color(kda_color(h.avg_kda)));
        table.SelectCell(2, i).Decorate(color(wr_color(h.win_rate)));
    }

    // Bar chart for KDA by hour
    std::vector<std::pair<std::string, double>> kda_bars;
    for (auto& h : data) {
        kda_bars.emplace_back(hour12(h.hour), h.avg_kda);
    }
    double max_kda = best->avg_kda;

    // Bar chart for WR by hour
    std::vector<std::pair<std::string, double>> wr_bars;
    for (auto& h : data) {
        wr_bars.emplace_back(hour12(h.hour), h.win_rate * 100.0);
    }

    return vbox({
        text("Performance by Time of Day") | bold | color(Color::Cyan),
        separator(),
        table.Render(),
        text(""),
        text("  KDA by Hour") | bold,
        make_bar_chart(kda_bars, max_kda * 1.1),
        text(""),
        text("  Win Rate by Hour") | bold,
        make_bar_chart(wr_bars, 100.0),
    });
}

Element render_sessions(const std::vector<SessionPerformance>& data) {
    if (data.empty()) return text("No session data available.") | dim;

    Elements session_blocks;
    for (auto& sp : data) {
        std::vector<std::vector<std::string>> rows;
        rows.push_back({"Game", "KDA", "DMG/Rnd", "RR"});
        for (auto& g : sp.games) {
            rows.push_back({
                std::to_string(g.game_number), f2(g.kda),
                f2(g.damage_per_round), frr(g.rr_change),
            });
        }

        auto table = Table(rows);
        table.SelectRow(0).Decorate(bold);
        table.SelectAll().Border(LIGHT);

        for (size_t i = 1; i < rows.size(); ++i) {
            auto& g = sp.games[i - 1];
            table.SelectCell(1, i).Decorate(color(kda_color(g.kda)));
            table.SelectCell(3, i).Decorate(color(rr_color(g.rr_change)));
        }

        session_blocks.push_back(vbox({
            hbox({
                text("Session " + std::to_string(sp.session_index + 1)) | bold,
                text("  " + std::to_string(sp.game_count) + " games") | dim,
                text("  avg KDA: " + f2(sp.avg_kda)) | color(kda_color(sp.avg_kda)),
                text("  total RR: " + frr(sp.total_rr)) | color(rr_color(sp.total_rr)),
            }),
            table.Render(),
            text(""),
        }));
    }

    return vbox({
        text("Performance After Consecutive Games") | bold | color(Color::Cyan),
        separator(),
        vbox(session_blocks),
    });
}

Element render_rr_sessions(const std::vector<SessionPerformance>& data) {
    if (data.empty()) return text("No RR session data available.") | dim;

    std::vector<std::vector<std::string>> rows;
    rows.push_back({"Session", "Games", "Total RR", "Avg RR/Game"});
    for (auto& sp : data) {
        rows.push_back({
            std::to_string(sp.session_index + 1),
            std::to_string(sp.game_count),
            frr(sp.total_rr), f2(sp.avg_rr_per_game),
        });
    }

    auto table = Table(rows);
    table.SelectRow(0).Decorate(bold);
    table.SelectAll().Border(LIGHT);

    for (size_t i = 1; i < rows.size(); ++i) {
        table.SelectCell(2, i).Decorate(color(rr_color(data[i - 1].total_rr)));
    }

    // Bar chart of total RR per session
    std::vector<std::pair<std::string, double>> rr_bars;
    double max_abs_rr = 1.0;
    for (auto& sp : data) {
        max_abs_rr = std::max(max_abs_rr, std::abs(static_cast<double>(sp.total_rr)));
    }
    Elements rr_rows;
    for (auto& sp : data) {
        auto c = rr_color(sp.total_rr);
        float pct = static_cast<float>(std::abs(sp.total_rr) / max_abs_rr);
        rr_rows.push_back(hbox({
            text("S" + std::to_string(sp.session_index + 1)) | size(WIDTH, EQUAL, 5),
            gauge(pct) | size(WIDTH, EQUAL, 25) | color(c),
            text(" " + frr(sp.total_rr)) | color(c),
        }));
    }

    return vbox({
        text("RR Change by Session Length") | bold | color(Color::Cyan),
        separator(),
        table.Render(),
        text(""),
        text("  RR per Session") | bold,
        vbox(rr_rows),
    });
}

Element render_rolling_kda(const std::vector<RollingMetric>& data) {
    if (data.empty()) return text("Not enough matches for rolling KDA.") | dim;

    auto [min_it, max_it] = std::ranges::minmax_element(data, {}, &RollingMetric::value);

    // Line graph
    std::vector<double> values;
    for (auto& m : data) values.push_back(m.value);
    auto graph = make_line_graph(values, "Rolling KDA Trend", 70, 12, Color::Cyan);

    // Sparkline bars
    double range_min = min_it->value;
    double range_max = max_it->value;
    double span = range_max - range_min;
    if (span < 0.01) span = 1.0;

    constexpr int bar_width = 25;
    Elements rows;
    for (auto& m : data) {
        auto c = kda_color(m.value);
        std::string label;
        if (&m == &*max_it) label = " ^ peak";
        if (&m == &*min_it) label = " v low";

        rows.push_back(hbox({
            text(std::to_string(m.match_index + 1)) | size(WIDTH, EQUAL, 5) | dim,
            gauge(static_cast<float>((m.value - range_min) / span))
                | size(WIDTH, EQUAL, bar_width) | color(c),
            text(" " + f2(m.value)) | color(c),
            text(label) | bold,
        }));
    }

    return vbox({
        graph,
        text(""),
        separator(),
        text("  Match Detail") | bold,
        text("  Range: " + f2(range_min) + " - " + f2(range_max)) | dim,
        vbox(rows),
    });
}

Element render_rolling_wr(const std::vector<RollingMetric>& data) {
    if (data.empty()) return text("Not enough matches for rolling win rate.") | dim;

    auto [min_it, max_it] = std::ranges::minmax_element(data, {}, &RollingMetric::value);

    // Line graph
    std::vector<double> values;
    for (auto& m : data) values.push_back(m.value * 100.0);
    auto graph = make_line_graph(values, "Rolling Win Rate Trend (%)", 70, 12, Color::Green);

    constexpr int bar_width = 25;
    Elements rows;
    for (auto& m : data) {
        auto c = wr_color(m.value);
        std::string label;
        if (&m == &*max_it) label = " ^ peak";
        if (&m == &*min_it) label = " v low";

        rows.push_back(hbox({
            text(std::to_string(m.match_index + 1)) | size(WIDTH, EQUAL, 5) | dim,
            gauge(static_cast<float>(m.value))
                | size(WIDTH, EQUAL, bar_width) | color(c),
            text(" " + fpct(m.value)) | color(c),
            text(label) | bold,
        }));
    }

    return vbox({
        graph,
        text(""),
        separator(),
        text("  Match Detail") | bold,
        text("  Range: " + fpct(min_it->value) + " - " + fpct(max_it->value)) | dim,
        vbox(rows),
    });
}

Element render_decay(const DecayCurveModel& model) {
    if (model.points.empty()) return text("Not enough data for decay analysis.") | dim;

    // Line graph of decay
    std::vector<double> values;
    for (auto& [pos, kda] : model.points) values.push_back(kda);
    auto graph = make_line_graph(values, "KDA Decay by Game Position in Session",
                                 70, 12, Color::Red);

    // Table
    std::vector<std::vector<std::string>> rows;
    rows.push_back({"Game #", "Avg KDA", "Predicted"});
    for (auto& [pos, kda] : model.points) {
        double predicted = model.slope * pos + model.intercept;
        rows.push_back({std::to_string(pos), f2(kda), f2(predicted)});
    }

    auto table = Table(rows);
    table.SelectRow(0).Decorate(bold);
    table.SelectAll().Border(LIGHT);

    for (size_t i = 1; i < rows.size(); ++i) {
        table.SelectCell(1, i).Decorate(color(kda_color(model.points[i - 1].second)));
    }

    std::string interpretation;
    Color interp_color;
    if (model.slope < -0.05) {
        interpretation = "FATIGUE DETECTED - KDA drops " + f2(std::abs(model.slope)) + " per game in session";
        interp_color = Color::Red;
    } else if (model.slope < 0) {
        interpretation = "Mild fatigue trend (slope near zero)";
        interp_color = Color::Yellow;
    } else {
        interpretation = "No fatigue detected - performance stable or improving";
        interp_color = Color::Green;
    }

    return vbox({
        graph,
        text(""),
        separator(),
        table.Render(),
        text(""),
        hbox({
            text("  Regression: ") | dim,
            text("KDA = " + f2(model.slope) + " * game + " + f2(model.intercept)),
        }),
        hbox({
            text("  R-squared: ") | dim,
            text(f2(model.r_squared)),
        }),
        text(""),
        text("  " + interpretation) | bold | color(interp_color),
    });
}

Element render_agents(const std::vector<AgentPerformance>& data) {
    if (data.empty()) return text("No agent data available.") | dim;

    std::vector<std::vector<std::string>> rows;
    rows.push_back({"Agent", "Games", "KDA", "Win Rate", "DMG/Rnd", "Pick %"});
    for (auto& a : data) {
        rows.push_back({
            a.agent, std::to_string(a.games), f2(a.avg_kda),
            fpct(a.win_rate), f1(a.avg_damage_per_round), fpct(a.pick_rate),
        });
    }

    auto table = Table(rows);
    table.SelectRow(0).Decorate(bold);
    table.SelectRow(0).SeparatorVertical(LIGHT);
    table.SelectAll().Border(LIGHT);

    for (size_t i = 1; i < rows.size(); ++i) {
        auto& a = data[i - 1];
        table.SelectCell(2, i).Decorate(color(kda_color(a.avg_kda)));
        table.SelectCell(3, i).Decorate(color(wr_color(a.win_rate)));
    }

    // KDA bar chart
    double max_kda = 0;
    for (auto& a : data) max_kda = std::max(max_kda, a.avg_kda);

    std::vector<std::pair<std::string, double>> kda_bars;
    for (auto& a : data) kda_bars.emplace_back(a.agent, a.avg_kda);

    return vbox({
        text("Agent Performance") | bold | color(Color::Cyan),
        separator(),
        table.Render(),
        text(""),
        text("  KDA by Agent") | bold,
        make_bar_chart(kda_bars, max_kda * 1.1),
    });
}

Element render_maps(const std::vector<MapPerformance>& data) {
    if (data.empty()) return text("No map data available.") | dim;

    std::vector<std::vector<std::string>> rows;
    rows.push_back({"Map", "Games", "KDA", "Win Rate", "Avg Score"});
    for (auto& m : data) {
        rows.push_back({
            m.map, std::to_string(m.games), f2(m.avg_kda),
            fpct(m.win_rate), f1(m.avg_score),
        });
    }

    auto table = Table(rows);
    table.SelectRow(0).Decorate(bold);
    table.SelectRow(0).SeparatorVertical(LIGHT);
    table.SelectAll().Border(LIGHT);

    for (size_t i = 1; i < rows.size(); ++i) {
        auto& m = data[i - 1];
        table.SelectCell(2, i).Decorate(color(kda_color(m.avg_kda)));
        table.SelectCell(3, i).Decorate(color(wr_color(m.win_rate)));
    }

    // Win rate bar chart
    std::vector<std::pair<std::string, double>> wr_bars;
    for (auto& m : data) wr_bars.emplace_back(m.map, m.win_rate * 100.0);

    return vbox({
        text("Map Performance") | bold | color(Color::Cyan),
        separator(),
        table.Render(),
        text(""),
        text("  Win Rate by Map") | bold,
        make_bar_chart(wr_bars, 100.0),
    });
}

// -- Report view --

void show_report(ScreenInteractive& screen, const ReportData& data) {
    int selected_tab = 0;
    std::vector<std::string> tab_labels = {
        " Overview      ",
        " Hourly        ",
        " Agents        ",
        " Maps          ",
        " Sessions      ",
        " RR            ",
        " Rolling KDA   ",
        " Rolling WR    ",
        " Decay         ",
    };

    auto menu_option = MenuOption::Vertical();
    menu_option.entries_option.transform = [](const EntryState& state) {
        auto elem = text(state.label);
        if (state.focused) {
            elem = elem | bold | color(Color::Cyan) | inverted;
        } else if (state.active) {
            elem = elem | bold | color(Color::Cyan);
        } else {
            elem = elem | dim;
        }
        return elem;
    };

    auto menu = Menu(&tab_labels, &selected_tab, menu_option);

    auto content_renderer = Renderer([&] {
        switch (selected_tab) {
            case 0: return render_overview(data.overview);
            case 1: return render_hourly(data.hourly);
            case 2: return render_agents(data.agents);
            case 3: return render_maps(data.maps);
            case 4: return render_sessions(data.sessions);
            case 5: return render_rr_sessions(data.rr_sessions);
            case 6: return render_rolling_kda(data.rolling_kda);
            case 7: return render_rolling_wr(data.rolling_wr);
            case 8: return render_decay(data.decay);
            default: return text("Unknown tab") | dim;
        }
    });

    auto layout = Container::Horizontal({menu, content_renderer});

    auto renderer = Renderer(layout, [&] {
        return vbox({
            render_header(data),
            separator(),
            hbox({
                vbox({
                    text(" Reports") | bold,
                    separator(),
                    menu->Render() | vscroll_indicator | yframe |
                        size(WIDTH, EQUAL, 18),
                }) | borderLight,
                separator(),
                content_renderer->Render() | flex | yframe | xflex,
            }) | flex,
            separator(),
            hbox({
                text(" [↑/↓] Navigate") | dim,
                text("  [Tab] Switch pane") | dim,
                text("  [b] Back") | dim,
                text("  [q] Quit") | dim,
            }),
        }) | borderHeavy | color(Color::White);
    });

    auto with_keys = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Character('q') || event == Event::Escape) {
            screen.Exit();
            return true;
        }
        if (event == Event::Character('b')) {
            screen.Exit();
            return true;
        }
        return false;
    });

    screen.Loop(with_keys);
}

} // namespace

// -- Main app loop --

void run_app(const AppConfig& config) {
    RateLimiter limiter;
    Cache cache;

    while (true) {
        auto screen = ScreenInteractive::Fullscreen();

        std::string search_input;
        std::string status_msg;
        bool searching = false;

        auto input_option = InputOption();
        input_option.placeholder = "name#tag";
        input_option.on_enter = [&] {
            if (search_input.empty()) return;
            auto hash_pos = search_input.find('#');
            if (hash_pos == std::string::npos || hash_pos == 0 ||
                hash_pos == search_input.size() - 1) {
                status_msg = "Invalid format. Use name#tag";
                return;
            }
            searching = true;
            status_msg = "";
            screen.Exit();
        };

        auto input = Input(&search_input, input_option);

        auto search_renderer = Renderer(input, [&] {
            Elements content;
            content.push_back(text(""));
            content.push_back(
                text("  VALORANT Fatigue Analyzer") | bold | color(Color::Cyan));
            content.push_back(text(""));
            content.push_back(text("  Enter player name#tag:") | dim);
            content.push_back(text(""));
            content.push_back(
                hbox({text("  > "), input->Render() | size(WIDTH, EQUAL, 40)}) | borderLight);
            content.push_back(text(""));

            if (!status_msg.empty()) {
                content.push_back(text("  " + status_msg) | color(Color::Red));
            }

            content.push_back(text(""));
            content.push_back(text("  [Enter] Search  [Esc] Quit") | dim);

            return vbox(content) | borderHeavy | color(Color::White) | center;
        });

        auto with_quit = CatchEvent(search_renderer, [&](Event event) {
            if (event == Event::Escape) {
                screen.Exit();
                return true;
            }
            return false;
        });

        screen.Loop(with_quit);

        if (!searching) break;

        auto hash_pos = search_input.find('#');
        std::string name = search_input.substr(0, hash_pos);
        std::string tag = search_input.substr(hash_pos + 1);

        // Loading screen
        auto loading_screen = ScreenInteractive::Fullscreen();
        std::atomic<bool> done{false};
        std::string load_status = "Looking up " + name + "#" + tag + "...";
        std::string error_msg;
        std::optional<ReportData> report;

        std::thread worker([&] {
            auto account = fetch_account(config.client, limiter, name, tag);
            if (!account) {
                error_msg = "Account not found: " + account.error().message;
                done = true;
                loading_screen.Post(Event::Custom);
                return;
            }

            load_status = "Fetching matches (up to " +
                          std::to_string(config.match_count) + ")...";
            loading_screen.Post(Event::Custom);

            auto matches = fetch_stored_matches(
                config.client, limiter, config.region, name, tag,
                config.match_count,
                [&](int current, int total) {
                    load_status = "Fetched " + std::to_string(current) +
                                  "/" + std::to_string(total) + " matches...";
                    loading_screen.Post(Event::Custom);
                });

            if (!matches || matches->empty()) {
                error_msg = matches ? "No competitive matches found."
                                    : "Error: " + matches.error().message;
                done = true;
                loading_screen.Post(Event::Custom);
                return;
            }

            load_status = "Fetching MMR history...";
            loading_screen.Post(Event::Custom);

            auto mmr_history = fetch_mmr_history(
                config.client, limiter, cache, config.region, name, tag,
                account->puuid);

            if (mmr_history) {
                apply_rr_to_summaries(*matches, *mmr_history);
            }

            load_status = "Computing analytics...";
            loading_screen.Post(Event::Custom);

            auto sessions = detect_sessions(
                *matches, std::chrono::minutes(config.gap_minutes));

            auto agents = performance_by_agent(*matches);
            auto maps = performance_by_map(*matches);

            report = ReportData{
                .player = *account,
                .match_count = static_cast<int>(matches->size()),
                .overview = compute_overview(*matches, agents, maps),
                .hourly = performance_by_hour(*matches),
                .sessions = performance_by_session(sessions),
                .rr_sessions = rr_by_session(sessions),
                .rolling_kda = rolling_kda(*matches, config.window),
                .rolling_wr = rolling_win_rate(*matches, config.window),
                .decay = decay_curve(sessions),
                .agents = std::move(agents),
                .maps = std::move(maps),
            };

            done = true;
            loading_screen.Post(Event::Custom);
        });

        auto loading_renderer = Renderer([&] {
            Elements content;
            content.push_back(text(""));
            content.push_back(
                text("  VALORANT Fatigue Analyzer") | bold | color(Color::Cyan));
            content.push_back(text(""));

            if (!error_msg.empty()) {
                content.push_back(text("  " + error_msg) | color(Color::Red));
                content.push_back(text(""));
                content.push_back(text("  Press any key to go back...") | dim);
            } else {
                content.push_back(text("  " + load_status) | bold);
                content.push_back(text(""));
                content.push_back(spinner(18, 0) | color(Color::Cyan));
            }

            return vbox(content) | borderHeavy | color(Color::White) | center;
        });

        auto loading_events = CatchEvent(loading_renderer, [&](Event event) {
            if (done) {
                loading_screen.Exit();
                return true;
            }
            if (event == Event::Escape) {
                done = true;
                loading_screen.Exit();
                return true;
            }
            return false;
        });

        loading_screen.Loop(loading_events);
        worker.join();

        if (report) {
            auto report_screen = ScreenInteractive::Fullscreen();
            show_report(report_screen, *report);
        }
    }
}

} // namespace valorant
