#pragma once

#include <string>

namespace crypto {

// Load the AES key from environment variables or file, or generate a new one
void init_key();

// Encrypt a plain-text string to a base64 encoded AES-256-GCM ciphertext
std::string encrypt(const std::string& plaintext);

// Decrypt a base64 encoded AES-256-GCM ciphertext back to plaintext
std::string decrypt(const std::string& ciphertext_b64);

} // namespace crypto
