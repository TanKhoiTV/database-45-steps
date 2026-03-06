// src/row_codec.cpp
#include "row_codec.h"
#include "db_error.h"
#include <utility>

bytes RowCodec::key_prefix(const Schema &schema) {
    auto prefix = bytes(5);
    auto ID = pack_le<uint32_t>(schema.id_);
    std::copy(ID.begin(), ID.end(), prefix.begin());
    prefix[4] = static_cast<std::byte>(ID_SEPARATOR);
    return prefix;
}

std::expected<bytes, std::error_code> RowCodec::encode_key(const Schema &schema, const Row &row) {
    if (schema.cols_.size() != row.size())
        return std::unexpected(db_error::inconsistent_length);

    auto key = key_prefix(schema);

    for (auto idx : schema.pkey_) {
        if (auto err = CellCodec::encode(row[idx], schema.cols_[idx].type_, key); err) {
            return std::unexpected(err);
        }
    }
    return key;
}

std::expected<bytes, std::error_code> RowCodec::encode_val(const Schema &schema, const Row &row) {
    if (schema.cols_.size() != row.size())
        return std::unexpected(db_error::inconsistent_length);

    auto val = bytes();
    size_t non_pkey_count = schema.cols_.size() - schema.pkey_.size();
    val.reserve(4 * non_pkey_count);

    for (size_t idx = 0; idx < schema.cols_.size(); ++idx) {
        if (schema.is_pkey(idx)) continue;
        if (auto err = CellCodec::encode(row[idx], schema.cols_[idx].type_, val); err) {
            return std::unexpected(err);
        }
    }

    return val;
}

std::error_code RowCodec::decode_key(const Schema &schema, Row &row, std::span<const std::byte> key) {
    if (schema.cols_.size() != row.size())
        return db_error::inconsistent_length;

    if (key.size() < KEY_PREFIX_SIZE) return db_error::expect_more_data;

    auto stored_id = unpack_le<uint32_t>(key.first<4>());
    if (stored_id != schema.id_) return db_error::bad_key;
    key = key.subspan<4>();
    if (key[0] != static_cast<std::byte>(ID_SEPARATOR))
        return db_error::bad_key;
    key = key.subspan<1>();

    for (auto idx : schema.pkey_) {
        auto res = CellCodec::decode(key, schema.cols_[idx].type_);
        if (!res.has_value()) return res.error();
        row[idx] = std::move(res.value());
    }

    return (!key.empty()) ? db_error::trailing_garbage : std::error_code{};
}

std::error_code RowCodec::decode_val(const Schema &schema, Row &row, std::span<const std::byte> val) {
    if (schema.cols_.size() != row.size())
        return db_error::inconsistent_length;

    for (size_t idx = 0; idx < schema.cols_.size(); ++idx) {
        if (schema.is_pkey(idx)) continue;
        auto res = CellCodec::decode(val, schema.cols_[idx].type_);
        if (!res.has_value()) return res.error();
        row[idx] = std::move(res.value());
    }

    return (!val.empty()) ? db_error::trailing_garbage : std::error_code{};
}
