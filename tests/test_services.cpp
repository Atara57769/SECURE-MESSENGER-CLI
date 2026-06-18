#include <gtest/gtest.h>
#include "database.hpp"
#include "repositories/user_repository.hpp"
#include "repositories/message_repository.hpp"
#include "services/user_services.hpp"
#include "services/message_services.hpp"
#include "auth.hpp"
#include "crypto.hpp"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <memory>

class ServicesTest : public ::testing::Test {
protected:
    std::unique_ptr<repository::SqliteDb> db;
    std::unique_ptr<repository::UserRepository> user_repo;
    std::unique_ptr<repository::MessageRepository> message_repo;

    void SetUp() override {
        db = std::make_unique<repository::SqliteDb>(":memory:");
        user_repo = std::make_unique<repository::UserRepository>(*db);
        message_repo = std::make_unique<repository::MessageRepository>(*db);
        user_repo->create_tables();
        message_repo->create_tables();
        
        // Ensure cryptography/JWT are initialized
        crypto::init_key();
        auth::init_jwt();
    }

    void TearDown() override {
        message_repo.reset();
        user_repo.reset();
        db.reset();
    }
};

TEST_F(ServicesTest, RegisterUserSuccessAndFailure) {
    // 1. Success case
    services::RegisterRequest req{"alice", "secret123"};
    nlohmann::json res = services::register_user(req, *user_repo);
    EXPECT_EQ(res["message"].get<std::string>(), "User registered successfully");

    // 2. Duplicate username
    EXPECT_THROW({
        services::register_user(req, *user_repo);
    }, services::ServiceException);

    // 3. Username too short
    services::RegisterRequest short_user{"al", "secret123"};
    try {
        services::register_user(short_user, *user_repo);
        FAIL() << "Should have thrown ServiceException";
    } catch (const services::ServiceException& e) {
        EXPECT_EQ(e.status_code(), 400);
    }

    // 4. Password too short
    services::RegisterRequest short_pass{"charlie", "123"};
    try {
        services::register_user(short_pass, *user_repo);
        FAIL() << "Should have thrown ServiceException";
    } catch (const services::ServiceException& e) {
        EXPECT_EQ(e.status_code(), 400);
    }
}

TEST_F(ServicesTest, AuthenticateUserSuccessAndFailure) {
    // Register alice
    services::RegisterRequest reg_req{"alice", "secret123"};
    services::register_user(reg_req, *user_repo);

    // 1. Success login
    services::LoginRequest login_req{"alice", "secret123"};
    nlohmann::json login_res = services::authenticate_user(login_req, *user_repo);
    
    EXPECT_TRUE(login_res.contains("access_token"));
    EXPECT_EQ(login_res["token_type"].get<std::string>(), "bearer");
    
    std::string token = login_res["access_token"].get<std::string>();

    // 2. Incorrect password login
    services::LoginRequest bad_pass{"alice", "wrongpass"};
    try {
        services::authenticate_user(bad_pass, *user_repo);
        FAIL() << "Should have thrown ServiceException";
    } catch (const services::ServiceException& e) {
        EXPECT_EQ(e.status_code(), 401);
    }

    // 3. Nonexistent user login
    services::LoginRequest bad_user{"charlie", "secret123"};
    try {
        services::authenticate_user(bad_user, *user_repo);
        FAIL() << "Should have thrown ServiceException";
    } catch (const services::ServiceException& e) {
        EXPECT_EQ(e.status_code(), 401);
    }
}

