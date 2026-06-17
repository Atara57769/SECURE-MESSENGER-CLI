#include <gtest/gtest.h>
#include "crypto.hpp"
#include "utils.hpp"
#include <stdexcept>
#include <string>

TEST(CryptoTest, EncryptNotPlaintext) {
    std::string original = "Secret message 123";
    std::string ciphertext = crypto::encrypt(original);
    
    EXPECT_NE(original, ciphertext);
    EXPECT_FALSE(ciphertext.empty());
}

TEST(CryptoTest, EncryptDecryptRoundtrip) {
    std::string original = "Testing 1, 2, 3... This is a very secret message.";
    std::string ciphertext = crypto::encrypt(original);
    
    std::string decrypted = crypto::decrypt(ciphertext);
    EXPECT_EQ(original, decrypted);
}

TEST(CryptoTest, TamperCiphertextThrowsException) {
    std::string original = "Secure text";
    std::string ciphertext = crypto::encrypt(original);
    
    // Base64 decode, modify one byte in the ciphertext, base64 encode back
    std::vector<uint8_t> decoded = utils::base64_decode(ciphertext);
    ASSERT_GE(decoded.size(), 20); // must have nonce + tag + ciphertext
    
    // Modify a byte in the encrypted content (nonce is first 12 bytes, tag is last 16 bytes)
    decoded[15] ^= 0xFF; 
    
    std::string tampered_ciphertext = utils::base64_encode(decoded);
    
    EXPECT_THROW({
        crypto::decrypt(tampered_ciphertext);
    }, std::runtime_error);
}

TEST(CryptoTest, InvalidBase64ThrowsException) {
    std::string invalid_b64 = "!!!This is not valid base64!!!";
    EXPECT_THROW({
        crypto::decrypt(invalid_b64);
    }, std::runtime_error);
}
