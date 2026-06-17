#include "services.hpp"
#include "auth.hpp"
#include "crypto.hpp"
#include "broadcaster.hpp"
#include "utils.hpp"
#include <stdexcept>
#include <iostream>

namespace services {

nlohmann::json register_user(const RegisterRequest& req, repository::Repository& repo) {
    if (req.username.length() < 3 || req.username.length() > 50) {
        throw ServiceException(400, "Username must be between 3 and 50 characters");
    }
    if (req.password.length() < 6) {
        throw ServiceException(400, "Password must be at least 6 characters");
    }

    auto existing = repo.get_user_by_username(req.username);
    if (existing) {
        throw ServiceException(400, "Username already registered");
    }

    std::string hashed = auth::hash_password(req.password);
    repo.create_user(req.username, hashed);
    
    return {{"message", "User registered successfully"}};
}

nlohmann::json authenticate_user(const LoginRequest& req, repository::Repository& repo) {
    auto user = repo.get_user_by_username(req.username);

    // Timing attack mitigation: always execute a Bcrypt checkpw verification,
    // even if the username does not exist, to make the computation time uniform.
    std::string dummy_hash = "$2b$12$eImiTXuWVMtY.n89.B.6IuxA.X/g19g2588s77977.B8B8B8B8B8B";
    std::string stored_hash = user ? user->password_hash : dummy_hash;

    // Run Bcrypt verification (takes ~100ms)
    bool password_valid = auth::verify_password(req.password, stored_hash);

    if (!user || !password_valid) {
        throw ServiceException(401, "Incorrect username or password");
    }

    // Increment login version to invalidate old sessions
    repo.increment_login_version(user->id);
    
    // Fetch updated user to get new login version
    auto updated_user = repo.get_user_by_username(req.username);
    if (!updated_user) {
        throw ServiceException(500, "Database state integrity error");
    }

    std::string access_token = auth::create_token(updated_user->username, updated_user->login_version);
    return {
        {"access_token", access_token},
        {"token_type", "bearer"}
    };
}

std::vector<MessageResponse> process_send_message(const SendMessageRequest& req, const std::string& username, repository::Repository& repo) {
    if (req.content.empty() || req.content.length() > 2000) {
        throw ServiceException(400, "Content must be between 1 and 2000 characters");
    }
    if (req.recipients.empty()) {
        throw ServiceException(400, "At least one recipient is required");
    }

    std::string ciphertext = crypto::encrypt(req.content);
    std::vector<MessageResponse> results;
    results.reserve(req.recipients.size());

    for (const auto& recipient : req.recipients) {
        // Ensure recipient exists
        auto rec_user = repo.get_user_by_username(recipient);
        if (!rec_user) {
            throw ServiceException(400, "Recipient '" + recipient + "' not found");
        }

        auto msg = repo.create_message(username, recipient, ciphertext);
        results.push_back(MessageResponse{
            msg.id,
            msg.sender,
            msg.recipient,
            req.content, // decrypted content
            msg.created_at,
            msg.updated_at,
            msg.is_deleted
        });
    }

    // Broadcast message events
    // 1. Broadcast to each recipient individually
    for (const auto& msg : results) {
        nlohmann::json event = {
            {"id", msg.id},
            {"sender", msg.sender},
            {"recipient", msg.recipient},
            {"content", msg.content},
            {"created_at", utils::db_time_to_iso(msg.created_at)}
        };
        if (msg.sender != msg.recipient) {
            broadcaster::g_broadcaster.broadcast(msg.recipient, event);
        }
    }

    // 2. Broadcast a single combined event to the sender (one entry with joined recipients)
    if (!results.empty()) {
        std::string all_recipients;
        for (size_t i = 0; i < req.recipients.size(); ++i) {
            if (i > 0) all_recipients += ", ";
            all_recipients += req.recipients[i];
        }

        nlohmann::json sender_event = {
            {"id", results[0].id},
            {"sender", username},
            {"recipient", all_recipients},
            {"content", req.content},
            {"created_at", utils::db_time_to_iso(results[0].created_at)}
        };
        broadcaster::g_broadcaster.broadcast(username, sender_event);
    }

    return results;
}

std::vector<MessageResponse> fetch_messages(const std::string& username, repository::Repository& repo) {
    auto db_messages = repo.get_messages_for_user(username);
    std::vector<MessageResponse> results;
    results.reserve(db_messages.size());

    for (const auto& msg : db_messages) {
        std::string decrypted_content;
        try {
            decrypted_content = crypto::decrypt(msg.ciphertext);
        } catch (const std::exception& e) {
            decrypted_content = "[Decryption Failed]";
        }

        results.push_back(MessageResponse{
            msg.id,
            msg.sender,
            msg.recipient,
            decrypted_content,
            msg.created_at,
            msg.updated_at,
            msg.is_deleted
        });
    }

    return results;
}

MessageResponse edit_message(int message_id, const std::string& username, const UpdateMessageRequest& req, repository::Repository& repo) {
    if (req.content.empty() || req.content.length() > 2000) {
        throw ServiceException(400, "Content must be between 1 and 2000 characters");
    }

    auto msg = repo.get_message_by_id(message_id);
    if (!msg) {
        throw ServiceException(404, "Message not found");
    }

    if (msg->sender != username) {
        throw ServiceException(403, "You can only edit your own messages");
    }

    if (msg->is_deleted) {
        throw ServiceException(400, "Cannot edit a deleted message");
    }

    std::string ciphertext = crypto::encrypt(req.content);
    repo.update_message(message_id, ciphertext, false);

    auto updated_msg = repo.get_message_by_id(message_id);
    if (!updated_msg) {
        throw ServiceException(500, "Failed to retrieve edited message");
    }

    MessageResponse res{
        updated_msg->id,
        updated_msg->sender,
        updated_msg->recipient,
        req.content,
        updated_msg->created_at,
        updated_msg->updated_at,
        updated_msg->is_deleted
    };

    // Broadcast edit event
    nlohmann::json event = {
        {"type", "edit"},
        {"id", res.id},
        {"sender", res.sender},
        {"recipient", res.recipient},
        {"content", res.content},
        {"created_at", utils::db_time_to_iso(res.created_at)},
        {"updated_at", utils::db_time_to_iso(res.updated_at)}
    };

    broadcaster::g_broadcaster.broadcast(res.recipient, event);
    if (res.sender != res.recipient) {
        broadcaster::g_broadcaster.broadcast(res.sender, event);
    }

    return res;
}

MessageResponse delete_message(int message_id, const std::string& username, repository::Repository& repo) {
    auto msg = repo.get_message_by_id(message_id);
    if (!msg) {
        throw ServiceException(404, "Message not found");
    }

    if (msg->sender != username) {
        throw ServiceException(403, "You can only delete your own messages");
    }

    repo.update_message(message_id, msg->ciphertext, true);

    auto updated_msg = repo.get_message_by_id(message_id);
    if (!updated_msg) {
        throw ServiceException(500, "Failed to retrieve deleted message");
    }

    MessageResponse res{
        updated_msg->id,
        updated_msg->sender,
        updated_msg->recipient,
        "", // Content is hidden for deleted messages
        updated_msg->created_at,
        updated_msg->updated_at,
        updated_msg->is_deleted
    };

    // Broadcast delete event
    nlohmann::json event = {
        {"type", "delete"},
        {"id", res.id},
        {"sender", res.sender},
        {"recipient", res.recipient},
        {"is_deleted", true}
    };

    broadcaster::g_broadcaster.broadcast(res.recipient, event);
    if (res.sender != res.recipient) {
        broadcaster::g_broadcaster.broadcast(res.sender, event);
    }

    return res;
}

std::pair<std::string, int> validate_stream_token(const std::string& token, repository::Repository& repo) {
    if (token.empty()) {
        throw ServiceException(401, "Missing token");
    }

    auto payload_opt = auth::decode_token(token);
    if (!payload_opt) {
        throw ServiceException(401, "Invalid or expired token");
    }

    auto payload = *payload_opt;
    if (!payload.contains("sub") || !payload.contains("version")) {
        throw ServiceException(401, "Invalid token payload");
    }

    std::string username = payload["sub"];
    int version = payload["version"];

    auto user = repo.get_user_by_username(username);
    if (!user || user->login_version != version) {
        throw ServiceException(401, "Session invalidated (logged in elsewhere)");
    }

    return {username, version};
}

nlohmann::json to_json(const MessageResponse& msg) {
    return {
        {"id", msg.id},
        {"sender", msg.sender},
        {"recipient", msg.recipient},
        {"content", msg.content},
        {"created_at", utils::db_time_to_iso(msg.created_at)},
        {"updated_at", msg.updated_at.empty() ? nullptr : utils::db_time_to_iso(msg.updated_at)},
        {"is_deleted", msg.is_deleted}
    };
}

nlohmann::json to_json(const std::vector<MessageResponse>& msgs) {
    auto arr = nlohmann::json::array();
    for (const auto& msg : msgs) {
        arr.push_back(to_json(msg));
    }
    return arr;
}

} // namespace services
