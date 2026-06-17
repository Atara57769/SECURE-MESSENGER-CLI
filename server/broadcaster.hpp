#pragma once

#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <nlohmann/json.hpp>

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

using EventQueue = ThreadSafeQueue<nlohmann::json>;
using EventQueuePtr = std::shared_ptr<EventQueue>;

class Broadcaster {
private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::vector<EventQueuePtr>> subscribers_;

public:
    Broadcaster() = default;

    // Get active online usernames
    std::vector<std::string> get_active_users();

    // Broadcast an event to a specific user's streams
    void broadcast(const std::string& username, const nlohmann::json& event);

    // Broadcast an event to everyone currently connected
    void broadcast_all(const nlohmann::json& event);

    // Subscribe a new event stream queue for a user
    EventQueuePtr subscribe(const std::string& username);

    // Unsubscribe a queue
    void unsubscribe(const std::string& username, EventQueuePtr q);

    // Clear all subscribers (clean shutdown or test reset)
    void clear();
};

// Global broadcaster instance
extern Broadcaster g_broadcaster;

} // namespace broadcaster
