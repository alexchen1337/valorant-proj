#include "valorant/env.hpp"
#include <cstdlib>
#include <fstream>
#include <string>

namespace valorant {

namespace {

std::string trim(std::string_view sv) {
    auto start = sv.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return "";
    auto end = sv.find_last_not_of(" \t\r\n");
    return std::string(sv.substr(start, end - start + 1));
}

std::string strip_quotes(const std::string& s) {
    if (s.size() >= 2 &&
        ((s.front() == '"' && s.back() == '"') ||
         (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

} // namespace

std::unordered_map<std::string, std::string> load_env(
    const std::filesystem::path& path) {

    std::unordered_map<std::string, std::string> vars;
    std::ifstream file(path);
    if (!file.is_open()) return vars;

    std::string line;
    while (std::getline(file, line)) {
        auto trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        auto eq = trimmed.find('=');
        if (eq == std::string::npos) continue;

        auto key = trim(trimmed.substr(0, eq));
        auto val = strip_quotes(trim(trimmed.substr(eq + 1)));

        if (!key.empty()) {
            vars[key] = val;
            ::setenv(key.c_str(), val.c_str(), 0); // don't overwrite existing
        }
    }

    return vars;
}

std::optional<std::string> get_env(const std::string& key) {
    if (auto* val = std::getenv(key.c_str())) {
        return std::string(val);
    }
    return std::nullopt;
}

} // namespace valorant
