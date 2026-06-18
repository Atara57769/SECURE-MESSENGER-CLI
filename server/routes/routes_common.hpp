#pragma once

#include "repositories/user_repository.hpp"
#include <httplib.h>
#include <string>

namespace routes {

// Helper to enforce authorization on standard REST routes.
// Returns username if successful, otherwise sets response status and returns empty string.
std::string require_auth(const httplib::Request& req, httplib::Response& res, repository::UserRepository& repo);

} // namespace routes
