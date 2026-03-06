// include/row_codec.h
#pragma once

#include "core/types.h"
#include "core/bit_utils.h"
#include "table/row.h"
#include "table/schema.h"
#include "table/cell_codec.h"
#include <expected>
#include <system_error>


class RowCodec {
    public:

    static constexpr size_t KEY_PREFIX_SIZE = 5; // 4 (schema ID) + 1 (separator)
    static constexpr std::byte ID_SEPARATOR = static_cast<std::byte>(0x00);

    static bytes key_prefix(const Schema &schema);

    static inline Row new_row(const Schema &schema) {
        return Row(schema.cols_.size(), Cell::make_empty());
    }

    static std::expected<bytes, std::error_code> encode_key(const Schema &schema, const Row &row);
    static std::expected<bytes, std::error_code> encode_val(const Schema &schema, const Row &row);
    static std::error_code decode_key(const Schema &schema, Row &row, std::span<const std::byte> key);
    static std::error_code decode_val(const Schema &schema, Row &row, std::span<const std::byte> val);
};


