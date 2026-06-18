#include <gtest/gtest.h>
#include "database.hpp"
#include "repositories/user_repository.hpp"
#include "repositories/message_repository.hpp"
#include "models.hpp"
#include <optional>
#include <vector>
#include <memory>

class RepositoryTest : public ::testing::Test {
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
    }

    void TearDown() override {
        message_repo.reset();
        user_repo.reset();
        db.reset();
    }
};

TEST_F(RepositoryTest, UserOperations) {
    // 1. Initially user should not exist
    auto user_opt = user_repo->get_user_by_username("alice");
    EXPECT_FALSE(user_opt.has_value());

    // 2. Create user
    repository::User alice = user_repo->create_user("alice", "hashed_password_123");
    EXPECT_GT(alice.id, 0);
    EXPECT_EQ(alice.username, "alice");
    EXPECT_EQ(alice.password_hash, "hashed_password_123");
    EXPECT_EQ(alice.login_version, 1);
    EXPECT_FALSE(alice.created_at.empty());

    // 3. Retrieve user
    user_opt = user_repo->get_user_by_username("alice");
    ASSERT_TRUE(user_opt.has_value());
    EXPECT_EQ(user_opt->id, alice.id);
    EXPECT_EQ(user_opt->username, "alice");
    EXPECT_EQ(user_opt->password_hash, "hashed_password_123");
    EXPECT_EQ(user_opt->login_version, 1);

    // 4. Increment login version
    user_repo->increment_login_version(alice.id);
    user_opt = user_repo->get_user_by_username("alice");
    ASSERT_TRUE(user_opt.has_value());
    EXPECT_EQ(user_opt->login_version, 2);
}

TEST_F(RepositoryTest, MessageOperations) {
    // 1. Create a message
    std::string sender = "alice";
    std::string recipient = "bob";
    std::string ciphertext = "encrypted_hello_world";

    repository::Message msg = message_repo->create_message(sender, recipient, ciphertext);
    EXPECT_GT(msg.id, 0);
    EXPECT_EQ(msg.sender, sender);
    EXPECT_EQ(msg.recipient, recipient);
    EXPECT_EQ(msg.ciphertext, ciphertext);
    EXPECT_FALSE(msg.created_at.empty());
    EXPECT_TRUE(msg.updated_at.empty());
    EXPECT_FALSE(msg.is_deleted);

    // 2. Retrieve message by ID
    auto msg_opt = message_repo->get_message_by_id(msg.id);
    ASSERT_TRUE(msg_opt.has_value());
    EXPECT_EQ(msg_opt->id, msg.id);
    EXPECT_EQ(msg_opt->sender, sender);
    EXPECT_EQ(msg_opt->ciphertext, ciphertext);

    // 3. Retrieve messages for user (sent and received)
    auto alice_msgs = message_repo->get_messages_for_user("alice");
    ASSERT_EQ(alice_msgs.size(), 1);
    EXPECT_EQ(alice_msgs[0].id, msg.id);

    auto bob_msgs = message_repo->get_messages_for_user("bob");
    ASSERT_EQ(bob_msgs.size(), 1);
    EXPECT_EQ(bob_msgs[0].id, msg.id);

    auto charlie_msgs = message_repo->get_messages_for_user("charlie");
    EXPECT_TRUE(charlie_msgs.empty());

    // 4. Update message content
    std::string new_ciphertext = "encrypted_hello_world_v2";
    message_repo->update_message(msg.id, new_ciphertext, false);

    msg_opt = message_repo->get_message_by_id(msg.id);
    ASSERT_TRUE(msg_opt.has_value());
    EXPECT_EQ(msg_opt->ciphertext, new_ciphertext);
    EXPECT_FALSE(msg_opt->updated_at.empty());
    EXPECT_FALSE(msg_opt->is_deleted);

    // 5. Delete message (update with is_deleted = true)
    message_repo->update_message(msg.id, new_ciphertext, true);

    msg_opt = message_repo->get_message_by_id(msg.id);
    ASSERT_TRUE(msg_opt.has_value());
    EXPECT_TRUE(msg_opt->is_deleted);

    // get_messages_for_user should filter out deleted messages
    alice_msgs = message_repo->get_messages_for_user("alice");
    EXPECT_TRUE(alice_msgs.empty());
}
