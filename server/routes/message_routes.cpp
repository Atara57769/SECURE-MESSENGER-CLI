#include "message_routes.hpp"
#include "services/message_services.hpp"
#include "routes_common.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

namespace routes {

void setup_message_routes(httplib::Server& svr, repository::UserRepository& user_repo, repository::MessageRepository& message_repo) {
    // ---------------------------------------------------------------------------
    // Send Messages (Authenticated)
    // ---------------------------------------------------------------------------
    svr.Post("/messages", [&user_repo, &message_repo](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string username = require_auth(req, res, user_repo);
            if (username.empty()) return;

            nlohmann::json body = nlohmann::json::parse(req.body);
            if (!body.contains("content") || !body.contains("recipients")) {
                res.status = 400;
                res.set_content(nlohmann::json{{"detail", "Missing content or recipients"}}.dump(), "application/json");
                return;
            }

            std::vector<std::string> recipients;
            for (const auto& r : body["recipients"]) {
                recipients.push_back(r.get<std::string>());
            }

            services::SendMessageRequest service_req{
                body["content"].get<std::string>(),
                recipients
            };

            auto result = services::process_send_message(service_req, username, user_repo, message_repo);
            res.status = 201;
            res.set_content(services::to_json(result).dump(), "application/json");
        } catch (const services::ServiceException& e) {
            res.status = e.status_code();
            res.set_content(nlohmann::json{{"detail", e.what()}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json{{"detail", e.what()}}.dump(), "application/json");
        }
    });

    // ---------------------------------------------------------------------------
    // Fetch Messages (Authenticated)
    // ---------------------------------------------------------------------------
    svr.Get("/messages", [&user_repo, &message_repo](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string username = require_auth(req, res, user_repo);
            if (username.empty()) return;

            auto result = services::fetch_messages(username, message_repo);
            res.status = 200;
            res.set_content(services::to_json(result).dump(), "application/json");
        } catch (const services::ServiceException& e) {
            res.status = e.status_code();
            res.set_content(nlohmann::json{{"detail", e.what()}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json{{"detail", e.what()}}.dump(), "application/json");
        }
    });

    // ---------------------------------------------------------------------------
    // Edit Message (Authenticated, Regex parameter)
    // ---------------------------------------------------------------------------
    svr.Patch(R"(/messages/(\d+))", [&user_repo, &message_repo](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string username = require_auth(req, res, user_repo);
            if (username.empty()) return;

            int message_id = std::stoi(req.matches[1]);
            nlohmann::json body = nlohmann::json::parse(req.body);
            if (!body.contains("content")) {
                res.status = 400;
                res.set_content(nlohmann::json{{"detail", "Missing content field"}}.dump(), "application/json");
                return;
            }

            services::UpdateMessageRequest service_req{
                body["content"].get<std::string>()
            };

            auto result = services::edit_message(message_id, username, service_req, message_repo);
            res.status = 200;
            res.set_content(services::to_json(result).dump(), "application/json");
        } catch (const services::ServiceException& e) {
            res.status = e.status_code();
            res.set_content(nlohmann::json{{"detail", e.what()}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json{{"detail", e.what()}}.dump(), "application/json");
        }
    });

    // ---------------------------------------------------------------------------
    // Delete Message (Authenticated, Regex parameter)
    // ---------------------------------------------------------------------------
    svr.Delete(R"(/messages/(\d+))", [&user_repo, &message_repo](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string username = require_auth(req, res, user_repo);
            if (username.empty()) return;

            int message_id = std::stoi(req.matches[1]);
            auto result = services::delete_message(message_id, username, message_repo);
            res.status = 200;
            res.set_content(services::to_json(result).dump(), "application/json");
        } catch (const services::ServiceException& e) {
            res.status = e.status_code();
            res.set_content(nlohmann::json{{"detail", e.what()}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json{{"detail", e.what()}}.dump(), "application/json");
        }
    });
}

} // namespace routes
