// test/table/test_row.cpp

/**
 * @file test_row.cpp
 * @brief Unit tests for @ref RowCodec key/value encode and decode.
 *
 * Uses a concrete three-column schema (`time i64, src str, dst str`) with
 * a composite primary key of (`src`, `dst`) to verify the exact binary
 * layout and full round-trip correctness.
 *
 * Expected key bytes:
 * ```
 * 01 00 00 00       schema_id = 1 (LE)
 * 00                ID_SEPARATOR
 * 01 00 00 00 61    str "a" (len=1, data='a')
 * 01 00 00 00 62    str "b" (len=1, data='b')
 * ```
 * Expected value bytes:
 * ```
 * 7B 00 00 00 00 00 00 00   i64 123 (LE)
 * ```
 */

#include <gtest/gtest.h>
#include "test_utils.h"         // to_bytes
#include "table/row.h"          // Row
#include "table/row_codec.h"    // RowCodec
#include "table/cell.h"         // Cell
#include "table/schema.h"       // Schema, ColumnHeader
#include "table/schema_codec.h" // SchemaCodec (included for completeness)
#include <vector>               // std::vector
#include <string>               // std::string
#include <cstdint>              // uint32_t

/**
 * @brief Verifies key encoding, value encoding, and full decode round-trip
 *        for a representative three-column row.
 */
TEST(RowTest, BasicOps) {
    auto schema = Schema{
        static_cast<uint32_t>(0x00000001),
        std::string{"link"},
        std::vector<ColumnHeader>{
            ColumnHeader{"time", Cell::Type::i64},
            ColumnHeader{"src", Cell::Type::str},
            ColumnHeader{"dst", Cell::Type::str}
        },
        std::vector<size_t>{1, 2}
    };

    auto row = Row{
        Cell::make_i64(123),
        Cell::make_str("a"),
        Cell::make_str("b")
    };

    auto key = bytes{std::byte{0x01}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0x00}, std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{'a'}, std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{'b'}};
    auto val = bytes{std::byte{123}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}};
    auto e_key = RowCodec::encode_key(schema, row);
    ASSERT_TRUE(e_key.has_value());
    EXPECT_EQ(e_key.value(), key);
    auto e_val = RowCodec::encode_val(schema, row);
    ASSERT_TRUE(e_val.has_value());
    EXPECT_EQ(e_val.value(), val);

    auto d_row = RowCodec::new_row(schema);
    auto err_1 = RowCodec::decode_key(schema, d_row, key);
    ASSERT_FALSE(err_1);
    auto err_2 = RowCodec::decode_val(schema, d_row, val);
    ASSERT_FALSE(err_2);
    EXPECT_EQ(d_row, row);
}
