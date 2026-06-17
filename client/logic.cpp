#include "logic.hpp"
#include "config.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <cstdlib>

namespace logic {

static std::string g_current_recipient;
static std::mutex g_io_mutex;

std::string c(const std::string& code, const std::string& text) {
    return code + text + config::RESET;
}

void banner() {
    std::cout << c(config::CYAN, config::BANNER_TEXT) << std::endl;
}

void info(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_io_mutex);
    std::cout << c(config::DIM, "  ℹ  " + msg) << std::endl;
}

void ok(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_io_mutex);
    std::cout << c(config::GREEN, "  ✔  " + msg) << std::endl;
}

void err(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_io_mutex);
    std::cout << c(config::RED, "  ✖  " + msg) << std::endl;
}

void warn(const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_io_mutex);
    std::cout << c(config::YELLOW, "  ⚠  " + msg) << std::endl;
}

static void trim_cr(std::string& s) {
    if (!s.empty() && s.back() == '\r') {
        s.pop_back();
    }
}

std::string prompt(const std::string& label) {
    std::cout << c(config::BOLD, "  " + label + ": ");
    std::string line;
    std::getline(std::cin, line);
    trim_cr(line);
    return line;
}

void show_help(const std::string& current_recipient) {
    std::lock_guard<std::mutex> lock(g_io_mutex);
    std::cout << std::endl;
    std::cout << c(config::CYAN, "  ┌─ Commands ────────────────────────────────────────────┐") << std::endl;
    std::cout << c(config::CYAN, "  │") << "  Just type a message → sent to " << c(config::BOLD, current_recipient) << std::endl;
    std::cout << c(config::CYAN, "  │") << "  " << c(config::BOLD, "/to <name1, name2, ...>") << "   — switch conversation partner(s)" << std::endl;
    std::cout << c(config::CYAN, "  │") << "  " << c(config::BOLD, "/list") << "        — show full message history" << std::endl;
    std::cout << c(config::CYAN, "  │") << "  " << c(config::BOLD, "/edit <id> <text>") << " — edit a message you sent" << std::endl;
    std::cout << c(config::CYAN, "  │") << "  " << c(config::BOLD, "/delete <id>") << "      — delete a message you sent" << std::endl;
    std::cout << c(config::CYAN, "  │") << "  " << c(config::BOLD, "/help") << "        — show this help" << std::endl;
    std::cout << c(config::CYAN, "  │") << "  " << c(config::BOLD, "/quit") << "        — exit" << std::endl;
    std::cout << c(config::CYAN, "  └───────────────────────────────────────────────────────┘") << std::endl;
    std::cout << std::endl;
}

static std::string ts(const std::string& iso) {
    size_t pos = iso.find('T');
    if (pos == std::string::npos) {
        pos = iso.find(' ');
    }
    if (pos != std::string::npos && pos + 6 <= iso.length()) {
        return iso.substr(pos + 1, 5);
    }
    return "??:??";
}

static std::vector<std::string> split_commas(const std::string& str) {
    std::vector<std::string> parts;
    std::string current;
    std::stringstream ss(str);
    while (std::getline(ss, current, ',')) {
        size_t first = current.find_first_not_of(" \t");
        if (first != std::string::npos) {
            size_t last = current.find_last_not_of(" \t");
            parts.push_back(current.substr(first, last - first + 1));
        }
    }
    return parts;
}

static void print_message_raw(const std::string& sender, const std::string& recipient, const std::string& content,
                           const std::string& created_at, const std::string& me, int msg_id,
                           bool is_edited, bool is_deleted) {
    std::string time_str = ts(created_at);
    bool mine = (sender == me);
    std::string colour = mine ? config::YELLOW : config::MAGENTA;
    std::string arrow = c(config::DIM, "→");

    std::string status_suffix;
    if (is_deleted) {
        status_suffix = c(config::RED, " [DELETED]");
    } else if (is_edited) {
        status_suffix = c(config::CYAN, " [EDITED]");
    }

    std::string id_str = (msg_id > 0) ? (" #" + std::to_string(msg_id)) : "";
    std::string header = "  " + c(colour, sender) + " " + arrow + " " + c(config::DIM, recipient) + "  " +
                         c(config::DIM, time_str) + c(config::DIM, id_str) + status_suffix;

    std::string body;
    if (is_deleted) {
        body = "    " + c(config::DIM, "(message deleted)");
    } else {
        body = "    " + c(config::WHITE, content);
    }

    std::cout << "\r" << header << "\n" << body << std::endl;
}

std::string login(const std::string& base_url, const std::string& username, const std::string& password) {
    httplib::Client cli(base_url);
    cli.set_connection_timeout(10);
    cli.set_read_timeout(10);

    nlohmann::json req_json = {{"username", username}, {"password", password}};
    auto res = cli.Post("/login", req_json.dump(), "application/json");

    if (!res) {
        err("Cannot reach server — is the server running?");
        std::exit(1);
    }

    if (res->status == 200) {
        try {
            auto res_json = nlohmann::json::parse(res->body);
            return res_json["access_token"].get<std::string>();
        } catch (...) {}
    }
    return "";
}

