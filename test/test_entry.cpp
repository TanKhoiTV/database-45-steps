#include <gtest/gtest.h>
#include "entry.h"
#include "entry_codec.h"
#include "test_utils.h"


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
