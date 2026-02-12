#include "valorant/analytics.hpp"
#include "valorant/api_client.hpp"
#include "valorant/cache.hpp"
#include "valorant/display.hpp"
#include "valorant/rate_limiter.hpp"
#include "valorant/session_detector.hpp"
#include "valorant/types.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_set>

namespace {

struct CliArgs {
    std::string name;
    std::string tag;
    std::string region = "na";
    int matches = 50;
    int window = 20;
    int gap_minutes = 45;
    valorant::OutputFormat format = valorant::OutputFormat::Table;
    std::string api_key;
    std::unordered_set<std::string> reports;
};

void print_usage() {
    std::cerr << R"(Usage: valorant-fatigue <name> <tag> [options]
  --region <na|eu|ap|kr>    Region (default: na)
  --matches <n>             Number of matches (default: 50)
  --window <n>              Rolling window size (default: 20)
  --gap <minutes>           Session gap threshold (default: 45)
  --format <table|csv>      Output format (default: table)
  --api-key <key>           API key (or set VALORANT_API_KEY env var)
  --report <type>           hourly|sessions|rr|rolling-kda|rolling-wr|decay|all
)";
}

std::optional<CliArgs> parse_args(int argc, char* argv[]) {
    if (argc < 3) return std::nullopt;

    CliArgs args;
    args.name = argv[1];
    args.tag = argv[2];

    for (int i = 3; i < argc; i += 2) {
        std::string flag = argv[i];
        if (i + 1 >= argc) {
            std::cerr << "Missing value for " << flag << "\n";
            return std::nullopt;
        }
        std::string val = argv[i + 1];

        if (flag == "--region") args.region = val;
        else if (flag == "--matches") args.matches = std::stoi(val);
        else if (flag == "--window") args.window = std::stoi(val);
        else if (flag == "--gap") args.gap_minutes = std::stoi(val);
        else if (flag == "--format") {
            args.format = (val == "csv") ? valorant::OutputFormat::Csv
                                         : valorant::OutputFormat::Table;
        }
        else if (flag == "--api-key") args.api_key = val;
        else if (flag == "--report") args.reports.insert(val);
        else {
            std::cerr << "Unknown option: " << flag << "\n";
            return std::nullopt;
        }
    }

    if (args.api_key.empty()) {
        if (auto* env = std::getenv("VALORANT_API_KEY")) {
            args.api_key = env;
        }
    }

    if (args.reports.empty()) args.reports.insert("all");

    return args;
}

bool should_report(const std::unordered_set<std::string>& reports, const std::string& name) {
    return reports.contains("all") || reports.contains(name);
}

} // namespace

int main(int argc, char* argv[]) {
    auto args = parse_args(argc, argv);
    if (!args) {
        print_usage();
        return 1;
    }

    valorant::ClientConfig config{.api_key = args->api_key};
    valorant::RateLimiter limiter;
    valorant::Cache cache;

    // Fetch account
    std::cerr << "Looking up " << args->name << "#" << args->tag << "...\n";
    auto account = valorant::fetch_account(config, limiter, args->name, args->tag);
    if (!account) {
        std::cerr << "Error fetching account: " << account.error().message << "\n";
        return 1;
    }

    // Fetch matches
    auto matches = valorant::fetch_matches(
        config, limiter, cache, args->region, args->name, args->tag,
        args->matches, valorant::display_progress);
    if (!matches) {
        std::cerr << "Error fetching matches: " << matches.error().message << "\n";
        return 1;
    }

    if (matches->empty()) {
        std::cerr << "No competitive matches found.\n";
        return 0;
    }

    // Fetch MMR history
    auto mmr_history = valorant::fetch_mmr_history(
        config, limiter, cache, args->region, args->name, args->tag, account->puuid);
    if (!mmr_history) {
        std::cerr << "Warning: Could not fetch MMR history: "
                  << mmr_history.error().message << "\n";
        mmr_history = std::vector<valorant::MmrHistoryEntry>{};
    }

    // Build summaries and detect sessions
    auto summaries = valorant::build_summaries(*matches, *mmr_history, account->puuid);
    auto sessions = valorant::detect_sessions(
        summaries, std::chrono::minutes(args->gap_minutes));

    valorant::display_player_header(*account, static_cast<int>(summaries.size()));

    // Run requested reports
    if (should_report(args->reports, "hourly")) {
        valorant::display_hourly(
            valorant::performance_by_hour(summaries), args->format);
    }

    if (should_report(args->reports, "sessions")) {
        valorant::display_sessions(
            valorant::performance_by_session(sessions), args->format);
    }

    if (should_report(args->reports, "rr")) {
        valorant::display_rr_sessions(
            valorant::rr_by_session(sessions), args->format);
    }

    if (should_report(args->reports, "rolling-kda")) {
        valorant::display_rolling_kda(
            valorant::rolling_kda(summaries, args->window), args->format);
    }

    if (should_report(args->reports, "rolling-wr")) {
        valorant::display_rolling_wr(
            valorant::rolling_win_rate(summaries, args->window), args->format);
    }

    if (should_report(args->reports, "decay")) {
        valorant::display_decay(
            valorant::decay_curve(sessions), args->format);
    }

    return 0;
}
