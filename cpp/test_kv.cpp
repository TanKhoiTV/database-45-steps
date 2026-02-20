#include <gtest/gtest.h>
#include "database.h"

// Helper to convert string literals to bytes for cleaner tests
bytes to_bytes(std::string_view s) {
    bytes b;
    for (char c : s) b.push_back(static_cast<std::byte>(c));
    return b;
}

TEST(KVTest, SetUpdates) {
    KV kv;
    kv.Open();

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

    // Verify final state remains correct
    auto [res, err4] = kv.Get(key);
    EXPECT_EQ(res.value(), val2);
    
    kv.Close();
}