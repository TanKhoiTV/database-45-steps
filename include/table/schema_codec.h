// include/table/schema_codec.h
#pragma once

/**
 * @file schema_codec.h
 * @brief Serialisation and deserialisation of @ref Schema objects in the @ref KeyValue store.
 *
 * Schemas are persisted as regular KV entries whose keys are distinguished
 * by well-known string prefixes:
 *
 * | Prefix                 | Purpose                                                   |
 * |------------------------|-----------------------------------------------------------|
 * | `@schema_<name>`       | Encoded @ref Schema for the table named `<name>`.         |
 * | `@counter`             | Monotonic counter used to assign unique @ref Schema::id_. |
 *
 * Encoded schema value layout (all integers little-endian):
 * ```
 * [ id(4) | name_len(4) | name | col_count(4)
 *   ( col_name_len(4) | col_name | col_type(1) ) * col_count
 *   pkey_count(4) | ( pkey_idx(4) ) * pkey_count ]
 * ```
 */

#include "table/schema.h"   // Schema
#include "core/types.h"     // bytes
#include <expected>         // std::expected
#include <system_error>     // std::error_code
#include <string_view>      // std::string_view
#include <span>             // std::span

/**
 * @brief Stateless codec for persisting and recovering @ref Schema objects.
 */
class SchemaCodec {
public:
    /** @brief KV key prefix for schema entries: `@schema_<table_name>`. */
    static constexpr std::string_view SCHEMA_KEY_PREFIX  = "@schema_";
    /** @brief KV key for the table-ID monotonic counter. */
    static constexpr std::string_view COUNTER_KEY_PREFIX = "@counter";

    /**
     * @brief Serialises @p schema into a flat byte buffer.
     * @param schema The schema to encode.
     * @return A @ref bytes buffer containing the complete encoded representation.
     */
    static bytes encode(const Schema &schema);

    /**
     * @brief Deserialises a @ref Schema from @p buf.
     *
     * @param buf The raw bytes previously produced by @ref encode.
     * @return The decoded @ref Schema, or an `std::error_code` on failure:
     *         - @ref db_error::expect_more_data — buffer is too short.
     *         - @ref db_error::bad_key          — a primary-key index exceeds the column count.
     *         - @ref db_error::trailing_garbage — unexpected bytes remain after decoding.
     */
    static std::expected<Schema, std::error_code> decode(std::span<const std::byte> buf);
};
