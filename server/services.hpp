#pragma once

#include "repository.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>

namespace services {

class ServiceException : public std::runtime_error {
private:
    int status_code_;
public:
    ServiceException(int status, const std::string& msg) : std::runtime_error(msg), status_code_(status) {}
    int status_code() const { return status_code_; }
};

// Register request fields
struct RegisterRequest {
    std::string username;
    std::string password;
};

// Login request fields
struct LoginRequest {
    std::string username;
    std::string password;
};

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

// Register a new user, throws runtime_error on conflict/invalid data
nlohmann::json register_user(const RegisterRequest& req, repository::Repository& repo);

// Authenticate a user, normalizes execution times via a dummy bcrypt validation
nlohmann::json authenticate_user(const LoginRequest& req, repository::Repository& repo);

// Send message process (encrypt, save, and broadcast to recipients and sender)
std::vector<MessageResponse> process_send_message(const SendMessageRequest& req, const std::string& username, repository::Repository& repo);

// Fetch decrypted messages for a user
std::vector<MessageResponse> fetch_messages(const std::string& username, repository::Repository& repo);

// Edit message content
MessageResponse edit_message(int message_id, const std::string& username, const UpdateMessageRequest& req, repository::Repository& repo);

// Mark a message as deleted
MessageResponse delete_message(int message_id, const std::string& username, repository::Repository& repo);

// Validate JWT token, check database for session validation (login_version), returns (username, login_version)
std::pair<std::string, int> validate_stream_token(const std::string& token, repository::Repository& repo);

// Helper serialization to JSON
nlohmann::json to_json(const MessageResponse& msg);
nlohmann::json to_json(const std::vector<MessageResponse>& msgs);

} // namespace services
