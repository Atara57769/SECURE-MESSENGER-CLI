#pragma once

#include <string>
#include <vector>
#include <atomic>

namespace logic {

struct State {
    std::string recipient;
};

// Colors formatting helper
std::string c(const std::string& code, const std::string& text);

// Print banner
void banner();

// Print different types of information messages
void info(const std::string& msg);
void ok(const std::string& msg);
void err(const std::string& msg);
void warn(const std::string& msg);

// Prompt user for input
std::string prompt(const std::string& label);

// Display interactive command help
void show_help(const std::string& current_recipient);

// Login and get JWT token, returns empty string on failure
std::string login(const std::string& base_url, const std::string& username, const std::string& password);

// Register user, returns empty string on success, or error details on failure
std::string register_user(const std::string& base_url, const std::string& username, const std::string& password);

// Fetch decrypted messages and print target conversation history
void fetch_history(const std::string& base_url, const std::string& token, const std::string& me, const std::string& filter_recipient);

// Listen to the SSE stream in a loop, prints events in real-time
void listen_stream(const std::string& base_url, const std::string& token, const std::string& me, std::atomic<bool>& stop);

// Interactive CLI input loop for typing messages and sending slash commands
void input_loop(const std::string& base_url, const std::string& token, const std::string& me, State& state, std::atomic<bool>& stop);

} // namespace logic
