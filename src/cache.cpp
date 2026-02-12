#include "valorant/cache.hpp"
#include <fstream>

namespace valorant {

Cache::Cache(std::filesystem::path base_dir) : base_dir_(std::move(base_dir)) {
    std::filesystem::create_directories(base_dir_ / "matches");
    std::filesystem::create_directories(base_dir_ / "mmr_history");
}

std::optional<nlohmann::json> Cache::get_match(const std::string& match_id) const {
    return read_json(base_dir_ / "matches" / (match_id + ".json"));
}

void Cache::store_match(const std::string& match_id, const nlohmann::json& data) {
    write_json(base_dir_ / "matches" / (match_id + ".json"), data);
}

std::optional<nlohmann::json> Cache::get_mmr_history(const std::string& puuid) const {
    return read_json(base_dir_ / "mmr_history" / (puuid + ".json"), mmr_ttl_);
}

void Cache::store_mmr_history(const std::string& puuid, const nlohmann::json& data) {
    write_json(base_dir_ / "mmr_history" / (puuid + ".json"), data);
}

std::optional<nlohmann::json> Cache::read_json(
    const std::filesystem::path& path,
    std::optional<std::chrono::minutes> ttl) const {

    if (!std::filesystem::exists(path)) return std::nullopt;

    if (ttl) {
        auto file_time = std::filesystem::last_write_time(path);
        auto file_age = std::filesystem::file_time_type::clock::now() - file_time;
        auto age = std::chrono::duration_cast<std::chrono::minutes>(file_age);
        if (age > *ttl) return std::nullopt;
    }

    std::ifstream file(path);
    if (!file.is_open()) return std::nullopt;

    try {
        return nlohmann::json::parse(file);
    } catch (...) {
        return std::nullopt;
    }
}

void Cache::write_json(const std::filesystem::path& path, const nlohmann::json& data) const {
    std::ofstream file(path);
    if (file.is_open()) {
        file << data.dump(2);
    }
}

} // namespace valorant
