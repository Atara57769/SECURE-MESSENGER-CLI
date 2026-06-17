#include "broadcaster.hpp"
#include <algorithm>

namespace broadcaster {

Broadcaster g_broadcaster;

std::vector<std::string> Broadcaster::get_active_users() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> active;
    active.reserve(subscribers_.size());
    for (const auto& pair : subscribers_) {
        active.push_back(pair.first);
    }
    return active;
}

void Broadcaster::broadcast(const std::string& username, const nlohmann::json& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = subscribers_.find(username);
    if (it == subscribers_.end()) return;

    auto& vec = it->second;
    for (auto it_q = vec.begin(); it_q != vec.end(); ) {
        if (!(*it_q)->push(event)) {
            // Queue full or closed, drop this subscription
            it_q = vec.erase(it_q);
        } else {
            ++it_q;
        }
    }

    if (vec.empty()) {
        subscribers_.erase(it);
    }
}

void Broadcaster::broadcast_all(const nlohmann::json& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = subscribers_.begin(); it != subscribers_.end(); ) {
        auto& vec = it->second;
        for (auto it_q = vec.begin(); it_q != vec.end(); ) {
            if (!(*it_q)->push(event)) {
                // Queue full or closed, drop this subscription
                it_q = vec.erase(it_q);
            } else {
                ++it_q;
            }
        }
        if (vec.empty()) {
            it = subscribers_.erase(it);
        } else {
            ++it;
        }
    }
}

EventQueuePtr Broadcaster::subscribe(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto q = std::make_shared<EventQueue>();

    bool is_first = (subscribers_.find(username) == subscribers_.end());
    subscribers_[username].push_back(q);

    if (is_first) {
        // Broadcast user presence online to everyone
        nlohmann::json event = {
            {"type", "presence"},
            {"username", username},
            {"status", "online"}
        };
        
        for (auto it = subscribers_.begin(); it != subscribers_.end(); ) {
            auto& vec = it->second;
            for (auto it_q = vec.begin(); it_q != vec.end(); ) {
                if (!(*it_q)->push(event)) {
                    it_q = vec.erase(it_q);
                } else {
                    ++it_q;
                }
            }
            if (vec.empty()) {
                it = subscribers_.erase(it);
            } else {
                ++it;
            }
        }
    }

    return q;
}

void Broadcaster::unsubscribe(const std::string& username, EventQueuePtr q) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Close the queue so that any active pop/wait wakes up and terminates
    if (q) {
        q->close();
    }

    auto it = subscribers_.find(username);
    if (it != subscribers_.end()) {
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), q), vec.end());
        
        if (vec.empty()) {
            subscribers_.erase(it);

            // Broadcast user presence offline to everyone
            nlohmann::json event = {
                {"type", "presence"},
                {"username", username},
                {"status", "offline"}
            };

            for (auto it_sub = subscribers_.begin(); it_sub != subscribers_.end(); ) {
                auto& sub_vec = it_sub->second;
                for (auto it_q = sub_vec.begin(); it_q != sub_vec.end(); ) {
                    if (!(*it_q)->push(event)) {
                        it_q = sub_vec.erase(it_q);
                    } else {
                        ++it_q;
                    }
                }
                if (sub_vec.empty()) {
                    it_sub = subscribers_.erase(it_sub);
                } else {
                    ++it_sub;
                }
            }
        }
    }
}

void Broadcaster::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pair : subscribers_) {
        for (auto& q : pair.second) {
            if (q) q->close();
        }
    }
    subscribers_.clear();
}

} // namespace broadcaster
