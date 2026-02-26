#include <gtest/gtest.h>
#include <filesystem>
#include <sstream>
#include "kv.h"
#include "test_utils.h"

const std::string test_db = (std::filesystem::temp_directory_path() / "kvdb_test_db").string();

TEST(KVTest, BasicOperations) {
    std::filesystem::remove(test_db);

    KV kv(test_db);
    auto open_err = kv.open();
    ASSERT_FALSE(open_err) << "Failed to open KV: " << open_err.message();

    bytes key = to_bytes("conf");
    bytes val1 = to_bytes("v1");
    bytes val2 = to_bytes("v2");

    // 1. Initial Set (Success, state changed)
    auto [upd1, err1] = kv.set(key, val1);
    EXPECT_TRUE(upd1);
    EXPECT_FALSE(err1);

    // 2. Update with different value (Success, state changed)
    auto [upd2, err2] = kv.set(key, val2);
    EXPECT_TRUE(upd2); // Returns true because val2 != val1
    EXPECT_FALSE(err2);

    // 3. Update with identical value (Unsuccessful update, state unchanged)
    auto [upd3, err3] = kv.set(key, val2);
    EXPECT_FALSE(upd3); // Returns false because value is the same
    EXPECT_FALSE(err3);

    // 4. Test Get success
    auto [g1, g_err1] = kv.get(key);
    ASSERT_TRUE(g1.has_value());
    EXPECT_EQ(g1.value(), val2);
    EXPECT_FALSE(g_err1);

    // 5. Test Get missing
    auto [g2, g_err2] = kv.get(to_bytes("xxx"));
    EXPECT_FALSE(g2.has_value());
    EXPECT_FALSE(g_err2);

    // 6. Test del missing
    auto [d1, d_err1] = kv.del(to_bytes("xxx"));
    EXPECT_FALSE(d1);
    EXPECT_FALSE(d_err1);

    // 7. Test del success
    auto [d2, d_err2] = kv.del(key);
    EXPECT_TRUE(d2);
    EXPECT_FALSE(d_err2);

    // Verify final state remains correct
    auto [res, err4] = kv.get(key);
    EXPECT_FALSE(res.has_value());

    // Add another key before the last test
    bytes new_key = to_bytes("new key");
    bytes new_val = to_bytes("new val");
    auto [upd4, s_err] = kv.set(new_key, new_val);
    ASSERT_TRUE(upd4);
    ASSERT_FALSE(s_err);

    // --- Persistence TEST ---
    ASSERT_FALSE(kv.close());
    ASSERT_FALSE(kv.open());

    auto [new_r1, new_err1] = kv.get(key);
    EXPECT_FALSE(new_r1.has_value());
    EXPECT_FALSE(new_err1);

    auto [new_r2, new_err2] = kv.get(new_key);
    ASSERT_TRUE(new_r2.has_value());
    EXPECT_EQ(new_r2.value(), new_val);
    EXPECT_FALSE(new_err2);

    ASSERT_FALSE(kv.close());

    std::filesystem::remove(test_db);
}

TEST(KVTest, UpdateMode) {
    std::filesystem::remove(test_db);

    KV kv(test_db);
    auto open_err = kv.open();
    ASSERT_FALSE(open_err) << "Failed to open KV: " << open_err.message();

    bytes key = to_bytes("conf");
    bytes val1 = to_bytes("v1");
    bytes val2 = to_bytes("v2");

    {
        auto [updated, err] = kv.set(key, val1, KV::UpdateMode::Update);
        EXPECT_FALSE(err);
        EXPECT_FALSE(updated);
    }

    {
        auto [updated, err] = kv.set(key, val1, KV::UpdateMode::Insert);
        EXPECT_FALSE(err);
        EXPECT_TRUE(updated);
    }

    {
        auto [updated, err] = kv.set(key, val2, KV::UpdateMode::Insert);
        EXPECT_FALSE(err);
        EXPECT_FALSE(updated);
    }

    {
        auto [updated, err] = kv.set(key, val2, KV::UpdateMode::Update);
        EXPECT_FALSE(err);
        EXPECT_TRUE(updated);
    }

    auto [del, del_err] = kv.del(key);
    EXPECT_TRUE(del);
    ASSERT_FALSE(del_err);

    {
        auto [updated, err] = kv.set(key, val1, KV::UpdateMode::Upsert);
        EXPECT_FALSE(err);
        EXPECT_TRUE(updated);
    }

    {
        auto [updated, err] = kv.set(key, val1, KV::UpdateMode::Upsert);
        EXPECT_FALSE(err);
        EXPECT_FALSE(updated);
    }

    {
        auto [updated, err] = kv.set(key, val2, KV::UpdateMode::Upsert);
        EXPECT_FALSE(err);
        EXPECT_TRUE(updated);
    }

    ASSERT_FALSE(kv.close());

    std::filesystem::remove(test_db);
}

TEST(KVTest, Recovery) {
    KV kv(test_db);
    auto prepare = [&]() {
        std::filesystem::remove(test_db);

        auto open_err = kv.open();
        ASSERT_FALSE(open_err) << "Failed to open KV: " << open_err.message();

        {
            auto [updated, err] = kv.set(to_bytes("k1"), to_bytes("v1"));
            ASSERT_TRUE(updated);
            ASSERT_FALSE(err);
        }
        {
            auto [updated, err] = kv.set(to_bytes("k2"), to_bytes("v2"));
            ASSERT_TRUE(updated);
            ASSERT_FALSE(err);
        }

        kv.close();
    };

    // -- Truncated log --
    prepare();
    {
        auto size = std::filesystem::file_size(test_db);
        std::filesystem::resize_file(test_db, size - 1);

        ASSERT_FALSE(kv.open());

        auto [val1, err1] = kv.get(to_bytes("k1"));
        ASSERT_TRUE(val1.has_value());
        EXPECT_EQ(std::string(reinterpret_cast<const char *>(val1->data()), val1->size()), "v1");

        auto [val2, err2] = kv.get(to_bytes("k2"));
        ASSERT_FALSE(val2.has_value());
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

        auto [val1, err1] = kv.get(to_bytes("k1"));
        ASSERT_TRUE(val1.has_value());
        EXPECT_EQ(std::string(reinterpret_cast<const char *>(val1->data()), val1->size()), "v1");

        auto [val2, err2] = kv.get(to_bytes("k2"));
        ASSERT_FALSE(val2.has_value());
    }
    ASSERT_FALSE(kv.close());

    std::filesystem::remove(test_db);
}
