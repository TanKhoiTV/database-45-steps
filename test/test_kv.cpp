#include <gtest/gtest.h>
#include <filesystem>
#include <sstream>
#include "kv.h"
#include "test_utils.h"

const std::string test_db = (std::filesystem::temp_directory_path() / "kvdb_test_db").string();

TEST(KVTest, BasicOperations) {
    std::filesystem::remove(test_db);

    KeyValue kv(test_db);
    auto open_err = kv.open();
    ASSERT_FALSE(open_err) << "Failed to open KV: " << open_err.message();

    bytes key = to_bytes("conf");
    bytes val1 = to_bytes("v1");
    bytes val2 = to_bytes("v2");

    // 1. Initial Set (Success, state changed)
    auto upd1 = kv.set(key, val1);
    EXPECT_TRUE(upd1);

    // 2. Update with different value (Success, state changed)
    auto upd2 = kv.set(key, val2);
    EXPECT_TRUE(upd2); // Returns true because val2 != val1

    // 3. Update with identical value (Unsuccessful update, state unchanged)
    auto upd3 = kv.set(key, val2);
    ASSERT_TRUE(upd3.has_value());
    EXPECT_FALSE(upd3.value()); // Returns false because value is the same

    // 4. Test Get success
    auto g1 = kv.get(key);
    ASSERT_TRUE(g1.has_value());
    EXPECT_EQ(g1.value(), val2);

    // 5. Test Get missing
    auto g2 = kv.get(to_bytes("xxx"));
    EXPECT_TRUE(g2 && !(*g2));

    // 6. Test del missing
    auto del1 = kv.del(to_bytes("xxx"));
    EXPECT_FALSE(del1.value());

    // 7. Test del success
    auto del2 = kv.del(key);
    EXPECT_TRUE(del2.value());

    // Verify final state remains correct
    auto final = kv.get(key);
    EXPECT_TRUE(final.has_value());

    // Add another key before the last test
    bytes new_key = to_bytes("new key");
    bytes new_val = to_bytes("new val");
    auto upd4 = kv.set(new_key, new_val);
    ASSERT_TRUE(upd4.has_value());

    // --- Persistence TEST ---
    ASSERT_FALSE(kv.close());
    ASSERT_FALSE(kv.open());

    auto new_r1 = kv.get(key);
    EXPECT_FALSE(new_r1.value());

    auto new_r2 = kv.get(new_key);
    ASSERT_TRUE(new_r2.has_value());
    EXPECT_EQ(new_r2.value(), new_val);

    ASSERT_FALSE(kv.close());

    std::filesystem::remove(test_db);
}

TEST(KVTest, UpdateMode) {
    std::filesystem::remove(test_db);

    KeyValue kv(test_db);
    auto open_err = kv.open();
    ASSERT_FALSE(open_err) << "Failed to open KV: " << open_err.message();

    bytes key = to_bytes("conf");
    bytes val1 = to_bytes("v1");
    bytes val2 = to_bytes("v2");

    auto updated = kv.set_ex(key, val1, KeyValue::UpdateMode::Update);
    EXPECT_FALSE(updated.value());

    updated = kv.set_ex(key, val1, KeyValue::UpdateMode::Insert);
    EXPECT_TRUE(updated.value());

    updated = kv.set_ex(key, val2, KeyValue::UpdateMode::Insert);
    EXPECT_FALSE(updated.value());

    updated = kv.set_ex(key, val2, KeyValue::UpdateMode::Update);
    EXPECT_TRUE(updated.value());

    auto del = kv.del(key);
    EXPECT_TRUE(del);

    updated = kv.set_ex(key, val1, KeyValue::UpdateMode::Upsert);
    EXPECT_TRUE(updated.value());

    updated = kv.set_ex(key, val1, KeyValue::UpdateMode::Upsert);
    EXPECT_FALSE(updated.value());

    updated = kv.set_ex(key, val2, KeyValue::UpdateMode::Upsert);
    EXPECT_TRUE(updated.value());

    ASSERT_FALSE(kv.close());

    std::filesystem::remove(test_db);
}

TEST(KVTest, Recovery) {
    KeyValue kv(test_db);
    auto prepare = [&]() {
        std::filesystem::remove(test_db);

        auto open_err = kv.open();
        ASSERT_FALSE(open_err) << "Failed to open KV: " << open_err.message();

        auto updated = kv.set(to_bytes("k1"), to_bytes("v1"));
        ASSERT_TRUE(updated.value());

        updated = kv.set(to_bytes("k2"), to_bytes("v2"));
        ASSERT_TRUE(updated.value());

        kv.close();
    };

    // -- Truncated log --
    prepare();
    {
        auto size = std::filesystem::file_size(test_db);
        std::filesystem::resize_file(test_db, size - 1);

        ASSERT_FALSE(kv.open());

        auto val1 = kv.get(to_bytes("k1"));
        ASSERT_TRUE(val1.value());
        EXPECT_EQ(std::string(reinterpret_cast<const char *>((*val1)->data()), (*val1)->size()), "v1");

        auto val2 = kv.get(to_bytes("k2"));
        ASSERT_FALSE(val2.value());
    }
    ASSERT_FALSE(kv.close());

    // -- Bad checksum --
    prepare();
    {
        std::fstream fs(test_db, std::ios::in | std::ios::out | std::ios::binary);
        fs.seekp(-1, std::ios::end);
        fs.put('\0');
        fs.close();

        ASSERT_FALSE(kv.open());

        auto val1 = kv.get(to_bytes("k1"));
        ASSERT_TRUE(val1.value());
        EXPECT_EQ(std::string(reinterpret_cast<const char *>((*val1)->data()), (*val1)->size()), "v1");

        auto val2 = kv.get(to_bytes("k2"));
        ASSERT_FALSE(val2.value());
    }
    ASSERT_FALSE(kv.close());

    std::filesystem::remove(test_db);
}
