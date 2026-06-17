#pragma once

#include <string>

namespace config {

inline const std::string DEFAULT_BASE_URL = "http://localhost:8000";

// ANSI escape codes for formatting
inline const std::string RESET   = "\033[0m";
inline const std::string BOLD    = "\033[1m";
inline const std::string DIM     = "\033[2m";
inline const std::string RED     = "\033[31m";
inline const std::string GREEN   = "\033[32m";
inline const std::string YELLOW  = "\033[33m";
inline const std::string BLUE    = "\033[34m";
inline const std::string MAGENTA = "\033[35m";
inline const std::string CYAN    = "\033[36m";
inline const std::string WHITE   = "\033[37m";

inline const std::string BANNER_TEXT = R"(
  ╔══════════════════════════════════════════════════════════════╗
  ║                                                              ║
  ║     SECURE MESSENGER CLI                                     ║
  ║     AES-256-GCM / Bcrypt / JWT / SSE / C++17                 ║
  ║                                                              ║
  ╚══════════════════════════════════════════════════════════════╝
)";

} // namespace config
