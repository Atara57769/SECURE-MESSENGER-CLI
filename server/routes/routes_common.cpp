#include "routes_common.hpp"
#include "auth.hpp"
#include <nlohmann/json.hpp>

namespace routes {

std::string require_auth(const httplib::Request& req, httplib::Response& res, repository::UserRepository& repo) {
    std::string auth_header = req.get_header_value("Authorization");
    if (auth_header.rfind("Bearer ", 0) != 0) {
        res.status = 401;
        res.set_content(nlohmann::json{{"detail", "Could not validate credentials"}}.dump(), "application/json");
        return "";
    }

    std::string token = auth_header.substr(7);
    auto payload_opt = auth::decode_token(token);
    if (!payload_opt) {
        res.status = 401;
        res.set_content(nlohmann::json{{"detail", "Could not validate credentials"}}.dump(), "application/json");
        return "";
    }

    auto payload = *payload_opt;
    if (!payload.contains("sub") || !payload.contains("version")) {
        res.status = 401;
        res.set_content(nlohmann::json{{"detail", "Invalid token payload"}}.dump(), "application/json");
        return "";
    }

    std::string username = payload["sub"];
    int version = payload["version"];

    auto user = repo.get_user_by_username(username);
    if (!user || user->login_version != version) {
        res.status = 401;
        res.set_content(nlohmann::json{{"detail", "Session invalidated (logged in elsewhere)"}}.dump(), "application/json");
        return "";
    }

    return username;
}

} // namespace routes
