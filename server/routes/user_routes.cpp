#include "user_routes.hpp"
#include "services/user_services.hpp"
#include "broadcaster.hpp"
#include "routes_common.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

namespace routes {

void setup_user_routes(httplib::Server& svr, repository::UserRepository& user_repo) {
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
    svr.Post("/register", [&user_repo](const httplib::Request& req, httplib::Response& res) {
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

            auto result = services::register_user(service_req, user_repo);
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
    svr.Post("/login", [&user_repo](const httplib::Request& req, httplib::Response& res) {
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

            auto result = services::authenticate_user(service_req, user_repo);
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
    // Presence Indicators (Authenticated)
    // ---------------------------------------------------------------------------
    svr.Get("/users/online", [&user_repo](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string username = require_auth(req, res, user_repo);
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
    svr.Get("/stream", [&user_repo](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string token;
            std::string auth_header = req.get_header_value("Authorization");
            if (auth_header.rfind("Bearer ", 0) == 0) {
                token = auth_header.substr(7);
            } else if (req.has_param("token")) {
                token = req.get_param_value("token");
            }

            auto val = services::validate_stream_token(token, user_repo);
            std::string username = val.first;

            auto q = broadcaster::g_broadcaster.subscribe(username);

            res.set_header("Cache-Control", "no-cache");
            res.set_header("X-Accel-Buffering", "no");

            res.set_chunked_content_provider(
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
