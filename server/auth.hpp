#pragma once

#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace auth {

// Initialize JWT secret key
void init_jwt();

// Hash a password using Bcrypt
std::string hash_password(const std::string& plain);

// Verify a password against a stored Bcrypt hash
bool verify_password(const std::string& plain, const std::string& hashed);

// Create a signed JWT token
std::string create_token(const std::string& username, int version);

// Decode and verify a JWT token. Returns the claims if valid, or nullopt if invalid/expired.
std::optional<nlohmann::json> decode_token(const std::string& token);

} // namespace auth
