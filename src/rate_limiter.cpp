#include "valorant/rate_limiter.hpp"
#include <thread>

namespace valorant {

RateLimiter::RateLimiter(int max_requests, std::chrono::seconds window)
    : max_requests_(max_requests), window_(window) {}

void RateLimiter::wait_for_slot() {
    std::lock_guard lock(mutex_);
    auto now = std::chrono::steady_clock::now();

    while (!timestamps_.empty() && (now - timestamps_.front()) > window_) {
        timestamps_.pop_front();
    }

    if (static_cast<int>(timestamps_.size()) >= max_requests_) {
        auto sleep_until = timestamps_.front() + window_;
        auto sleep_duration = sleep_until - now;
        if (sleep_duration.count() > 0) {
            mutex_.unlock();
            std::this_thread::sleep_for(sleep_duration);
            mutex_.lock();
        }
        now = std::chrono::steady_clock::now();
        while (!timestamps_.empty() && (now - timestamps_.front()) > window_) {
            timestamps_.pop_front();
        }
    }

    timestamps_.push_back(now);
}

} // namespace valorant
