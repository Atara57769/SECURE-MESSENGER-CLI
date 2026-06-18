add_executable(messenger_server
    main.cpp
    routes/routes_common.cpp
    routes/user_routes.cpp
    routes/message_routes.cpp
    services/user_services.cpp
    services/message_services.cpp
    auth.cpp
    crypto.cpp
    repositories/user_repository.cpp
    repositories/message_repository.cpp
    broadcaster.cpp
    utils.cpp
)

target_include_directories(messenger_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(messenger_server
    PRIVATE
        httplib::httplib
        nlohmann_json::nlohmann_json
        jwt-cpp::jwt-cpp
        bcrypt
        SQLite::SQLite3
        OpenSSL::SSL
        OpenSSL::Crypto
)

target_compile_definitions(messenger_server PRIVATE JWT_DISABLE_PICOJSON)
