#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace valorant {

std::unordered_map<std::string, std::string> load_env(
    const std::filesystem::path& path = ".env");

std::optional<std::string> get_env(const std::string& key);

} // namespace valorant
