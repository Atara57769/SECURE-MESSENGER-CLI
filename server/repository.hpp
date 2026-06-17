#pragma once

#include "database.hpp"
#include "models.hpp"
#include <optional>
#include <vector>

namespace repository {

class Repository {
private:
    SqliteDb& db_;

public:
    Repository(SqliteDb& db);

    // Create database tables if they do not exist
    void create_tables();

    // Query user by username
    std::optional<User> get_user_by_username(const std::string& username);

    // Create a new user record
    User create_user(const std::string& username, const std::string& password_hash);

    // Increment a user's login version
    void increment_login_version(int user_id);

    // Create a message record
    Message create_message(const std::string& sender, const std::string& recipient, const std::string& ciphertext);

    // Retrieve a message by ID
    std::optional<Message> get_message_by_id(int message_id);

    // Get all messages for a user (either sent or received) where is_deleted = 0
    std::vector<Message> get_messages_for_user(const std::string& username);

    // Update message ciphertext or is_deleted flag
    void update_message(int message_id, const std::string& ciphertext, bool is_deleted);
};

} // namespace repository
