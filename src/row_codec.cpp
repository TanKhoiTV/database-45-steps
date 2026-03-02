// src/row_codec.cpp
#include "row_codec.h"
#include "db_error.h"
#include <utility>

bytes RowCodec::key_prefix(const Schema &schema) {
    auto prefix = bytes(5);
    auto ID = pack_le<uint32_t>(schema.id);
    std::copy(ID.begin(), ID.end(), prefix.begin());
    prefix[4] = static_cast<std::byte>(ID_SEPARATOR);
    return prefix;
}

std::expected<bytes, std::error_code> RowCodec::encode_key(const Schema &schema, const Row &row) {
    if (schema.cols.size() != row.size())
        return std::unexpected(db_error::inconsistent_length);

    auto key = key_prefix(schema);

    for (auto idx : schema.pkey) {
        if (auto err = CellCodec::encode(row[idx], schema.cols[idx].type, key); err) {
            return std::unexpected(err);
        }
    }
    return key;
}

std::expected<bytes, std::error_code> RowCodec::encode_val(const Schema &schema, const Row &row) {
    if (schema.cols.size() != row.size())
        return std::unexpected(db_error::inconsistent_length);

    auto is_pkey = std::vector<bool>(schema.cols.size());
    for (auto i : schema.pkey) is_pkey.at(i) = true;

    auto val = bytes();
    size_t non_pkey_count = schema.cols.size() - schema.pkey.size();
    val.reserve(4 * non_pkey_count);

    for (size_t idx = 0; idx < schema.cols.size(); ++idx) {
        if (is_pkey[idx]) continue;
        if (auto err = CellCodec::encode(row[idx], schema.cols[idx].type, val); err) {
            return std::unexpected(err);
        }
    }

    return val;
}

std::error_code RowCodec::decode_key(const Schema &schema, Row &row, std::span<const std::byte> key) {
    if (schema.cols.size() != row.size())
        return db_error::inconsistent_length;

    if (key.size() < KEY_PREFIX_SIZE) return db_error::expect_more_data;

    auto stored_id = unpack_le<uint32_t>(key.first<4>());
    if (stored_id != schema.id) return db_error::bad_key;
    key = key.subspan<4>();
    if (key[0] != static_cast<std::byte>(ID_SEPARATOR))
        return db_error::bad_key;
    key = key.subspan<1>();

    for (auto idx : schema.pkey) {
        auto res = CellCodec::decode(key, schema.cols[idx].type);
        if (!res.has_value()) return res.error();
        row[idx] = std::move(res.value());
    }

    if (!key.empty()) return db_error::trailing_garbage;
    return {};
}

std::error_code RowCodec::decode_val(const Schema &schema, Row &row, std::span<const std::byte> val) {
    if (schema.cols.size() != row.size())
        return db_error::inconsistent_length;

    auto is_pkey = std::vector<bool>(schema.cols.size());
    for (auto i : schema.pkey) is_pkey.at(i) = true;

    for (size_t idx = 0; idx < schema.cols.size(); ++idx) {
        if (is_pkey[idx]) continue;
        auto res = CellCodec::decode(val, schema.cols[idx].type);
        if (!res.has_value()) return res.error();
        row[idx] = std::move(res.value());
    }

    if (!val.empty()) return db_error::trailing_garbage;
    return {};
}
