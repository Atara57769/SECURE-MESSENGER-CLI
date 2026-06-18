#pragma once

#include "repositories/user_repository.hpp"
#include "repositories/message_repository.hpp"
#include <httplib.h>

namespace routes {

void setup_message_routes(httplib::Server& svr, repository::UserRepository& user_repo, repository::MessageRepository& message_repo);

} // namespace routes
