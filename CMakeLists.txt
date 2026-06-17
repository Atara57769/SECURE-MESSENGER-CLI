cmake_minimum_required(VERSION 3.15)
project(secure_messenger CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# 1. Fetch nlohmann/json
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# 2. Fetch cpp-httplib
FetchContent_Declare(
    cpp-httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.18.3
)
set(HTTPLIB_REQUIRE_OPENSSL ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cpp-httplib)

# 3. Fetch jwt-cpp
FetchContent_Declare(
    jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG        v0.7.0
)
set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(jwt-cpp)

# 4. Fetch Bcrypt.cpp
FetchContent_Declare(
    bcrypt
    GIT_REPOSITORY https://github.com/hilch/Bcrypt.cpp.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(bcrypt)

# Find SQLite3 and OpenSSL (which are installed on the system)
find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)

add_subdirectory(server)
add_subdirectory(client)
