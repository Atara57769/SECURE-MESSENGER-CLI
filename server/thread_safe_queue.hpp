#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace broadcaster {

template <typename T>
class ThreadSafeQueue {
private:
    std::queue<T> q_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool closed_ = false;
    size_t max_size_ = 1000;

public:
    ThreadSafeQueue(size_t max_size = 1000) : max_size_(max_size) {}

    bool push(const T& val) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_) return false;
        if (q_.size() >= max_size_) {
            return false;
        }
        q_.push(val);
        cv_.notify_one();
        return true;
    }

    bool pop(T& val, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (cv_.wait_for(lock, timeout, [this] { return !q_.empty() || closed_; })) {
            if (closed_ || q_.empty()) {
                return false;
            }
            val = q_.front();
            q_.pop();
            return true;
        }
        return false;
    }

    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        cv_.notify_all();
    }
};

} // namespace broadcaster
