add_executable(messenger_server
    main.cpp
    routes.cpp
    services.cpp
    auth.cpp
    crypto.cpp
    repository.cpp
    broadcaster.cpp
    utils.cpp
)

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