std::string register_user(const std::string& base_url, const std::string& username, const std::string& password) {
    httplib::Client cli(base_url);
    nlohmann::json req_json = {{"username", username}, {"password", password}};
    auto res = cli.Post("/register", req_json.dump(), "application/json");

    if (!res) {
        err("Cannot reach server — is the server running?");
        std::exit(1);
    }

    if (res->status == 201) {
        return ""; // Success
    }

    try {
        auto res_json = nlohmann::json::parse(res->body);
        if (res_json.contains("detail")) {
            return res_json["detail"].get<std::string>();
        }
    } catch (...) {}

    return "Server returned status code " + std::to_string(res->status);
}

void fetch_history(const std::string& base_url, const std::string& token, const std::string& me, const std::string& filter_recipient) {
    httplib::Client cli(base_url);
    httplib::Headers headers = {
        {"Authorization", "Bearer " + token}
    };

    auto res = cli.Get("/messages", headers);
    if (!res || res->status != 200) {
        err("Could not fetch message history.");
        return;
    }

    nlohmann::json messages;
    try {
        messages = nlohmann::json::parse(res->body);
    } catch (...) {
        err("Invalid JSON history payload.");
        return;
    }

    // Parse targets
    auto target_list = split_commas(filter_recipient);
    std::vector<nlohmann::json> filtered;
    for (const auto& msg : messages) {
        std::string sender = msg.value("sender", "");
        std::string recipient = msg.value("recipient", "");

        bool matches = false;
        for (const auto& target : target_list) {
            if ((sender == me && recipient == target) || (recipient == me && sender == target)) {
                matches = true;
                break;
            }
        }
        if (matches) {
            filtered.push_back(msg);
        }
    }

    std::lock_guard<std::mutex> lock(g_io_mutex);
    if (filtered.empty()) {
        std::cout << c(config::DIM, "  ℹ  No messages yet.") << std::endl;
        return;
    }

    std::string label = filter_recipient.empty() ? "all" : ("with " + filter_recipient);
    std::cout << c(config::DIM, "  ─── History (" + label + ", " + std::to_string(filtered.size()) + " messages) ─────────────") << std::endl;

    for (const auto& msg : filtered) {
        print_message_raw(
            msg.value("sender", ""),
            msg.value("recipient", ""),
            msg.value("content", ""),
            msg.value("created_at", ""),
            me,
            msg.value("id", 0),
            !msg["updated_at"].is_null(),
            msg.value("is_deleted", false)
        );
    }

    std::cout << c(config::DIM, "  ─── End of history ──────────────────────────────────────") << std::endl;
}

