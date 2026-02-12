#include <gtest/gtest.h>
#include "valorant/analytics.hpp"
#include "valorant/session_detector.hpp"

using namespace valorant;
using namespace std::chrono;

namespace {

PlayerMatchSummary make_match(int kills, int deaths, int assists, bool won,
                              int rr, int hour, int game_len_secs = 2400,
                              int damage = 3000, int rounds = 24) {
    PlayerMatchSummary m;
    m.match_id = "match-" + std::to_string(rand());
    m.kills = kills;
    m.deaths = deaths;
    m.assists = assists;
    m.won = won;
    m.rr_change = rr;
    m.rr_available = true;
    m.damage_made = damage;
    m.rounds_played = rounds;
    m.game_length_secs = game_len_secs;

    auto base = system_clock::from_time_t(1700000000); // fixed epoch
    m.game_start = base + hours(hour);
    return m;
}

std::vector<PlayerMatchSummary> make_session_matches(int count, int start_hour = 0) {
    std::vector<PlayerMatchSummary> matches;
    for (int i = 0; i < count; ++i) {
        int kills = 20 - i * 2;
        int deaths = 10 + i;
        matches.push_back(make_match(kills, deaths, 5, kills > deaths,
                                     kills > deaths ? 15 : -15,
                                     start_hour + i));
    }
    return matches;
}

} // namespace

TEST(Analytics, KdaCalculation) {
    PlayerMatchSummary m;
    m.kills = 20;
    m.deaths = 10;
    m.assists = 5;
    EXPECT_DOUBLE_EQ(m.kda(), 2.5);
}

TEST(Analytics, KdaZeroDeaths) {
    PlayerMatchSummary m;
    m.kills = 15;
    m.deaths = 0;
    m.assists = 3;
    EXPECT_DOUBLE_EQ(m.kda(), 18.0);
}

TEST(Analytics, DamagePerRound) {
    PlayerMatchSummary m;
    m.damage_made = 3600;
    m.rounds_played = 24;
    EXPECT_DOUBLE_EQ(m.damage_per_round(), 150.0);
}

TEST(Analytics, DamagePerRoundZeroRounds) {
    PlayerMatchSummary m;
    m.damage_made = 100;
    m.rounds_played = 0;
    EXPECT_DOUBLE_EQ(m.damage_per_round(), 0.0);
}

TEST(PerformanceByHour, BucketsMatchesByHour) {
    auto base = system_clock::from_time_t(0);
    std::vector<PlayerMatchSummary> matches;

    // Create matches at specific local hours
    for (int i = 0; i < 5; ++i) {
        auto m = make_match(20, 10, 5, true, 15, i * 24 + 14); // hour 14 each day
        matches.push_back(m);
    }

    auto result = performance_by_hour(matches);
    ASSERT_FALSE(result.empty());

    int total = 0;
    for (auto& h : result) total += h.match_count;
    EXPECT_EQ(total, 5);
}

TEST(PerformanceByHour, EmptyInput) {
    auto result = performance_by_hour({});
    EXPECT_TRUE(result.empty());
}

TEST(RollingKda, WindowSmallerThanData) {
    auto matches = make_session_matches(10);
    auto result = rolling_kda(matches, 5);
    EXPECT_EQ(result.size(), 6u); // 10 - 5 + 1
}

TEST(RollingKda, WindowLargerThanData) {
    auto matches = make_session_matches(3);
    auto result = rolling_kda(matches, 20);
    EXPECT_EQ(result.size(), 1u); // uses effective window = 3
}

TEST(RollingKda, ValuesInReasonableRange) {
    auto matches = make_session_matches(10);
    auto result = rolling_kda(matches, 5);
    for (auto& m : result) {
        EXPECT_GE(m.value, 0.0);
        EXPECT_LE(m.value, 50.0);
    }
}

TEST(RollingWinRate, BoundedZeroOne) {
    auto matches = make_session_matches(10);
    auto result = rolling_win_rate(matches, 5);
    for (auto& m : result) {
        EXPECT_GE(m.value, 0.0);
        EXPECT_LE(m.value, 1.0);
    }
}

TEST(RollingWinRate, AllWins) {
    std::vector<PlayerMatchSummary> matches;
    for (int i = 0; i < 10; ++i) {
        matches.push_back(make_match(20, 10, 5, true, 15, i));
    }
    auto result = rolling_win_rate(matches, 5);
    for (auto& m : result) {
        EXPECT_DOUBLE_EQ(m.value, 1.0);
    }
}

TEST(RollingWinRate, AllLosses) {
    std::vector<PlayerMatchSummary> matches;
    for (int i = 0; i < 10; ++i) {
        matches.push_back(make_match(5, 15, 2, false, -15, i));
    }
    auto result = rolling_win_rate(matches, 5);
    for (auto& m : result) {
        EXPECT_DOUBLE_EQ(m.value, 0.0);
    }
}

TEST(SessionPerformance, CorrectGameCount) {
    auto matches = make_session_matches(5);
    auto sessions = detect_sessions(matches);
    auto result = performance_by_session(sessions);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].game_count, 5);
    EXPECT_EQ(result[0].games.size(), 5u);
}

TEST(SessionPerformance, GameNumbersSequential) {
    auto matches = make_session_matches(4);
    auto sessions = detect_sessions(matches);
    auto result = performance_by_session(sessions);
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(result[0].games[i].game_number, i + 1);
    }
}

TEST(DecayCurve, NegativeSlopeForFatigueData) {
    // Build sessions with declining performance
    std::vector<Session> sessions;
    for (int s = 0; s < 5; ++s) {
        Session session;
        session.index = s;
        session.matches = make_session_matches(5, s * 10);
        sessions.push_back(session);
    }

    auto result = decay_curve(sessions, 3);
    EXPECT_LT(result.slope, 0.0);
    EXPECT_FALSE(result.points.empty());
    EXPECT_GE(result.r_squared, 0.0);
    EXPECT_LE(result.r_squared, 1.0);
}

TEST(DecayCurve, PointsSortedByPosition) {
    std::vector<Session> sessions;
    for (int s = 0; s < 3; ++s) {
        Session session;
        session.index = s;
        session.matches = make_session_matches(4, s * 10);
        sessions.push_back(session);
    }

    auto result = decay_curve(sessions, 3);
    for (size_t i = 1; i < result.points.size(); ++i) {
        EXPECT_GT(result.points[i].first, result.points[i - 1].first);
    }
}

TEST(DecayCurve, SkipsShortSessions) {
    std::vector<Session> sessions;
    Session short_session;
    short_session.index = 0;
    short_session.matches = make_session_matches(2);
    sessions.push_back(short_session);

    auto result = decay_curve(sessions, 3);
    EXPECT_TRUE(result.points.empty());
}

TEST(RrBySession, TotalRrSumsCorrectly) {
    auto matches = make_session_matches(4);
    auto sessions = detect_sessions(matches);
    auto result = rr_by_session(sessions);
    ASSERT_EQ(result.size(), 1u);

    int expected_total = 0;
    for (auto& m : matches) expected_total += m.rr_change;
    EXPECT_EQ(result[0].total_rr, expected_total);
}
