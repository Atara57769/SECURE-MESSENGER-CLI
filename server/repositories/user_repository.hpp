#pragma once

#include "database.hpp"
#include "models.hpp"
#include <optional>
#include <string>

namespace repository {

class UserRepository {
private:
    SqliteDb& db_;

public:
    UserRepository(SqliteDb& db);

    // Create database tables if they do not exist
    void create_tables();

    // Query user by username
    std::optional<User> get_user_by_username(const std::string& username);

    // Create a new user record
    User create_user(const std::string& username, const std::string& password_hash);

    // Increment a user's login version
    void increment_login_version(int user_id);
};

} // namespace repository
