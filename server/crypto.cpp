#include "crypto.hpp"
#include "utils.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sys/stat.h>
#include <cstdlib>

namespace crypto {

static std::vector<uint8_t> g_key;
static bool g_key_initialized = false;

// Evp Context Deleter for RAII
struct EvpCtxDeleter {
    void operator()(EVP_CIPHER_CTX* ctx) const {
        if (ctx) EVP_CIPHER_CTX_free(ctx);
    }
};
using EvpCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, EvpCtxDeleter>;

static std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        if (i + 1 < hex.length()) {
            std::string byteString = hex.substr(i, 2);
            uint8_t byte = static_cast<uint8_t>(std::strtol(byteString.c_str(), nullptr, 16));
            bytes.push_back(byte);
        }
    }
    return bytes;
}

static std::string bytes_to_hex(const std::vector<uint8_t>& bytes) {
    std::ostringstream oss;
    for (uint8_t b : bytes) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return oss.str();
}

static bool file_exists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

void init_key() {
    if (g_key_initialized) return;

    // 1. Check environment variable
    const char* env_key = std::getenv("MESSENGER_ENCRYPTION_KEY");
    if (env_key) {
        g_key = hex_to_bytes(env_key);
        if (g_key.size() == 32) {
            g_key_initialized = true;
            return;
        }
    }

    // 2. Check files in CWD or parent directory
    std::string filename = ".messenger.key";
    std::string path = filename;
    if (!file_exists(path)) {
        std::string parent_path = "../" + filename;
        if (file_exists(parent_path)) {
            path = parent_path;
        }
    }

    if (file_exists(path)) {
        std::ifstream ifs(path);
        std::string hex_str;
        if (ifs >> hex_str) {
            g_key = hex_to_bytes(hex_str);
            if (g_key.size() == 32) {
                g_key_initialized = true;
                return;
            }
        }
    }

    // 3. Generate a new key if not found
    g_key.resize(32);
    if (RAND_bytes(g_key.data(), g_key.size()) != 1) {
        throw std::runtime_error("Failed to generate secure random key using OpenSSL");
    }

    std::string new_hex = bytes_to_hex(g_key);
    std::ofstream ofs(filename);
    if (ofs << new_hex) {
        ofs.close();
        chmod(filename.c_str(), S_IRUSR | S_IWUSR); // owner-read-write only
    } else {
        std::cerr << "Warning: Could not save AES key to file " << filename << std::endl;
    }

    g_key_initialized = true;
}

std::string encrypt(const std::string& plaintext) {
    init_key();

    // 1. Generate 12-byte IV/nonce
    std::vector<uint8_t> nonce(12);
    if (RAND_bytes(nonce.data(), nonce.size()) != 1) {
        throw std::runtime_error("Failed to generate random nonce using OpenSSL");
    }

    // 2. Set up OpenSSL cipher context
    EvpCtxPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        throw std::runtime_error("Failed to create EVP_CIPHER_CTX");
    }

    // Initialize encryption with AES-256-GCM
    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        throw std::runtime_error("Failed to initialize EVP_EncryptInit_ex");
    }

    // Set IV length
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN, nonce.size(), nullptr) != 1) {
        throw std::runtime_error("Failed to set IV length");
    }

    // Initialize key and IV
    if (EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, g_key.data(), nonce.data()) != 1) {
        throw std::runtime_error("Failed to set key/IV");
    }

    // Encrypt
    std::vector<uint8_t> ciphertext(plaintext.size());
    int len = 0;
    if (EVP_EncryptUpdate(ctx.get(), ciphertext.data(), &len, 
                           reinterpret_cast<const uint8_t*>(plaintext.data()), plaintext.size()) != 1) {
        throw std::runtime_error("Encryption failed in EVP_EncryptUpdate");
    }
    int ciphertext_len = len;

    // Finalize
    std::vector<uint8_t> tag(16);
    if (EVP_EncryptFinal_ex(ctx.get(), tag.data(), &len) != 1) {
        throw std::runtime_error("Encryption failed in EVP_EncryptFinal_ex");
    }

    // Get the authentication tag
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_GET_TAG, 16, tag.data()) != 1) {
        throw std::runtime_error("Failed to get GCM authentication tag");
    }

    // 3. Combine nonce + ciphertext + tag
    std::vector<uint8_t> combined;
    combined.reserve(nonce.size() + ciphertext_len + tag.size());
    combined.insert(combined.end(), nonce.begin(), nonce.end());
    combined.insert(combined.end(), ciphertext.begin(), ciphertext.begin() + ciphertext_len);
    combined.insert(combined.end(), tag.begin(), tag.end());

    // 4. Base64 encode the final binary buffer
    return utils::base64_encode(combined);
}

std::string decrypt(const std::string& ciphertext_b64) {
    init_key();

    // 1. Decode base64 to binary buffer
    std::vector<uint8_t> combined = utils::base64_decode(ciphertext_b64);
    if (combined.size() < 12 + 16) {
        throw std::runtime_error("Ciphertext too short to be valid AES-GCM (missing nonce or tag)");
    }

    // 2. Split buffer
    std::vector<uint8_t> nonce(combined.begin(), combined.begin() + 12);
    std::vector<uint8_t> tag(combined.end() - 16, combined.end());
    std::vector<uint8_t> ciphertext(combined.begin() + 12, combined.end() - 16);

    // 3. Setup OpenSSL cipher context
    EvpCtxPtr ctx(EVP_CIPHER_CTX_new());
    if (!ctx) {
        throw std::runtime_error("Failed to create EVP_CIPHER_CTX");
    }

    // Initialize decryption with AES-256-GCM
    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        throw std::runtime_error("Failed to initialize EVP_DecryptInit_ex");
    }

    // Set IV length
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_IVLEN, nonce.size(), nullptr) != 1) {
        throw std::runtime_error("Failed to set IV length");
    }

    // Initialize key and IV
    if (EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, g_key.data(), nonce.data()) != 1) {
        throw std::runtime_error("Failed to set key/IV");
    }

    // Set expected authentication tag
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_AEAD_SET_TAG, tag.size(), tag.data()) != 1) {
        throw std::runtime_error("Failed to set expected authentication tag");
    }

    // Decrypt
    std::vector<uint8_t> decrypted(ciphertext.size());
    int len = 0;
    if (EVP_DecryptUpdate(ctx.get(), decrypted.data(), &len, ciphertext.data(), ciphertext.size()) != 1) {
        throw std::runtime_error("Decryption failed in EVP_DecryptUpdate");
    }
    int decrypted_len = len;

    // Finalize and verify tag
    int ret = EVP_DecryptFinal_ex(ctx.get(), decrypted.data() + len, &len);
    if (ret <= 0) {
        throw std::runtime_error("Integrity check failed: AEAD tag verification failed during decryption");
    }
    decrypted_len += len;

    return std::string(reinterpret_cast<char*>(decrypted.data()), decrypted_len);
}

} // namespace crypto