TEST_F(ServicesTest, SendAndFetchMessages) {
    // Register users
    services::register_user({"alice", "secret123"}, *user_repo);
    services::register_user({"bob", "secret456"}, *user_repo);

    // 1. Send message successfully
    services::SendMessageRequest send_req{"hello bob!", {"bob"}};
    auto send_res = services::process_send_message(send_req, "alice", *user_repo, *message_repo);
    
    ASSERT_EQ(send_res.size(), 1);
    EXPECT_GT(send_res[0].id, 0);
    EXPECT_EQ(send_res[0].sender, "alice");
    EXPECT_EQ(send_res[0].recipient, "bob");
    EXPECT_EQ(send_res[0].content, "hello bob!");

    // 2. Fetch messages
    auto alice_msgs = services::fetch_messages("alice", *message_repo);
    ASSERT_EQ(alice_msgs.size(), 1);
    EXPECT_EQ(alice_msgs[0].content, "hello bob!");

    auto bob_msgs = services::fetch_messages("bob", *message_repo);
    ASSERT_EQ(bob_msgs.size(), 1);
    EXPECT_EQ(bob_msgs[0].content, "hello bob!");

    // 3. Send to nonexistent user throws
    services::SendMessageRequest send_bad_req{"hello", {"nonexistent"}};
    EXPECT_THROW({
        services::process_send_message(send_bad_req, "alice", *user_repo, *message_repo);
    }, services::ServiceException);
}

TEST_F(ServicesTest, EditMessage) {
    services::register_user({"alice", "secret123"}, *user_repo);
    services::register_user({"bob", "secret456"}, *user_repo);

    auto send_res = services::process_send_message({"hello bob", {"bob"}}, "alice", *user_repo, *message_repo);
    int msg_id = send_res[0].id;

    // 1. Edit own message successfully
    services::UpdateMessageRequest edit_req{"hello bob (edited)"};
    auto edit_res = services::edit_message(msg_id, "alice", edit_req, *message_repo);
    
    EXPECT_EQ(edit_res.id, msg_id);
    EXPECT_EQ(edit_res.content, "hello bob (edited)");
    EXPECT_FALSE(edit_res.updated_at.empty());

    // 2. Try to edit someone else's message throws 403 Forbidden
    try {
        services::edit_message(msg_id, "bob", edit_req, *message_repo);
        FAIL() << "Should have thrown ServiceException";
    } catch (const services::ServiceException& e) {
        EXPECT_EQ(e.status_code(), 403);
    }
}

TEST_F(ServicesTest, DeleteMessage) {
    services::register_user({"alice", "secret123"}, *user_repo);
    services::register_user({"bob", "secret456"}, *user_repo);

    auto send_res = services::process_send_message({"hello bob", {"bob"}}, "alice", *user_repo, *message_repo);
    int msg_id = send_res[0].id;

    // 1. Try to delete someone else's message throws 403 Forbidden
    try {
        services::delete_message(msg_id, "bob", *message_repo);
        FAIL() << "Should have thrown ServiceException";
    } catch (const services::ServiceException& e) {
        EXPECT_EQ(e.status_code(), 403);
    }

    // 2. Delete own message successfully
    auto delete_res = services::delete_message(msg_id, "alice", *message_repo);
    EXPECT_EQ(delete_res.id, msg_id);
    EXPECT_TRUE(delete_res.is_deleted);
    EXPECT_EQ(delete_res.content, ""); // content should be cleared/hidden

    // 3. Try to edit a deleted message throws 400
    try {
        services::edit_message(msg_id, "alice", {"new text"}, *message_repo);
        FAIL() << "Should have thrown ServiceException";
    } catch (const services::ServiceException& e) {
        EXPECT_EQ(e.status_code(), 400);
    }
}

TEST_F(ServicesTest, ValidateStreamToken) {
    services::register_user({"alice", "secret123"}, *user_repo);

    // Login and get token
    auto login_res = services::authenticate_user({"alice", "secret123"}, *user_repo);
    std::string token1 = login_res["access_token"].get<std::string>();

    // 1. Verify token validation returns correct user and login version
    auto val_res = services::validate_stream_token(token1, *user_repo);
    EXPECT_EQ(val_res.first, "alice");
    EXPECT_EQ(val_res.second, 2); // registration version is 1, login increments to 2

    // 2. Perform second login (invalidates token1 because version increments to 3)
    auto login_res2 = services::authenticate_user({"alice", "secret123"}, *user_repo);
    std::string token2 = login_res2["access_token"].get<std::string>();

    // Token1 validation should now fail
    EXPECT_THROW({
        services::validate_stream_token(token1, *user_repo);
    }, services::ServiceException);

    // Token2 should succeed
    auto val_res2 = services::validate_stream_token(token2, *user_repo);
    EXPECT_EQ(val_res2.first, "alice");
    EXPECT_EQ(val_res2.second, 3);
}