void listen_stream(const std::string& base_url, const std::string& token, const std::string& me, std::atomic<bool>& stop) {
    info("Connecting to live stream…");

    while (!stop.load()) {
        httplib::Client cli(base_url);
        // Use default read timeout (300s) to allow blocking socket reads on SSE

        std::string path = "/stream?token=" + token;
        std::string buffer;

        auto res = cli.Get(path, [&](const char *data, size_t data_len) {
            if (stop.load()) return false; // Aborts stream

            buffer.append(data, data_len);
            size_t pos;
            while ((pos = buffer.find("\n\n")) != std::string::npos) {
                std::string block = buffer.substr(0, pos);
                buffer.erase(0, pos + 2);

                std::stringstream ss(block);
                std::string line;
                while (std::getline(ss, line)) {
                    if (line.rfind("data:", 0) == 0) {
                        std::string payload = line.substr(5);
                        // trim
                        size_t first = payload.find_first_not_of(" \t");
                        if (first != std::string::npos) {
                            payload = payload.substr(first);
                        }

                        try {
                            auto event = nlohmann::json::parse(payload);
                            std::string event_type = event.value("type", "message");

                            std::lock_guard<std::mutex> io_lock(g_io_mutex);

                            if (event_type == "presence") {
                                std::string username = event.value("username", "");
                                std::string status = event.value("status", "");
                                std::cout << "\r" << c(config::DIM, "  ℹ  User " + c(config::BOLD, username) + " is now " + c(config::CYAN, status)) << std::endl;
                            } else if (event_type == "edit") {
                                int msg_id = event.value("id", 0);
                                std::string sender = event.value("sender", "");
                                std::string recipient = event.value("recipient", "");
                                std::string content = event.value("content", "");
                                std::string created_at = event.value("created_at", "");
                                std::cout << "\r" << c(config::DIM, "  ℹ  Message #" + std::to_string(msg_id) + " was edited by " + sender) << std::endl;
                                print_message_raw(sender, recipient, content, created_at, me, msg_id, true, false);
                            } else if (event_type == "delete") {
                                int msg_id = event.value("id", 0);
                                std::string sender = event.value("sender", "");
                                std::string recipient = event.value("recipient", "");
                                std::string created_at = event.value("created_at", "");
                                std::cout << "\r" << c(config::DIM, "  ℹ  Message #" + std::to_string(msg_id) + " was deleted by " + sender) << std::endl;
                                print_message_raw(sender, recipient, "", created_at, me, msg_id, false, true);
                            } else {
                                // Normal message
                                int msg_id = event.value("id", 0);
                                std::string sender = event.value("sender", "");
                                std::string recipient = event.value("recipient", "");
                                std::string content = event.value("content", "");
                                std::string created_at = event.value("created_at", "");
                                print_message_raw(sender, recipient, content, created_at, me, msg_id, false, false);
                            }

                            // Re-print prompt
                            std::cout << c(config::DIM, "  [" + me + "→" + g_current_recipient + "] ") << c(config::BOLD, "") << std::flush;

                        } catch (...) {}
                    }
                }
            }
            return true;
        });

        if (!res) {
            warn("Stream connection lost (Error: " + std::to_string(static_cast<int>(res.error())) + ") — reconnecting in 3 s…");
        } else {
            warn("Stream connection lost (Status: " + std::to_string(res->status) + ") — reconnecting in 3 s…");
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

void input_loop(const std::string& base_url, const std::string& token, const std::string& me, State& state, std::atomic<bool>& stop) {
    httplib::Headers headers = {
        {"Authorization", "Bearer " + token}
    };

    g_current_recipient = state.recipient;
    show_help(state.recipient);
    info("Currently chatting with: " + c(config::BOLD, state.recipient));
    std::cout << std::endl;

    while (!stop.load()) {
        {
            std::lock_guard<std::mutex> lock(g_io_mutex);
            std::cout << c(config::DIM, "  [" + me + "→" + state.recipient + "] ") << c(config::BOLD, "");
            std::cout.flush();
        }

        std::string line;
        if (!std::getline(std::cin, line)) {
            stop.store(true);
            break;
        }
        trim_cr(line);

        if (line.empty()) continue;

        if (line[0] == '/') {
            std::stringstream ss(line);
            std::string cmd;
            ss >> cmd;

            if (cmd == "/quit" || cmd == "/exit" || cmd == "/q") {
                stop.store(true);
                break;
            } else if (cmd == "/to") {
                std::string new_recipient;
                std::getline(ss, new_recipient);
                // trim
                size_t first = new_recipient.find_first_not_of(" \t");
                if (first != std::string::npos) {
                    new_recipient = new_recipient.substr(first);
                }

                if (new_recipient.empty()) {
                    err("Usage: /to <username1, username2, ...>");
                } else {
                    state.recipient = new_recipient;
                    {
                        std::lock_guard<std::mutex> lock(g_io_mutex);
                        g_current_recipient = state.recipient;
                    }
                    ok("Switched — now chatting with " + c(config::BOLD, new_recipient));
                    info("Fetching history with " + new_recipient + "…");
                    fetch_history(base_url, token, me, new_recipient);
                    std::cout << std::endl;
                }
            } else if (cmd == "/list") {
                std::string partner;
                std::getline(ss, partner);
                size_t first = partner.find_first_not_of(" \t");
                if (first != std::string::npos) {
                    partner = partner.substr(first);
                } else {
                    partner = state.recipient;
                }
                fetch_history(base_url, token, me, partner);
            } else if (cmd == "/help") {
                show_help(state.recipient);
            } else if (cmd == "/edit") {
                std::string id_str, new_text;
                ss >> id_str;
                std::getline(ss, new_text);
                size_t first = new_text.find_first_not_of(" \t");
                if (first != std::string::npos) {
                    new_text = new_text.substr(first);
                }

                if (id_str.empty() || new_text.empty()) {
                    err("Usage: /edit <id> <new text>");
                } else {
                    httplib::Client cli(base_url);
                    nlohmann::json edit_req = {{"content", new_text}};
                    auto res = cli.Patch("/messages/" + id_str, headers, edit_req.dump(), "application/json");
                    if (res && res->status == 200) {
                        ok("Message #" + id_str + " updated.");
                    } else {
                        std::string detail = res ? res->body : "network error";
                        err("Edit failed: " + detail);
                    }
                }
            } else if (cmd == "/delete") {
                std::string id_str;
                ss >> id_str;
                if (id_str.empty()) {
                    err("Usage: /delete <id>");
                } else {
                    httplib::Client cli(base_url);
                    auto res = cli.Delete("/messages/" + id_str, headers);
                    if (res && res->status == 200) {
                        ok("Message #" + id_str + " deleted.");
                    } else {
                        std::string detail = res ? res->body : "network error";
                        err("Delete failed: " + detail);
                    }
                }
            } else {
                err("Unknown command '" + cmd + "'. Type /help for options.");
            }
            continue;
        }

        // Send a message
        auto recipients = split_commas(state.recipient);
        if (recipients.empty()) continue;

        httplib::Client cli(base_url);
        nlohmann::json msg_req = {
            {"recipients", recipients},
            {"content", line}
        };

        auto res = cli.Post("/messages", headers, msg_req.dump(), "application/json");
        if (!res || res->status != 201) {
            std::string detail = res ? res->body : "network error";
            err("Send failed: " + detail);
        }
    }
}

} // namespace logic
