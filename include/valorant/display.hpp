#pragma once

#include "valorant/api_client.hpp"
#include "valorant/cache.hpp"
#include "valorant/rate_limiter.hpp"
#include "valorant/types.hpp"
#include <string>
#include <vector>

namespace valorant {

struct ReportData {
    PlayerIdentity player;
    int match_count = 0;
    OverviewStats overview;
    std::vector<HourlyPerformance> hourly;
    std::vector<SessionPerformance> sessions;
    std::vector<SessionPerformance> rr_sessions;
    std::vector<RollingMetric> rolling_kda;
    std::vector<RollingMetric> rolling_wr;
    DecayCurveModel decay;
    std::vector<AgentPerformance> agents;
    std::vector<MapPerformance> maps;
};

struct AppConfig {
    ClientConfig client;
    std::string region = "na";
    int match_count = 200;
    int window = 20;
    int gap_minutes = 45;
};

void run_app(const AppConfig& config);

} // namespace valorant
