#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

namespace valorant {

class Cache {
public:
    explicit Cache(std::filesystem::path base_dir = "data");

    std::optional<nlohmann::json> get_match(const std::string& match_id) const;
    void store_match(const std::string& match_id, const nlohmann::json& data);

    std::optional<nlohmann::json> get_mmr_history(const std::string& puuid) const;
    void store_mmr_history(const std::string& puuid, const nlohmann::json& data);

private:
    std::filesystem::path base_dir_;
    static constexpr auto mmr_ttl_ = std::chrono::minutes(30);

    std::optional<nlohmann::json> read_json(const std::filesystem::path& path,
                                            std::optional<std::chrono::minutes> ttl = std::nullopt) const;
    void write_json(const std::filesystem::path& path, const nlohmann::json& data) const;
};

} // namespace valorant
