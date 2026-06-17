#include "routes.hpp"
#include "services.hpp"
#include "broadcaster.hpp"
#include "auth.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

namespace routes {

// Helper to enforce authorization on standard REST routes.
// Returns username if successful, otherwise sets response status and returns empty string.
static std::string require_auth(const httplib::Request& req, httplib::Response& res, repository::Repository& repo) {
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

void setup_routes(httplib::Server& svr, repository::Repository& repo) {
    // ---------------------------------------------------------------------------
    // Test Reset Endpoint (to clear broadcaster subscriptions between tests)
    // ---------------------------------------------------------------------------
    svr.Post("/test/reset", [](const httplib::Request& req, httplib::Response& res) {
        broadcaster::g_broadcaster.clear();
        res.status = 200;
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    // ---------------------------------------------------------------------------
    // Register
    // ---------------------------------------------------------------------------
    svr.Post("/register", [&repo](const httplib::Request& req, httplib::Response& res) {
        try {
            nlohmann::json body = nlohmann::json::parse(req.body);
            if (!body.contains("username") || !body.contains("password")) {
                res.status = 400;
                res.set_content(nlohmann::json{{"detail", "Missing username or password"}}.dump(), "application/json");
                return;
            }

            services::RegisterRequest service_req{
                body["username"].get<std::string>(),
                body["password"].get<std::string>()
            };

            auto result = services::register_user(service_req, repo);
            res.status = 201;
            res.set_content(result.dump(), "application/json");
        } catch (const services::ServiceException& e) {
            res.status = e.status_code();
            res.set_content(nlohmann::json{{"detail", e.what()}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json{{"detail", e.what()}}.dump(), "application/json");
        }
    });

    // ---------------------------------------------------------------------------
    // Login
    // ---------------------------------------------------------------------------
    svr.Post("/login", [&repo](const httplib::Request& req, httplib::Response& res) {
        try {
            nlohmann::json body = nlohmann::json::parse(req.body);
            if (!body.contains("username") || !body.contains("password")) {
                res.status = 400;
                res.set_content(nlohmann::json{{"detail", "Missing username or password"}}.dump(), "application/json");
                return;
            }

            services::LoginRequest service_req{
                body["username"].get<std::string>(),
                body["password"].get<std::string>()
            };

            auto result = services::authenticate_user(service_req, repo);
            res.status = 200;
            res.set_content(result.dump(), "application/json");
        } catch (const services::ServiceException& e) {
            res.status = e.status_code();
            res.set_content(nlohmann::json{{"detail", e.what()}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json{{"detail", e.what()}}.dump(), "application/json");
        }
    });

    // ---------------------------------------------------------------------------
    // Send Messages (Authenticated)
    // ---------------------------------------------------------------------------
    svr.Post("/messages", [&repo](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string username = require_auth(req, res, repo);
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

            auto result = services::process_send_message(service_req, username, repo);
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
    svr.Get("/messages", [&repo](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string username = require_auth(req, res, repo);
            if (username.empty()) return;

            auto result = services::fetch_messages(username, repo);
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
    svr.Patch(R"(/messages/(\d+))", [&repo](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string username = require_auth(req, res, repo);
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

            auto result = services::edit_message(message_id, username, service_req, repo);
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
    svr.Delete(R"(/messages/(\d+))", [&repo](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string username = require_auth(req, res, repo);
            if (username.empty()) return;

            int message_id = std::stoi(req.matches[1]);
            auto result = services::delete_message(message_id, username, repo);
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
    // Presence Indicators (Authenticated)
    // ---------------------------------------------------------------------------
    svr.Get("/users/online", [&repo](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string username = require_auth(req, res, repo);
            if (username.empty()) return;

            auto online = broadcaster::g_broadcaster.get_active_users();
            res.status = 200;
            res.set_content(nlohmann::json{{"online_users", online}}.dump(), "application/json");
        } catch (const services::ServiceException& e) {
            res.status = e.status_code();
            res.set_content(nlohmann::json{{"detail", e.what()}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(nlohmann::json{{"detail", e.what()}}.dump(), "application/json");
        }
    });

    // ---------------------------------------------------------------------------
    // SSE Stream
    // ---------------------------------------------------------------------------
    svr.Get("/stream", [&repo](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string token;
            std::string auth_header = req.get_header_value("Authorization");
            if (auth_header.rfind("Bearer ", 0) == 0) {
                token = auth_header.substr(7);
            } else if (req.has_param("token")) {
                token = req.get_param_value("token");
            }

            auto val = services::validate_stream_token(token, repo);
            std::string username = val.first;

            auto q = broadcaster::g_broadcaster.subscribe(username);

            res.set_header("Content-Type", "text/event-stream");
            res.set_header("Cache-Control", "no-cache");
            res.set_header("X-Accel-Buffering", "no");

            res.set_content_provider(
                "text/event-stream",
                [username, q](size_t offset, httplib::DataSink& sink) {
                    nlohmann::json event;
                    bool popped = false;

                    // Poll the queue with short timeouts so we can check sink status quickly
                    for (int i = 0; i < 300; ++i) {
                        if (!sink.is_writable()) {
                            return false;
                        }
                        if (q->pop(event, std::chrono::milliseconds(50))) {
                            popped = true;
                            break;
                        }
                    }

                    if (popped) {
                        std::string payload = event.dump();
                        std::string sse_data = "event: message\ndata: " + payload + "\n\n";
                        if (!sink.write(sse_data.data(), sse_data.size())) {
                            return false;
                        }
                    } else {
                        std::string heartbeat = ": heartbeat\n\n";
                        if (!sink.write(heartbeat.data(), heartbeat.size())) {
                            return false;
                        }
                    }
                    return true;
                },
                [username, q](bool success) {
                    broadcaster::g_broadcaster.unsubscribe(username, q);
                }
            );

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
