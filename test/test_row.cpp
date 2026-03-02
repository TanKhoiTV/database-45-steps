#include "test_utils.h"
#include "row.h"
#include "row_codec.h"
#include "cell.h"
#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <cstdint>

TEST(RowTest, BasicOps) {
    auto schema = Schema{
        .id = static_cast<uint32_t>(0x00000001),
        .name = std::string{"link"},
        .cols = std::vector<ColumnHeader>{
            ColumnHeader{"time", Cell::Type::i64},
            ColumnHeader{"src", Cell::Type::str},
            ColumnHeader{"dst", Cell::Type::str}
        },
        .pkey = std::vector<size_t>{1, 2}
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
