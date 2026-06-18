#include "message_repository.hpp"
#include "utils.hpp"
#include <stdexcept>

namespace repository {

MessageRepository::MessageRepository(SqliteDb& db) : db_(db) {}

void MessageRepository::create_tables() {
    db_.execute(
        "CREATE TABLE IF NOT EXISTS messages ("
        "id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, "
        "sender VARCHAR NOT NULL, "
        "recipient VARCHAR NOT NULL, "
        "ciphertext TEXT NOT NULL, "
        "created_at DATETIME NOT NULL, "
        "updated_at DATETIME, "
        "is_deleted BOOLEAN DEFAULT 0 NOT NULL"
        ");"
    );
    db_.execute(
        "CREATE INDEX IF NOT EXISTS ix_messages_id ON messages (id);"
    );
}

Message MessageRepository::create_message(const std::string& sender, const std::string& recipient, const std::string& ciphertext) {
    auto stmt = db_.prepare("INSERT INTO messages (sender, recipient, ciphertext, created_at, is_deleted) VALUES (?, ?, ?, ?, 0);");
    stmt.bind(1, sender);
    stmt.bind(2, recipient);
    stmt.bind(3, ciphertext);
    stmt.bind(4, utils::get_current_db_time());
    stmt.step();
    
    sqlite3_int64 row_id = sqlite3_last_insert_rowid(db_.get_raw_db());
    auto msg_opt = get_message_by_id(static_cast<int>(row_id));
    if (!msg_opt) {
        throw std::runtime_error("Failed to retrieve created message");
    }
    return *msg_opt;
}

std::optional<Message> MessageRepository::get_message_by_id(int message_id) {
    auto stmt = db_.prepare("SELECT id, sender, recipient, ciphertext, created_at, updated_at, is_deleted FROM messages WHERE id = ?;");
    stmt.bind(1, message_id);
    if (stmt.step()) {
        return Message{
            stmt.get_int(0),
            stmt.get_string(1),
            stmt.get_string(2),
            stmt.get_string(3),
            stmt.get_string(4),
            stmt.is_null(5) ? "" : stmt.get_string(5),
            stmt.get_int(6) != 0
        };
    }
    return std::nullopt;
}

std::vector<Message> MessageRepository::get_messages_for_user(const std::string& username) {
    auto stmt = db_.prepare("SELECT id, sender, recipient, ciphertext, created_at, updated_at, is_deleted "
                            "FROM messages WHERE (sender = ? OR recipient = ?) AND is_deleted = 0 ORDER BY created_at ASC;");
    stmt.bind(1, username);
    stmt.bind(2, username);
    
    std::vector<Message> results;
    while (stmt.step()) {
        results.push_back(Message{
            stmt.get_int(0),
            stmt.get_string(1),
            stmt.get_string(2),
            stmt.get_string(3),
            stmt.get_string(4),
            stmt.is_null(5) ? "" : stmt.get_string(5),
            stmt.get_int(6) != 0
        });
    }
    return results;
}

void MessageRepository::update_message(int message_id, const std::string& ciphertext, bool is_deleted) {
    auto stmt = db_.prepare("UPDATE messages SET ciphertext = ?, updated_at = ?, is_deleted = ? WHERE id = ?;");
    stmt.bind(1, ciphertext);
    stmt.bind(2, utils::get_current_db_time());
    stmt.bind(3, is_deleted ? 1 : 0);
    stmt.bind(4, message_id);
    stmt.step();
}

} // namespace repository
