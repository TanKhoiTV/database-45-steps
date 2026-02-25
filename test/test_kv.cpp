#include <gtest/gtest.h>
#include <filesystem>
#include <sstream>
#include "kv.h"
#include "test_utils.h"
#include "entry_codec.h"

// Helper to convert string literals to bytes for cleaner tests
bytes to_bytes(std::string_view s) {
    bytes b;
    for (char c : s) b.push_back(static_cast<std::byte>(c));
    return b;
}


const std::string test_db = (std::filesystem::temp_directory_path() / "kvdb_test_db").string();

TEST(KVTest, BasicOperationsAndPersistence) {
    std::filesystem::remove(test_db);

    KV kv(test_db);
    auto open_err = kv.Open();
    dump_file(test_db);
    ASSERT_FALSE(open_err) << "Failed to open KV: " << open_err.message();

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
    ASSERT_FALSE(kv.Close());
    ASSERT_FALSE(kv.Open());

    auto [new_r1, new_err1] = kv.Get(key);
    EXPECT_FALSE(new_r1.has_value());
    EXPECT_FALSE(new_err1);

    auto [new_r2, new_err2] = kv.Get(new_key);
    ASSERT_TRUE(new_r2.has_value());
    EXPECT_EQ(new_r2.value(), new_val);
    EXPECT_FALSE(new_err2);

    ASSERT_FALSE(kv.Close());

    std::filesystem::remove(test_db);
}

TEST(EntryTest, EncodeDecode) {
    // 1. Test regular entry
    Entry ent{to_bytes("k1"), to_bytes("xxx"), false};

    // Test round-trip only
    bytes expected_data = EntryCodec::encode(ent);

    BufferReader reader{std::span<const std::byte>(expected_data)};
    auto decoded = EntryCodec::decode(reader);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(std::holds_alternative<Entry>(decoded.value()));
    EXPECT_EQ(std::get<Entry>(decoded.value()), ent);

    // 2. Test deleted entry
    Entry tomb{to_bytes("k2"), {}, true};
    bytes expected_tomb = EntryCodec::encode(tomb);

    BufferReader tomb_reader{std::span<const std::byte>(expected_tomb)};
    auto decoded_tomb = EntryCodec::decode(tomb_reader);;
    ASSERT_TRUE(decoded_tomb.has_value());
    EXPECT_TRUE(std::holds_alternative<Entry>(decoded_tomb.value()));
    EXPECT_EQ(tomb, std::get<Entry>(decoded_tomb.value()));
}

TEST(EntryTest, EntryEOF) {
    bytes empty{};
    BufferReader empty_reader{std::span<const std::byte>(empty)};

    auto result = EntryCodec::decode(empty_reader);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::holds_alternative<EntryEOF>(result.value()));
}

TEST(EntryTest, BadChecksumDetection) {
    Entry ent{to_bytes("k1"), to_bytes("xxx"), false};
    bytes encoded = EntryCodec::encode(ent);

    encoded.back() ^= std::byte{0xFF};

    BufferReader reader{std::span<const std::byte>(encoded)};
    auto result = EntryCodec::decode(reader);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), db_error::bad_checksum);
}
