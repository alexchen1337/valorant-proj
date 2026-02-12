#include "valorant/display.hpp"
#include "valorant/env.hpp"
#include <iostream>
#include <string>

namespace {

std::optional<valorant::AppConfig> parse_args(int argc, char* argv[]) {
    valorant::AppConfig config;

    for (int i = 1; i < argc; i += 2) {
        std::string flag = argv[i];
        if (i + 1 >= argc) {
            std::cerr << "Missing value for " << flag << "\n";
            return std::nullopt;
        }
        std::string val = argv[i + 1];

        if (flag == "--region") config.region = val;
        else if (flag == "--matches") config.match_count = std::stoi(val);
        else if (flag == "--window") config.window = std::stoi(val);
        else if (flag == "--gap") config.gap_minutes = std::stoi(val);
        else if (flag == "--api-key") config.client.api_key = val;
        else {
            std::cerr << "Unknown option: " << flag << "\n";
            return std::nullopt;
        }
    }

    if (config.client.api_key.empty()) {
        if (auto key = valorant::get_env("VALORANT_API_KEY")) {
            config.client.api_key = *key;
        }
    }

    return config;
}

} // namespace

int main(int argc, char* argv[]) {
    valorant::load_env();

    auto config = parse_args(argc, argv);
    if (!config) {
        std::cerr << R"(Usage: valorant-fatigue [options]
  --region <na|eu|ap|kr>    Region (default: na)
  --matches <n>             Number of matches (default: 200)
  --window <n>              Rolling window size (default: 20)
  --gap <minutes>           Session gap threshold (default: 45)
  --api-key <key>           API key (or set VALORANT_API_KEY in .env)
)";
        return 1;
    }

    valorant::run_app(*config);
    return 0;
}
