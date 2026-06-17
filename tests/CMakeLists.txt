include(GoogleTest)

add_executable(messenger_tests
    test_crypto.cpp
    test_auth.cpp
    test_repository.cpp
    test_services.cpp
    ../server/routes.cpp
    ../server/services.cpp
    ../server/auth.cpp
    ../server/crypto.cpp
    ../server/repository.cpp
    ../server/broadcaster.cpp
    ../server/utils.cpp
)

target_include_directories(messenger_tests PRIVATE ../server)

target_link_libraries(messenger_tests
    PRIVATE
        gtest_main
        httplib::httplib
        nlohmann_json::nlohmann_json
        jwt-cpp::jwt-cpp
        bcrypt
        SQLite::SQLite3
        OpenSSL::SSL
        OpenSSL::Crypto
)

target_compile_definitions(messenger_tests PRIVATE JWT_DISABLE_PICOJSON)

gtest_discover_tests(messenger_tests)
