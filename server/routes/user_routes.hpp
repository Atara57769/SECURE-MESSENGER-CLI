#pragma once

#include "repositories/user_repository.hpp"
#include <httplib.h>

namespace routes {

void setup_user_routes(httplib::Server& svr, repository::UserRepository& user_repo);

} // namespace routes
