# valorant-fatigue

A C++ CLI tool that analyzes Valorant match history to detect how fatigue across gaming sessions degrades KDA, win rate, and RR gains. Features an interactive terminal UI for exploring reports.

## Demo

https://github.com/user-attachments/assets/cca064f3-fe72-471a-9061-86ab7785de05


## Features

- **Performance by Time of Day** — average KDA and win rate bucketed by hour
- **Session Performance** — KDA, damage/round, and RR tracked per game within each session
- **RR by Session Length** — total and average RR gain/loss per session
- **Rolling KDA** — sliding window KDA over match history with sparkline bars
- **Rolling Win Rate** — sliding window win rate with visual indicators
- **Fatigue Decay Curve** — linear regression of KDA by game position in session, with R² and fatigue interpretation

## Prerequisites

- C++23 compiler (Clang 16+, GCC 13+, or AppleClang 15+)
- CMake 3.20+
- OpenSSL
- [Henrik Valorant API](https://docs.henrikdev.xyz/) key

### macOS

```bash
brew install cmake openssl
```

### Ubuntu/Debian

```bash
sudo apt install cmake libssl-dev build-essential
```

## Build

```bash
cmake -B build
cmake --build build
```

## Setup

Create a `.env` file in the project root:

```
VALORANT_API_KEY=HDEV-your-api-key-here
```

Get a free API key at [https://docs.henrikdev.xyz/](https://docs.henrikdev.xyz/).

Alternatively, pass it directly via `--api-key` or export it:

```bash
export VALORANT_API_KEY=HDEV-your-api-key-here
```

## Usage

```
./build/valorant-fatigue <name> <tag> [options]
```

### Options

| Flag | Description | Default |
|---|---|---|
| `--region <na\|eu\|ap\|kr>` | Player region | `na` |
| `--matches <n>` | Number of competitive matches to fetch | `50` |
| `--window <n>` | Rolling window size for KDA/WR | `20` |
| `--gap <minutes>` | Time gap to define session boundary | `45` |
| `--api-key <key>` | API key (overrides .env) | — |

### Examples

```bash
# Basic usage (reads API key from .env)
./build/valorant-fatigue TenZ 1

# EU player, last 30 matches
./build/valorant-fatigue PlayerOne 1234 --region eu --matches 30

# Custom session gap and rolling window
./build/valorant-fatigue PlayerOne 1234 --gap 60 --window 10
```

### TUI Navigation

- **Left/Right arrows** — switch between report tabs
- **q** or **Esc** — quit

## Running Tests

```bash
cd build && ctest --output-on-failure
```

37 unit tests covering analytics, session detection, and .env parsing.

## Project Structure

```
valorant-proj/
├── CMakeLists.txt
├── .env                     # Your API key (gitignored)
├── include/valorant/
│   ├── types.hpp            # Data structs
│   ├── api_client.hpp       # API fetch functions
│   ├── rate_limiter.hpp     # Token-bucket rate limiter
│   ├── cache.hpp            # File-based JSON cache
│   ├── session_detector.hpp # Session boundary detection
│   ├── analytics.hpp        # 6 analytics computations
│   ├── display.hpp          # FTXUI terminal UI
│   └── env.hpp              # .env file parser
├── src/                     # Implementation files
├── tests/                   # GoogleTest unit tests
└── deps/                    # Header-only dependencies
    ├── httplib.h            # cpp-httplib (HTTPS)
    └── nlohmann/json.hpp    # JSON parsing
```

## How It Works

1. Fetches competitive match history via the Henrik API (with rate limiting and file-based caching)
2. Correlates matches with MMR history to get RR changes
3. Groups matches into sessions using a configurable time gap (default 45 min)
4. Computes 6 analytics reports across sessions
5. Displays results in an interactive TUI with color-coded tables and sparkline charts

Cached match data is stored in `data/` — subsequent runs skip API calls for already-fetched matches.
