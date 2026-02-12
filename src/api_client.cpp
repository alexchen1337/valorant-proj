#include "valorant/api_client.hpp"
#include <httplib.h>
#include <algorithm>
#include <ctime>
#include <thread>
#include <unordered_map>

namespace valorant {

namespace {

TimePoint parse_epoch(int64_t epoch_secs) {
    return std::chrono::system_clock::from_time_t(static_cast<time_t>(epoch_secs));
}

TimePoint parse_iso8601(const std::string& s) {
    std::tm tm{};
    sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d",
           &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
           &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    return std::chrono::system_clock::from_time_t(timegm(&tm));
}

int safe_int(const nlohmann::json& j, const std::string& key, int fallback = 0) {
    if (j.contains(key) && !j[key].is_null() && j[key].is_number())
        return j[key].get<int>();
    return fallback;
}

std::string safe_str(const nlohmann::json& j, const std::string& key,
                     const std::string& fallback = "") {
    if (j.contains(key) && !j[key].is_null() && j[key].is_string())
        return j[key].get<std::string>();
    return fallback;
}

std::string safe_obj_name(const nlohmann::json& j, const std::string& key,
                          const std::string& fallback = "") {
    if (j.contains(key) && j[key].is_object() && j[key].contains("name"))
        return j[key]["name"].get<std::string>();
    if (j.contains(key) && j[key].is_string())
        return j[key].get<std::string>();
    return fallback;
}

} // namespace

std::expected<nlohmann::json, ApiError> fetch_endpoint(
    const ClientConfig& config, RateLimiter& limiter, const std::string& path) {

    constexpr int max_retries = 3;
    for (int attempt = 0; attempt < max_retries; ++attempt) {
        limiter.wait_for_slot();

        httplib::SSLClient client(config.base_url);
        client.set_connection_timeout(10);
        client.set_read_timeout(30);

        httplib::Headers headers;
        if (!config.api_key.empty()) {
            headers.emplace("Authorization", config.api_key);
        }

        auto res = client.Get(path, headers);
        if (!res) {
            return std::unexpected(ApiError{0, "Connection failed: " + httplib::to_string(res.error())});
        }

        if (res->status == 429) {
            std::this_thread::sleep_for(std::chrono::seconds(2 * (attempt + 1)));
            continue;
        }

        if (res->status != 200) {
            std::string msg = "HTTP " + std::to_string(res->status);
            try {
                auto err_json = nlohmann::json::parse(res->body);
                if (err_json.contains("errors") && err_json["errors"].is_array() &&
                    !err_json["errors"].empty()) {
                    msg += ": " + err_json["errors"][0].value("message", "");
                }
            } catch (...) {}
            return std::unexpected(ApiError{res->status, msg});
        }

        try {
            auto body = nlohmann::json::parse(res->body);
            if (body.contains("data")) return body["data"];
            return body;
        } catch (const nlohmann::json::exception& e) {
            return std::unexpected(ApiError{0, std::string("JSON parse error: ") + e.what()});
        }
    }

    return std::unexpected(ApiError{429, "Rate limited after retries"});
}

PlayerIdentity parse_account(const nlohmann::json& j) {
    return {
        .name = safe_str(j, "name"),
        .tag = safe_str(j, "tag"),
        .puuid = safe_str(j, "puuid"),
        .region = safe_str(j, "region"),
        .card_small = j.contains("card") && j["card"].contains("small")
                          ? j["card"]["small"].get<std::string>()
                          : "",
    };
}

std::expected<PlayerIdentity, ApiError> fetch_account(
    const ClientConfig& config, RateLimiter& limiter,
    const std::string& name, const std::string& tag) {

    auto result = fetch_endpoint(config, limiter,
                                 "/valorant/v1/account/" + name + "/" + tag);
    if (!result) return std::unexpected(result.error());
    return parse_account(*result);
}

PlayerMatchSummary parse_stored_match(const nlohmann::json& j) {
    PlayerMatchSummary s;
    auto& meta = j["meta"];
    auto& stats = j["stats"];
    auto& teams = j["teams"];

    s.match_id = safe_str(meta, "id");
    s.map = safe_obj_name(meta, "map");
    s.mode = safe_str(meta, "mode");

    if (meta.contains("started_at") && meta["started_at"].is_string()) {
        s.game_start = parse_iso8601(meta["started_at"].get<std::string>());
    }

    s.kills = safe_int(stats, "kills");
    s.deaths = safe_int(stats, "deaths");
    s.assists = safe_int(stats, "assists");
    s.score = safe_int(stats, "score");
    s.agent = safe_obj_name(stats, "character");

    if (stats.contains("damage") && stats["damage"].is_object()) {
        s.damage_made = safe_int(stats["damage"], "made");
    }

    std::string my_team = safe_str(stats, "team");
    std::string my_team_lower = my_team;
    std::transform(my_team_lower.begin(), my_team_lower.end(),
                   my_team_lower.begin(), ::tolower);

    int my_rounds = 0;
    int enemy_rounds = 0;
    if (teams.is_object()) {
        for (auto& [team_key, rounds] : teams.items()) {
            if (team_key == my_team_lower) {
                my_rounds = rounds.get<int>();
            } else {
                enemy_rounds = rounds.get<int>();
            }
        }
    }

    s.rounds_played = my_rounds + enemy_rounds;
    s.won = my_rounds > enemy_rounds;

    // game_length not available in stored matches, estimate from rounds
    s.game_length_secs = s.rounds_played * 100; // ~100s per round

    return s;
}

std::expected<std::vector<PlayerMatchSummary>, ApiError> fetch_stored_matches(
    const ClientConfig& config, RateLimiter& limiter,
    const std::string& region, const std::string& name, const std::string& tag,
    int count, ProgressCallback on_progress) {

    std::vector<PlayerMatchSummary> all;
    constexpr int page_size = 50;
    int pages_needed = (count + page_size - 1) / page_size;

    for (int page = 1; page <= pages_needed; ++page) {
        int remaining = count - static_cast<int>(all.size());
        int fetch_size = std::min(remaining, page_size);

        std::string path = "/valorant/v1/stored-matches/" + region + "/" +
                           name + "/" + tag + "?mode=competitive&size=" +
                           std::to_string(fetch_size) + "&page=" + std::to_string(page);

        auto result = fetch_endpoint(config, limiter, path);
        if (!result) {
            if (all.empty()) return std::unexpected(result.error());
            break; // return what we have
        }

        auto& data = *result;
        if (!data.is_array() || data.empty()) break;

        for (auto& match_json : data) {
            all.push_back(parse_stored_match(match_json));
        }

        if (on_progress) {
            on_progress(static_cast<int>(all.size()), count);
        }

        if (static_cast<int>(data.size()) < fetch_size) break; // no more data
    }

    std::ranges::sort(all, {}, &PlayerMatchSummary::game_start);
    return all;
}

MmrHistoryEntry parse_mmr_entry(const nlohmann::json& j) {
    MmrHistoryEntry entry;
    entry.match_id = safe_str(j, "match_id");
    entry.rr_change = safe_int(j, "mmr_change_to_last_game");
    entry.rr_after = safe_int(j, "elo");
    entry.tier_after = safe_int(j, "currenttier");
    entry.timestamp = parse_epoch(j.value("date_raw", int64_t(0)));
    return entry;
}

std::expected<std::vector<MmrHistoryEntry>, ApiError> fetch_mmr_history(
    const ClientConfig& config, RateLimiter& limiter, Cache& cache,
    const std::string& region, const std::string& name, const std::string& tag,
    const std::string& puuid) {

    auto cached = cache.get_mmr_history(puuid);
    if (cached) {
        std::vector<MmrHistoryEntry> entries;
        for (auto& j : *cached) {
            entries.push_back(parse_mmr_entry(j));
        }
        return entries;
    }

    auto result = fetch_endpoint(config, limiter,
                                 "/valorant/v1/mmr-history/" + region + "/" + name + "/" + tag);
    if (!result) return std::unexpected(result.error());

    auto& data = *result;
    if (!data.is_array()) {
        return std::unexpected(ApiError{0, "Expected array of MMR history"});
    }

    cache.store_mmr_history(puuid, data);

    std::vector<MmrHistoryEntry> entries;
    for (auto& j : data) {
        entries.push_back(parse_mmr_entry(j));
    }
    return entries;
}

void apply_rr_to_summaries(
    std::vector<PlayerMatchSummary>& summaries,
    const std::vector<MmrHistoryEntry>& mmr_history) {

    std::unordered_map<std::string, int> rr_by_match;
    for (auto& entry : mmr_history) {
        rr_by_match[entry.match_id] = entry.rr_change;
    }

    for (auto& s : summaries) {
        auto it = rr_by_match.find(s.match_id);
        if (it != rr_by_match.end()) {
            s.rr_change = it->second;
            s.rr_available = true;
        }
    }
}

} // namespace valorant
