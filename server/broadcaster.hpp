#pragma once

#include "thread_safe_queue.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace broadcaster {

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
