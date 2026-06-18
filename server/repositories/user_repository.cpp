#include "user_repository.hpp"
#include "utils.hpp"
#include <stdexcept>

namespace repository {

UserRepository::UserRepository(SqliteDb& db) : db_(db) {}

void UserRepository::create_tables() {
    db_.execute(
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT, "
        "username VARCHAR NOT NULL, "
        "password_hash VARCHAR NOT NULL, "
        "created_at DATETIME NOT NULL, "
        "login_version INTEGER DEFAULT 1 NOT NULL"
        ");"
    );
    db_.execute(
        "CREATE UNIQUE INDEX IF NOT EXISTS ix_users_username ON users (username);"
    );
}

std::optional<User> UserRepository::get_user_by_username(const std::string& username) {
    auto stmt = db_.prepare("SELECT id, username, password_hash, created_at, login_version FROM users WHERE username = ?;");
    stmt.bind(1, username);
    if (stmt.step()) {
        return User{
            stmt.get_int(0),
            stmt.get_string(1),
            stmt.get_string(2),
            stmt.get_string(3),
            stmt.get_int(4)
        };
    }
    return std::nullopt;
}

User UserRepository::create_user(const std::string& username, const std::string& password_hash) {
    auto stmt = db_.prepare("INSERT INTO users (username, password_hash, created_at, login_version) VALUES (?, ?, ?, 1);");
    stmt.bind(1, username);
    stmt.bind(2, password_hash);
    stmt.bind(3, utils::get_current_db_time());
    stmt.step();
    
    auto user_opt = get_user_by_username(username);
    if (!user_opt) {
        throw std::runtime_error("Failed to retrieve created user: " + username);
    }
    return *user_opt;
}

void UserRepository::increment_login_version(int user_id) {
    auto stmt = db_.prepare("UPDATE users SET login_version = login_version + 1 WHERE id = ?;");
    stmt.bind(1, user_id);
    stmt.step();
}

} // namespace repository
