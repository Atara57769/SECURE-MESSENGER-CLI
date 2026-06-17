#pragma once

#include <string>

namespace repository {

struct User {
    int id = 0;
    std::string username;
    std::string password_hash;
    std::string created_at;
    int login_version = 1;
};

struct Message {
    int id = 0;
    std::string sender;
    std::string recipient;
    std::string ciphertext;
    std::string created_at;
    std::string updated_at; // Will be empty string if null
    bool is_deleted = false;
};

} // namespace repository
