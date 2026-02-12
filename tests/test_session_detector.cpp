#include <gtest/gtest.h>
#include "valorant/session_detector.hpp"

using namespace valorant;
using namespace std::chrono;

namespace {

PlayerMatchSummary make_match_at(int minute_offset, int duration_secs = 2400) {
    PlayerMatchSummary m;
    m.match_id = "match-" + std::to_string(minute_offset);
    m.game_start = system_clock::from_time_t(1700000000) + minutes(minute_offset);
    m.game_length_secs = duration_secs;
    m.kills = 15;
    m.deaths = 10;
    m.assists = 5;
    return m;
}

} // namespace

TEST(SessionDetector, EmptyInput) {
    auto result = detect_sessions({});
    EXPECT_TRUE(result.empty());
}

TEST(SessionDetector, SingleMatch) {
    std::vector<PlayerMatchSummary> matches = {make_match_at(0)};
    auto result = detect_sessions(matches);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].game_count(), 1);
}

TEST(SessionDetector, ConsecutiveMatchesSameSession) {
    // 3 matches, each 40 min long, starting 45 min apart (5 min gap < 45 min threshold)
    std::vector<PlayerMatchSummary> matches = {
        make_match_at(0, 2400),     // 0-40 min
        make_match_at(45, 2400),    // 45-85 min (5 min gap)
        make_match_at(90, 2400),    // 90-130 min (5 min gap)
    };
    auto result = detect_sessions(matches, minutes(45));
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].game_count(), 3);
}

TEST(SessionDetector, LargeGapSplitsSessions) {
    // Match 1: 0-40min, Match 2: 120-160min (80 min gap > 45 min threshold)
    std::vector<PlayerMatchSummary> matches = {
        make_match_at(0, 2400),
        make_match_at(120, 2400),
    };
    auto result = detect_sessions(matches, minutes(45));
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].game_count(), 1);
    EXPECT_EQ(result[1].game_count(), 1);
}

TEST(SessionDetector, ThreeDistinctSessions) {
    std::vector<PlayerMatchSummary> matches = {
        make_match_at(0, 2400),
        make_match_at(45, 2400),
        make_match_at(300, 2400),  // 5-hour gap
        make_match_at(345, 2400),
        make_match_at(390, 2400),
        make_match_at(800, 2400),  // another big gap
    };
    auto result = detect_sessions(matches, minutes(45));
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0].game_count(), 2);
    EXPECT_EQ(result[1].game_count(), 3);
    EXPECT_EQ(result[2].game_count(), 1);
}

TEST(SessionDetector, SessionIndicesSequential) {
    std::vector<PlayerMatchSummary> matches = {
        make_match_at(0),
        make_match_at(200),
        make_match_at(400),
    };
    auto result = detect_sessions(matches, minutes(45));
    for (int i = 0; i < static_cast<int>(result.size()); ++i) {
        EXPECT_EQ(result[i].index, i);
    }
}

TEST(SessionDetector, CustomGapThreshold) {
    // 30 min gap, with 20 min threshold should split
    std::vector<PlayerMatchSummary> matches = {
        make_match_at(0, 600),   // 0-10 min
        make_match_at(40, 600),  // 40-50 min (30 min gap)
    };
    auto result = detect_sessions(matches, minutes(20));
    EXPECT_EQ(result.size(), 2u);

    // Same data with 60 min threshold should not split
    result = detect_sessions(matches, minutes(60));
    EXPECT_EQ(result.size(), 1u);
}

TEST(SessionDetector, TotalMatchCountPreserved) {
    std::vector<PlayerMatchSummary> matches;
    for (int i = 0; i < 20; ++i) {
        matches.push_back(make_match_at(i * 100));
    }

    auto result = detect_sessions(matches, minutes(45));
    int total = 0;
    for (auto& s : result) total += s.game_count();
    EXPECT_EQ(total, 20);
}
