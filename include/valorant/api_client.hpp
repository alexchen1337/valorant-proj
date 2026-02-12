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

std::expected<std::vector<PlayerMatchSummary>, ApiError> fetch_stored_matches(
    const ClientConfig& config, RateLimiter& limiter,
    const std::string& region, const std::string& name, const std::string& tag,
    int count = 200, ProgressCallback on_progress = nullptr);

std::expected<std::vector<MmrHistoryEntry>, ApiError> fetch_mmr_history(
    const ClientConfig& config, RateLimiter& limiter, Cache& cache,
    const std::string& region, const std::string& name, const std::string& tag,
    const std::string& puuid);

PlayerMatchSummary parse_stored_match(const nlohmann::json& j);
MmrHistoryEntry parse_mmr_entry(const nlohmann::json& j);
PlayerIdentity parse_account(const nlohmann::json& j);

void apply_rr_to_summaries(
    std::vector<PlayerMatchSummary>& summaries,
    const std::vector<MmrHistoryEntry>& mmr_history);

} // namespace valorant
