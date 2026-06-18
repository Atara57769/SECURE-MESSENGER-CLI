#include "message_services.hpp"
#include "crypto.hpp"
#include "broadcaster.hpp"
#include "utils.hpp"
#include <stdexcept>
#include <iostream>

namespace services {

std::vector<MessageResponse> process_send_message(const SendMessageRequest& req, const std::string& username, repository::UserRepository& user_repo, repository::MessageRepository& message_repo) {
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
        auto rec_user = user_repo.get_user_by_username(recipient);
        if (!rec_user) {
            throw ServiceException(400, "Recipient '" + recipient + "' not found");
        }

        auto msg = message_repo.create_message(username, recipient, ciphertext);
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

std::vector<MessageResponse> fetch_messages(const std::string& username, repository::MessageRepository& repo) {
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

MessageResponse edit_message(int message_id, const std::string& username, const UpdateMessageRequest& req, repository::MessageRepository& repo) {
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

MessageResponse delete_message(int message_id, const std::string& username, repository::MessageRepository& repo) {
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

nlohmann::json to_json(const MessageResponse& msg) {
    return {
        {"id", msg.id},
        {"sender", msg.sender},
        {"recipient", msg.recipient},
        {"content", msg.content},
        {"created_at", utils::db_time_to_iso(msg.created_at)},
        {"updated_at", msg.updated_at.empty() ? nlohmann::json() : nlohmann::json(utils::db_time_to_iso(msg.updated_at))},
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
