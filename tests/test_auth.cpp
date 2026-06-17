#include <gtest/gtest.h>
#include "auth.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <optional>

TEST(AuthTest, HashAndVerifyPassword) {
    std::string password = "my_secure_password";
    std::string hash = auth::hash_password(password);
    
    EXPECT_NE(password, hash);
    EXPECT_TRUE(auth::verify_password(password, hash));
    EXPECT_FALSE(auth::verify_password("wrong_password", hash));
    EXPECT_FALSE(auth::verify_password("", hash));
}

TEST(AuthTest, JwtRoundtrip) {
    std::string username = "alice";
    int version = 3;
    
    std::string token = auth::create_token(username, version);
    EXPECT_FALSE(token.empty());
    
    auto decoded_opt = auth::decode_token(token);
    ASSERT_TRUE(decoded_opt.has_value());
    
    auto decoded = *decoded_opt;
    EXPECT_EQ(decoded["sub"].get<std::string>(), username);
    EXPECT_EQ(decoded["version"].get<int>(), version);
}

TEST(AuthTest, DecodeInvalidOrTamperedToken) {
    // 1. Completely invalid token structure
    auto decoded_opt1 = auth::decode_token("invalid.jwt.token");
    EXPECT_FALSE(decoded_opt1.has_value());
    
    // 2. Tamper with signature
    std::string username = "bob";
    int version = 1;
    std::string token = auth::create_token(username, version);
    
    // JWT has 3 parts separated by dots: header.payload.signature
    // Let's modify a character in the signature part (after the second dot)
    size_t last_dot = token.find_last_of('.');
    ASSERT_NE(last_dot, std::string::npos);
    token[last_dot + 1] = (token[last_dot + 1] == 'A') ? 'B' : 'A';
    
    auto decoded_opt2 = auth::decode_token(token);
    EXPECT_FALSE(decoded_opt2.has_value());
}
