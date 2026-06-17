#pragma once

#include "database.hpp"
#include "repository.hpp"
#include <httplib.h>

namespace routes {

// Setup HTTP endpoints and route handlers
void setup_routes(httplib::Server& svr, repository::Repository& repo);

} // namespace routes
