add_executable(messenger_client
    main.cpp
    logic.cpp
)

target_link_libraries(messenger_client
    PRIVATE
        httplib::httplib
        nlohmann_json::nlohmann_json
        OpenSSL::SSL
        OpenSSL::Crypto
)
