#define JWT_DISABLE_PICOJSON

#include "auth.hpp"
#include "bcrypt.h"
#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>
#include <openssl/rand.h>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <sys/stat.h>
#include <cstdlib>

namespace auth {

static std::string g_jwt_secret;
static bool g_jwt_initialized = false;

static bool file_exists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

void init_jwt() {
    if (g_jwt_initialized) return;

    // 1. Check environment variable
    const char* env_secret = std::getenv("MESSENGER_JWT_SECRET");
    if (env_secret) {
        g_jwt_secret = env_secret;
        g_jwt_initialized = true;
        return;
    }

    // 2. Check local files in CWD or parent
    std::string filename = ".jwt.key";
    std::string path = filename;
    if (!file_exists(path)) {
        std::string parent_path = "../" + filename;
        if (file_exists(parent_path)) {
            path = parent_path;
        }
    }

    if (file_exists(path)) {
        std::ifstream ifs(path);
        std::string secret;
        if (ifs >> secret) {
            g_jwt_secret = secret;
            g_jwt_initialized = true;
            return;
        }
    }

    // 3. Generate a 32-byte secure random value, formatted as a 64-char hex string
    std::vector<uint8_t> secret_bytes(32);
    if (RAND_bytes(secret_bytes.data(), secret_bytes.size()) != 1) {
        throw std::runtime_error("Failed to generate random secret bytes using OpenSSL");
    }

    std::ostringstream oss;
    for (uint8_t b : secret_bytes) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    g_jwt_secret = oss.str();

    std::ofstream ofs(filename);
    if (ofs << g_jwt_secret) {
        ofs.close();
        chmod(filename.c_str(), S_IRUSR | S_IWUSR); // owner-read-write only
    } else {
        std::cerr << "Warning: Could not save JWT secret to file " << filename << std::endl;
    }

    g_jwt_initialized = true;
}

std::string hash_password(const std::string& plain) {
    return bcrypt::generateHash(plain);
}

bool verify_password(const std::string& plain, const std::string& hashed) {
    return bcrypt::validatePassword(plain, hashed);
}

std::string create_token(const std::string& username, int version) {
    init_jwt();

    auto now = std::chrono::system_clock::now();
    auto expire = now + std::chrono::hours(24);

    auto token = jwt::create()
        .set_subject(username)
        .set_payload_claim("version", jwt::claim(version))
        .set_expires_at(expire)
        .sign(jwt::algorithm::hs256{g_jwt_secret});

    return token;
}

std::optional<nlohmann::json> decode_token(const std::string& token) {
    init_jwt();

    try {
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{g_jwt_secret});
        verifier.verify(decoded);

        nlohmann::json payload = nlohmann::json::object();
        for (const auto& pair : decoded.get_payload_claims()) {
            payload[pair.first] = pair.second.as_json();
        }
        return payload;
    } catch (const std::exception& e) {
        // Token expired, signature invalid, or malformed payload structure
        return std::nullopt;
    }
}

} // namespace auth
