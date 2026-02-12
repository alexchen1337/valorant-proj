#pragma once

#include <chrono>
#include <deque>
#include <mutex>

namespace valorant {

class RateLimiter {
public:
    RateLimiter(int max_requests = 30,
                std::chrono::seconds window = std::chrono::seconds(60));

    void wait_for_slot();

private:
    int max_requests_;
    std::chrono::seconds window_;
    std::deque<std::chrono::steady_clock::time_point> timestamps_;
    std::mutex mutex_;
};

} // namespace valorant
