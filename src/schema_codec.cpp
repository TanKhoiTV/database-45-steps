#include "schema_codec.h"
#include "types.h"
#include "bit_utils.h"
#include "db_error.h"
#include "cell_codec.h"

bytes SchemaCodec::encode(const Schema &schema) {
    bytes out;
    push_u32(out, schema.id_);
    push_str(out, schema.name_);
    push_u32(out, static_cast<uint32_t>(schema.cols_.size()));
    for (const auto &col : schema.cols_) {
        push_str(out, col.name_);
        out.push_back(static_cast<std::byte>(col.type_));
    }
    push_u32(out, static_cast<uint32_t>(schema.pkey_.size()));
    for (auto idx : schema.pkey_) {
        push_u32(out, static_cast<uint32_t>(idx));
    }
    return out;
}

std::expected<Schema, std::error_code> SchemaCodec::decode(std::span<const std::byte> buf) {
    auto id = read_u32(buf);
    if (!id) return std::unexpected(db_error::expect_more_data);

    auto name = read_str(buf);
    if (!name) return std::unexpected(db_error::expect_more_data);

    auto col_count = read_u32(buf);
    if (!col_count) return std::unexpected(db_error::expect_more_data);

    std::vector<ColumnHeader> cols;
    cols.reserve(*col_count);
    for (uint32_t i = 0; i < *col_count; ++i) {
        auto col_name = read_str(buf);
        if (!col_name) return std::unexpected(db_error::expect_more_data);
        auto col_type = CellCodec::read_cell_type(buf);
        if (!col_type) return std::unexpected(db_error::expect_more_data);
        cols.emplace_back(std::move(*col_name), *col_type);
    }

    auto pkey_count = read_u32(buf);
    if (!pkey_count) return std::unexpected(db_error::expect_more_data);

    std::vector<size_t> pkey;
    pkey.reserve(*pkey_count);
    for (uint32_t i = 0; i < *pkey_count; ++i) {
        auto key = read_u32(buf);
        if (!key) return std::unexpected(db_error::expect_more_data);
        if (*key >= cols.size()) return std::unexpected(db_error::bad_key);
        pkey.push_back(*key);
    }

    if (!buf.empty()) return std::unexpected(db_error::trailing_garbage);

    auto schema = Schema(
        *id,
        std::move(*name),
        std::move(cols),
        std::move(pkey)
    );
    return schema;
}
