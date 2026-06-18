#pragma once

#include "database.hpp"
#include "models.hpp"
#include <optional>
#include <vector>
#include <string>

namespace repository {

class MessageRepository {
private:
    SqliteDb& db_;

public:
    MessageRepository(SqliteDb& db);

    // Create database tables if they do not exist
    void create_tables();

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
