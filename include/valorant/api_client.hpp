#pragma once

#include "valorant/cache.hpp"
#include "valorant/rate_limiter.hpp"
#include "valorant/types.hpp"
#include <expected>
#include <functional>
#include <string>
#include <vector>

namespace valorant {

using ProgressCallback = std::function<void(int current, int total)>;

std::expected<nlohmann::json, ApiError> fetch_endpoint(
    const ClientConfig& config, RateLimiter& limiter, const std::string& path);

std::expected<PlayerIdentity, ApiError> fetch_account(
    const ClientConfig& config, RateLimiter& limiter,
    const std::string& name, const std::string& tag);

std::expected<std::vector<MatchData>, ApiError> fetch_matches(
    const ClientConfig& config, RateLimiter& limiter, Cache& cache,
    const std::string& region, const std::string& name, const std::string& tag,
    int count = 50, ProgressCallback on_progress = nullptr);

std::expected<MmrSnapshot, ApiError> fetch_mmr(
    const ClientConfig& config, RateLimiter& limiter,
    const std::string& region, const std::string& name, const std::string& tag);

std::expected<std::vector<MmrHistoryEntry>, ApiError> fetch_mmr_history(
    const ClientConfig& config, RateLimiter& limiter, Cache& cache,
    const std::string& region, const std::string& name, const std::string& tag,
    const std::string& puuid);

MatchData parse_match(const nlohmann::json& j);
MmrHistoryEntry parse_mmr_entry(const nlohmann::json& j);
PlayerIdentity parse_account(const nlohmann::json& j);
MmrSnapshot parse_mmr_snapshot(const nlohmann::json& j);

std::vector<PlayerMatchSummary> build_summaries(
    const std::vector<MatchData>& matches,
    const std::vector<MmrHistoryEntry>& mmr_history,
    const std::string& puuid);

} // namespace valorant
