#pragma once

#include "repositories/user_repository.hpp"
#include "service_exception.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <utility>

namespace services {

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

// Register a new user, throws runtime_error on conflict/invalid data
nlohmann::json register_user(const RegisterRequest& req, repository::UserRepository& repo);

// Authenticate a user, normalizes execution times via a dummy bcrypt validation
nlohmann::json authenticate_user(const LoginRequest& req, repository::UserRepository& repo);

// Validate JWT token, check database for session validation (login_version), returns (username, login_version)
std::pair<std::string, int> validate_stream_token(const std::string& token, repository::UserRepository& repo);

} // namespace services
