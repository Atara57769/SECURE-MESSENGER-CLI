#pragma once

#include "repositories/user_repository.hpp"
#include "repositories/message_repository.hpp"
#include "service_exception.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace services {

// Send message request fields
struct SendMessageRequest {
    std::string content;
    std::vector<std::string> recipients;
};

// Update message request fields
struct UpdateMessageRequest {
    std::string content;
};

// Message response structure
struct MessageResponse {
    int id;
    std::string sender;
    std::string recipient;
    std::string content; // Decrypted
    std::string created_at; // ISO string
    std::string updated_at; // ISO string or null
    bool is_deleted;
};

// Send message process (encrypt, save, and broadcast to recipients and sender)
std::vector<MessageResponse> process_send_message(const SendMessageRequest& req, const std::string& username, repository::UserRepository& user_repo, repository::MessageRepository& message_repo);

// Fetch decrypted messages for a user
std::vector<MessageResponse> fetch_messages(const std::string& username, repository::MessageRepository& repo);

// Edit message content
MessageResponse edit_message(int message_id, const std::string& username, const UpdateMessageRequest& req, repository::MessageRepository& repo);

// Mark a message as deleted
MessageResponse delete_message(int message_id, const std::string& username, repository::MessageRepository& repo);

// Helper serialization to JSON
nlohmann::json to_json(const MessageResponse& msg);
nlohmann::json to_json(const std::vector<MessageResponse>& msgs);

} // namespace services
