#include <gtest/gtest.h>
#include "cell.h"
#include "cell_codec.h"
#include <vector>
#include <span>

TEST(CellTest, EncodeDecode) {
    // --- Test I64 ---
    {
        auto cell = Cell::make_i64(-2);
        auto expected_data = bytes{
            std::byte{0xfe}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff},
            std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0xff}
        };

        bytes encoded;
        auto err_enc = CellCodec::encode(cell, Cell::Type::i64, encoded);
        ASSERT_FALSE(err_enc); // Success is usually a falsy error_code
        ASSERT_EQ(expected_data, encoded);

        std::span<const std::byte> decode_buf(encoded);
        auto decoded_res = CellCodec::decode(decode_buf, Cell::Type::i64);

        ASSERT_TRUE(decoded_res.has_value());
        ASSERT_EQ(cell, decoded_res.value());
        ASSERT_TRUE(decode_buf.empty()); // Equivalent to len(rest) == 0
    }

    // --- Test String (Bytes) ---
    {
        auto cell = Cell::make_str("asdf");
        auto expected_data = bytes{
            std::byte{4}, std::byte{0}, std::byte{0}, std::byte{0}, // Length prefix (LE)
            std::byte{'a'}, std::byte{'s'}, std::byte{'d'}, std::byte{'f'}
        };

        bytes encoded;
        auto err_enc = CellCodec::encode(cell, Cell::Type::str, encoded);
        ASSERT_FALSE(err_enc);
        ASSERT_EQ(expected_data, encoded);

        std::span<const std::byte> decode_buf(encoded);
        auto decoded_res = CellCodec::decode(decode_buf, Cell::Type::str);

        ASSERT_TRUE(decoded_res.has_value());
        ASSERT_EQ(cell, decoded_res.value());
        ASSERT_TRUE(decode_buf.empty());
    }
}
