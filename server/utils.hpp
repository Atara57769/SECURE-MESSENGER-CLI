#pragma once

#include <string>
#include <vector>

namespace utils {

// Get current database time in YYYY-MM-DD HH:MM:SS.ffffff format (UTC)
std::string get_current_db_time();

// Convert YYYY-MM-DD HH:MM:SS.ffffff (space) to YYYY-MM-DDTHH:MM:SS.ffffff (T separator)
std::string db_time_to_iso(const std::string& db_time);

// Base64 encode raw bytes to a string
std::string base64_encode(const std::vector<uint8_t>& data);

// Base64 decode a string to raw bytes
std::vector<uint8_t> base64_decode(const std::string& b64_str);

} // namespace utils
