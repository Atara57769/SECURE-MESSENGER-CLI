#include "logic.hpp"
#include "config.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <atomic>

int main(int argc, char* argv[]) {
    std::string base_url = config::DEFAULT_BASE_URL;

    // Simple CLI arg parser for --url
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--url" || arg == "-u") && i + 1 < argc) {
            base_url = argv[i + 1];
            ++i;
        }
    }

    logic::banner();

    // ── Auth Flow ──────────────────────────────────────────────────────────
    std::string username;
    std::string password;
    std::string token;

    while (true) {
        username = logic::prompt("Username");
        password = logic::prompt("Password");

        if (username.empty() || password.empty()) {
            logic::err("Username and password cannot be empty.");
            continue;
        }

        token = logic::login(base_url, username, password);
        if (!token.empty()) {
            break; // Login success
        }

        // Login failed, offer registration
        std::string answer = logic::prompt("'" + username + "' not found or wrong password. Register? [y/N]");
        std::transform(answer.begin(), answer.end(), answer.begin(), ::tolower);
        
        if (answer != "y" && answer != "yes") {
            logic::err("Login failed. Exiting.");
            return 1;
        }

        // Register loop
        while (true) {
            std::string err_msg = logic::register_user(base_url, username, password);
            if (err_msg.empty()) {
                break; // Registered successfully
            }
            logic::err("Registration failed: " + err_msg);
            logic::info("Please try again (username >= 3 chars, password >= 6 chars).");
            username = logic::prompt("Username");
            password = logic::prompt("Password");
        }

        logic::ok("Registered as '" + username + "'.");
        
        // Log in after successful registration
        token = logic::login(base_url, username, password);
        if (!token.empty()) {
            break; // Login success
        } else {
            logic::err("Login after registration failed. Exiting.");
            return 1;
        }
    }

    logic::ok("Logged in as " + logic::c(config::BOLD, username) + ".");
    std::cout << std::endl;

    // ── Choose recipient ───────────────────────────────────────────────────
    std::string recipient = logic::prompt("Chat with (username)");
    while (recipient.empty()) {
        logic::err("Please enter a username.");
        recipient = logic::prompt("Chat with (username)");
    }

    logic::State state{recipient};

    // ── Fetch history ──────────────────────────────────────────────────────
    logic::fetch_history(base_url, token, username, recipient);

    // ── SSE and Input Loop threads ─────────────────────────────────────────
    std::atomic<bool> stop(false);

    // Run SSE stream listener in a background thread
    std::thread listener_thread([base_url, token, username, &stop]() {
        logic::listen_stream(base_url, token, username, stop);
    });

    // Run interactive input loop in the main thread
    logic::input_loop(base_url, token, username, state, stop);

    // Quit and join
    stop.store(true);
    if (listener_thread.joinable()) {
        listener_thread.join();
    }

    std::cout << std::endl;
    logic::info("Goodbye.");

    return 0;
}
