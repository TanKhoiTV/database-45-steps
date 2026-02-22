#include <gtest/gtest.h>
#include <filesystem>
#include <sstream>
#include "database.h"

// Helper to convert string literals to bytes for cleaner tests
bytes to_bytes(std::string_view s) {
    bytes b;
    for (char c : s) b.push_back(static_cast<std::byte>(c));
    return b;
}

// Overload for Entry comparison (required for EXPECT_EQ)
bool operator==(const Entry& a, const Entry& b) {
    return a.key == b.key && a.val == b.val;
}


const std::string test_db = ".test_db";

// class KVTest : public ::testing::Test {
//     protected:

//     void SetUp() override {
//         std::filesystem::remove(test_db);
//     }

//     void TearDown() override {
//         std::filesystem::remove(test_db);
//     }
// };

TEST(KVTest, BasicOperationsAndPersistence) {
    KV kv(test_db);
    error open_err = kv.Open();
    std::cerr << open_err.message() << std::endl;
    ASSERT_FALSE(kv.Open());

    bytes key = to_bytes("conf");
    bytes val1 = to_bytes("v1");
    bytes val2 = to_bytes("v2");

    // 1. Initial Set (Success, state changed)
    auto [upd1, err1] = kv.Set(key, val1);
    EXPECT_TRUE(upd1); 
    EXPECT_FALSE(err1);

    // 2. Update with different value (Success, state changed)
    auto [upd2, err2] = kv.Set(key, val2);
    EXPECT_TRUE(upd2); // Returns true because val2 != val1
    EXPECT_FALSE(err2);

    // 3. Update with identical value (Unsuccessful update, state unchanged)
    auto [upd3, err3] = kv.Set(key, val2);
    EXPECT_FALSE(upd3); // Returns false because value is the same
    EXPECT_FALSE(err3);

    // 4. Test Get success
    auto [g1, g_err1] = kv.Get(key);
    ASSERT_TRUE(g1.has_value());
    EXPECT_EQ(g1.value(), val2);
    EXPECT_FALSE(g_err1);

    // 5. Test Get missing
    auto [g2, g_err2] = kv.Get(to_bytes("xxx"));
    EXPECT_FALSE(g2.has_value());
    EXPECT_FALSE(g_err2);

    // 6. Test Del missing
    auto [d1, d_err1] = kv.Del(to_bytes("xxx"));
    EXPECT_FALSE(d1);
    EXPECT_FALSE(d_err1);

    // 7. Test Del success
    auto [d2, d_err2] = kv.Del(key);
    EXPECT_TRUE(d2);
    EXPECT_FALSE(d_err2);

    // Verify final state remains correct
    auto [res, err4] = kv.Get(key);
    EXPECT_FALSE(res.has_value());

    // Add another key before the last test
    bytes new_key = to_bytes("new key");
    bytes new_val = to_bytes("new val");
    auto [upd4, s_err] = kv.Set(new_key, new_val);
    ASSERT_TRUE(upd4);
    ASSERT_FALSE(s_err);

    // --- Persistence TEST ---
    kv.Close();
    ASSERT_FALSE(kv.Open());

    auto [new_r1, new_err1] = kv.Get(key);
    EXPECT_FALSE(new_r1.has_value());
    EXPECT_FALSE(new_err1);

    auto [new_r2, new_err2] = kv.Get(new_key);
    ASSERT_TRUE(new_r2.has_value());
    EXPECT_EQ(new_r2.value(), new_val);
    EXPECT_FALSE(new_err2);
    
    kv.Close();
}

TEST(EntryTest, EncodeDecode) {
    // 1. Test regular entry
    Entry ent{to_bytes("k1"), to_bytes("xxx"), false};

    bytes expected_data = {
        std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0}, // klen
        std::byte{3}, std::byte{0}, std::byte{0}, std::byte{0}, // vlen
        std::byte{0},                                           // flag
        std::byte{'k'}, std::byte{'1'},                         // key
        std::byte{'x'}, std::byte{'x'}, std::byte{'x'}          // val
    };

    // Test encode
    EXPECT_EQ(ent.Encode(), expected_data);

    // Test decode using a stringstream as buffer
    std::string s_data(reinterpret_cast<const char *>(expected_data.data()), expected_data.size());
    std::stringstream ss(s_data);

    Entry decoded;
    error err = decoded.Decode(ss);

    EXPECT_EQ(ent, decoded);
    EXPECT_FALSE(err);

    // 2. Test deleted entry
    Entry tomb{to_bytes("k2"), {}, true};
    bytes expected_tomb = {
        std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0}, // klen
        std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}, // vlen
        std::byte{1},                                           // flag
        std::byte{'k'}, std::byte{'2'}                          // key
    };

    EXPECT_EQ(tomb.Encode(), expected_tomb);

    std::string s_tomb(reinterpret_cast<const char *>(expected_tomb.data()), expected_tomb.size());
    std::stringstream ss_tomb(s_tomb);
    Entry decoded_tomb;
    ASSERT_FALSE(decoded_tomb.Decode(ss_tomb));
    EXPECT_EQ(tomb, decoded_tomb);
}