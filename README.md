# Secure Messenger

A secure, real-time command-line messaging application written in modern C++ (C++17). The system uses **AES-256-GCM** for server-side database encryption, **Bcrypt** for password hashing, **JWT** for stateless session authentication, and **Server-Sent Events (SSE)** for real-time delivery of messages and presence updates.

---

## Key Features

- **End-to-End Database Encryption**: All message contents are encrypted before being saved to the SQLite database using AES-256-GCM.
- **Secure Authentication**: User authentication is powered by Bcrypt password hashing and JSON Web Tokens (JWT) for session management.
- **Real-Time Streaming**: Live chat and presence notifications are delivered instantly using Server-Sent Events (SSE).
- **Multi-Recipient Chats**: Send messages to multiple recipients simultaneously.
- **Message Lifecycle Management**: Edit or delete sent messages with real-time propagation to online recipients.
- **Beautiful CLI UI**: Interactive terminal client with color-coded layouts, loading indicators, and structured menus.

---

## Technology Stack

- **Language Standard**: C++17
- **Build System**: CMake (minimum version 3.15)
- **External Dependencies (automatically fetched via CMake):**
  - [nlohmann/json](https://github.com/nlohmann/json) - JSON parsing and serialization.
  - [cpp-httplib](https://github.com/yhirose/cpp-httplib) - HTTP/HTTPS server and client.
  - [jwt-cpp](https://github.com/Thalhammer/jwt-cpp) - JWT generation and validation.
  - [Bcrypt.cpp](https://github.com/hilch/Bcrypt.cpp) - Bcrypt implementation for C++.
- **System Dependencies:**
  - **OpenSSL** (Required for AES-256-GCM and SHA/HMAC functions)
  - **SQLite3** (Database storage engine)

---

## Prerequisites

Before building, make sure you have the required system packages installed.

### macOS (via Homebrew)
```bash
brew install cmake openssl sqlite
```

### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install build-essential cmake libssl-dev libsqlite3-dev
```

---

## Build Instructions

Build both the client and server targets using CMake:

```bash
# Create build directory
mkdir -p build && cd build

# Configure the project
cmake ..

# Build both executables (messenger_server and messenger_client)
cmake --build .
```

After a successful build, the binaries will be located under:
- **Server**: `build/server/messenger_server`
- **Client**: `build/client/messenger_client`

---

## Running the Tests

To compile and execute the automated test suite (GoogleTest):

```bash
# Compile the test suite target
cmake --build build --target messenger_tests

# Execute tests
./build/tests/messenger_tests
```

---

## Running the Application

### 1. Starting the Server

Launch the server executable to listen on port `8000`:

```bash
./build/server/messenger_server
```

#### Server Configurations (Environment Variables)

Configuration is managed using environment variables. An example template is provided in [.env.example](file:///Users/tova/workspace/untitled%20folder/cpp/.env.example):
- `MESSENGER_DB_PATH`: Custom path to the SQLite database file (defaults to `messenger.db`).
- `MESSENGER_ENCRYPTION_KEY`: A 32-byte hex-encoded key for AES-256-GCM. If not provided, the server attempts to load it from `.messenger.key` or securely generates a new one on startup.

---

### 2. Starting the Client

Launch the interactive CLI client. By default, it connects to `http://localhost:8000`:

```bash
./build/client/messenger_client
```

To connect to a custom server address, use the `-u` or `--url` flag:

```bash
./build/client/messenger_client --url http://192.168.1.100:8000
```

#### Client Interactive Flow
1. **Login / Register**: Enter your username and password. If the username does not exist, the client will prompt you to register.
2. **Select Partner**: Enter the username (or comma-separated list of usernames) you want to chat with.
3. **Chatting**: Start typing messages! They will be delivered instantly.

---

## CLI Client Commands

While inside the client's interactive input loop, you can type `/help` or use any of the following slash commands:

| Command | Arguments | Description |
| :--- | :--- | :--- |
| `/to` | `<username1, username2, ...>` | Switch conversation partner(s) |
| `/list` | `[recipient]` | Show full message history with the target user(s) |
| `/edit` | `<id> <new_text>` | Edit a message you previously sent by its message ID |
| `/delete`| `<id>` | Delete a message you previously sent by its message ID |
| `/help` | *None* | Show the help menu |
| `/quit` | *None* (or `/exit`, `/q`) | Safely terminate the client session and exit |

---

## Directory Structure

```text
.
├── CMakeLists.txt         # Root CMake project configuration
├── .env.example           # Template for server configuration
├── .env                   # Local server configuration (ignored by git)
├── messenger.db           # SQLite database file containing tables and indices
├── client/
│   ├── CMakeLists.txt     # Client build setup
│   ├── config.hpp         # Terminal colors and banner configuration
│   ├── logic.hpp/cpp      # Client CLI logic, networking, and SSE reader
│   └── main.cpp           # Client entrypoint and auth loop
└── server/
    ├── CMakeLists.txt            # Server build setup
    ├── main.cpp                  # Server entrypoint and setup
    ├── auth.hpp/cpp              # JWT generation and validation
    ├── crypto.hpp/cpp            # AES-256-GCM encryption helper functions
    ├── database.hpp              # RAII SQLite wrapper classes
    ├── models.hpp                # Data structures (User, Message)
    ├── thread_safe_queue.hpp     # Template for thread-safe event queue
    ├── repositories/
    │   ├── user_repository.hpp/cpp   # User SQLite query mappings
    │   └── message_repository.hpp/cpp# Message SQLite query mappings
    ├── routes/
    │   ├── routes_common.hpp/cpp     # Route authorization helpers
    │   ├── user_routes.hpp/cpp       # User REST & SSE route configurations
    │   └── message_routes.hpp/cpp    # Message REST route configurations
    ├── services/
    │   ├── service_exception.hpp     # Custom services exception definition
    │   ├── user_services.hpp/cpp     # User registration and login services
    │   └── message_services.hpp/cpp  # Message operations service layer
    └── utils.hpp/cpp             # General helper functions (timestamp, base64)
```
