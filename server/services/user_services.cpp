#include "user_services.hpp"
#include "auth.hpp"

namespace services {

nlohmann::json register_user(const RegisterRequest& req, repository::UserRepository& repo) {
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

nlohmann::json authenticate_user(const LoginRequest& req, repository::UserRepository& repo) {
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

std::pair<std::string, int> validate_stream_token(const std::string& token, repository::UserRepository& repo) {
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

} // namespace services
