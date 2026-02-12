#include "valorant/api_client.hpp"
#include <httplib.h>
#include <algorithm>
#include <thread>
#include <unordered_map>

namespace valorant {

namespace {

TimePoint parse_timestamp(int64_t epoch_secs) {
    return std::chrono::system_clock::from_time_t(static_cast<time_t>(epoch_secs));
}

int safe_int(const nlohmann::json& j, const std::string& key, int fallback = 0) {
    if (j.contains(key) && !j[key].is_null()) return j[key].get<int>();
    return fallback;
}

std::string safe_str(const nlohmann::json& j, const std::string& key,
                     const std::string& fallback = "") {
    if (j.contains(key) && !j[key].is_null()) return j[key].get<std::string>();
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

MatchData parse_match(const nlohmann::json& j) {
    MatchData match;
    auto& meta = j["metadata"];
    match.match_id = safe_str(meta, "match_id");
    match.map = safe_str(meta, "map");
    match.mode = safe_str(meta, "mode");
    match.game_start = parse_timestamp(meta.value("started_at", int64_t(0)));
    match.game_length_secs = safe_int(meta, "game_length_in_ms") / 1000;
    match.region = safe_str(meta, "region");
    match.platform = safe_str(meta, "platform");

    if (j.contains("players")) {
        for (auto& [team_key, team_players] : j["players"].items()) {
            for (auto& p : team_players) {
                MatchPlayer mp;
                mp.puuid = safe_str(p, "puuid");
                mp.name = safe_str(p, "name");
                mp.tag = safe_str(p, "tag");
                mp.team = team_key;
                mp.agent = safe_str(p, "agent");
                mp.tier = safe_int(p, "tier");

                if (p.contains("stats")) {
                    auto& s = p["stats"];
                    mp.stats.kills = safe_int(s, "kills");
                    mp.stats.deaths = safe_int(s, "deaths");
                    mp.stats.assists = safe_int(s, "assists");
                    mp.stats.score = safe_int(s, "score");
                    mp.stats.bodyshots = safe_int(s, "bodyshots");
                    mp.stats.headshots = safe_int(s, "headshots");
                    mp.stats.legshots = safe_int(s, "legshots");
                    mp.stats.damage_made = safe_int(s, "damage_made");
                    mp.stats.damage_received = safe_int(s, "damage_received");
                }

                match.players.push_back(std::move(mp));
            }
        }
    }

    if (j.contains("teams")) {
        for (auto& [team_id, team_data] : j["teams"].items()) {
            TeamResult tr;
            tr.team_id = team_id;
            tr.rounds_won = safe_int(team_data, "rounds_won");
            tr.rounds_lost = safe_int(team_data, "rounds_lost");
            tr.won = team_data.value("won", false);
            match.teams.push_back(std::move(tr));
        }
    }

    return match;
}

std::expected<std::vector<MatchData>, ApiError> fetch_matches(
    const ClientConfig& config, RateLimiter& limiter, Cache& cache,
    const std::string& region, const std::string& name, const std::string& tag,
    int count, ProgressCallback on_progress) {

    std::string platform = "pc";
    std::string path = "/valorant/v4/matches/" + region + "/" + platform +
                       "/" + name + "/" + tag + "?mode=competitive&size=" +
                       std::to_string(std::min(count, 50));

    auto result = fetch_endpoint(config, limiter, path);
    if (!result) return std::unexpected(result.error());

    std::vector<MatchData> matches;
    auto& data = *result;

    if (!data.is_array()) {
        return std::unexpected(ApiError{0, "Expected array of matches"});
    }

    int total = static_cast<int>(data.size());
    int current = 0;

    for (auto& match_json : data) {
        ++current;
        if (on_progress) on_progress(current, total);

        std::string mid = safe_str(match_json["metadata"], "match_id");

        auto cached = cache.get_match(mid);
        if (cached) {
            matches.push_back(parse_match(*cached));
            continue;
        }

        cache.store_match(mid, match_json);
        matches.push_back(parse_match(match_json));
    }

    return matches;
}

MmrHistoryEntry parse_mmr_entry(const nlohmann::json& j) {
    MmrHistoryEntry entry;
    entry.match_id = safe_str(j, "match_id");
    entry.rr_change = safe_int(j, "last_mmr_change");
    entry.rr_after = safe_int(j, "elo");
    entry.tier_after = safe_int(j, "tier");
    entry.timestamp = parse_timestamp(j.value("date_raw", int64_t(0)));
    return entry;
}

MmrSnapshot parse_mmr_snapshot(const nlohmann::json& j) {
    MmrSnapshot snap;
    snap.current_rr = safe_int(j, "current_data_elo", 0);
    if (j.contains("current_data")) {
        auto& cd = j["current_data"];
        snap.current_rr = safe_int(cd, "elo");
        snap.current_tier = safe_int(cd, "currenttier");
        snap.tier_name = safe_str(cd, "currenttierpatched");
    }
    return snap;
}

std::expected<MmrSnapshot, ApiError> fetch_mmr(
    const ClientConfig& config, RateLimiter& limiter,
    const std::string& region, const std::string& name, const std::string& tag) {

    auto result = fetch_endpoint(config, limiter,
                                 "/valorant/v2/mmr/" + region + "/" + name + "/" + tag);
    if (!result) return std::unexpected(result.error());
    return parse_mmr_snapshot(*result);
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

std::vector<PlayerMatchSummary> build_summaries(
    const std::vector<MatchData>& matches,
    const std::vector<MmrHistoryEntry>& mmr_history,
    const std::string& puuid) {

    std::unordered_map<std::string, int> rr_by_match;
    for (auto& entry : mmr_history) {
        rr_by_match[entry.match_id] = entry.rr_change;
    }

    std::vector<PlayerMatchSummary> summaries;
    for (auto& match : matches) {
        const MatchPlayer* me = nullptr;
        for (auto& p : match.players) {
            if (p.puuid == puuid) {
                me = &p;
                break;
            }
        }
        if (!me) continue;

        bool won = false;
        int rounds_played = 0;
        for (auto& team : match.teams) {
            if (team.team_id == me->team) {
                won = team.won;
                rounds_played = team.rounds_won + team.rounds_lost;
                break;
            }
        }

        PlayerMatchSummary s;
        s.match_id = match.match_id;
        s.map = match.map;
        s.mode = match.mode;
        s.agent = me->agent;
        s.game_start = match.game_start;
        s.game_length_secs = match.game_length_secs;
        s.kills = me->stats.kills;
        s.deaths = me->stats.deaths;
        s.assists = me->stats.assists;
        s.score = me->stats.score;
        s.damage_made = me->stats.damage_made;
        s.rounds_played = rounds_played;
        s.won = won;

        auto rr_it = rr_by_match.find(match.match_id);
        if (rr_it != rr_by_match.end()) {
            s.rr_change = rr_it->second;
            s.rr_available = true;
        }

        summaries.push_back(std::move(s));
    }

    std::ranges::sort(summaries, {}, &PlayerMatchSummary::game_start);
    return summaries;
}

} // namespace valorant
