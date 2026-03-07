// include/table/row_codec.h
#pragma once

/**
 * @file row_codec.h
 * @brief Binary serialisation and deserialisation of @ref Row objects.
 *
 * A row is split across two @ref KeyValue entries:
 *
 * **Key** layout:
 * ```
 * [ schema_id(4) | separator(1) | pk_cell_0 | pk_cell_1 | ... ]
 * ```
 * Only primary-key columns are encoded into the KV key; this allows point
 * lookups directly from primary-key values.
 *
 * **Value** layout:
 * ```
 * [ non_pk_cell_0 | non_pk_cell_1 | ... ]
 * ```
 * Non-key columns are encoded in column-declaration order, skipping key columns.
 */

#include "core/types.h"         // bytes
#include "core/bit_utils.h"     // pack_le, unpack_le
#include "table/row.h"          // Row
#include "table/schema.h"       // Schema
#include "table/cell_codec.h"   // CellCodec
#include <expected>             // std::expected
#include <system_error>         // std::error_code
#include <span>                 // std::span

/**
 * @brief Stateless codec that maps a (@ref Schema, @ref Row) pair to/from
 *        the binary key and value stored in the @ref KeyValue layer.
 */
class RowCodec {
public:
    /** @brief Total size of the key prefix in bytes (`schema_id(4) + separator(1)`). */
    static constexpr size_t     KEY_PREFIX_SIZE = 5;
    /** @brief Byte written between the schema ID and the primary-key payload. */
    static constexpr std::byte  ID_SEPARATOR    = static_cast<std::byte>(0x00);

    /**
     * @brief Builds the 5-byte key prefix for @p schema.
     *
     * All keys belonging to this table begin with this prefix, which allows
     * efficient range scans to be scoped to a single table.
     *
     * @param schema The table schema.
     * @return A 5-byte @ref bytes containing `[ schema_id(4 LE) | 0x00 ]`.
     */
    static bytes key_prefix(const Schema &schema);

    /**
     * @brief Allocates a blank @ref Row with one NULL cell per column in @p schema.
     * @param schema The table schema.
     * @return A `Row` of `schema.cols_.size()` default-constructed (empty) @ref Cell values.
     */
    static inline Row new_row(const Schema &schema) {
        return Row(schema.cols_.size(), Cell::make_empty());
    }

    /**
     * @brief Encodes the primary-key columns of @p row into a KV key.
     * @param schema Provides column types and primary-key indices.
     * @param row    Source row; size must equal `schema.cols_.size()`.
     * @return The encoded key bytes, or @ref db_error::inconsistent_length /
     *         @ref db_error::type_mismatch on failure.
     */
    static std::expected<bytes, std::error_code> encode_key(const Schema &schema, const Row &row);

    /**
     * @brief Encodes the non-primary-key columns of @p row into a KV value.
     * @param schema Provides column types and primary-key membership.
     * @param row    Source row; size must equal `schema.cols_.size()`.
     * @return The encoded value bytes, or @ref db_error::inconsistent_length /
     *         @ref db_error::type_mismatch on failure.
     */
    static std::expected<bytes, std::error_code> encode_val(const Schema &schema, const Row &row);

    /**
     * @brief Decodes primary-key cells from @p key into the corresponding positions of @p row.
     *
     * Validates the 4-byte schema ID and the separator byte before reading cell data.
     * Returns @ref db_error::trailing_garbage if bytes remain after all key columns are decoded.
     *
     * @param schema Provides the expected schema ID and key-column types.
     * @param row    Destination row (modified in-place); size must equal `schema.cols_.size()`.
     * @param key    Raw key bytes as stored in the @ref KeyValue layer.
     * @return Empty error code on success; a @ref db_error otherwise.
     */
    static std::error_code decode_key(const Schema &schema, Row &row, std::span<const std::byte> key);

    /**
     * @brief Decodes non-primary-key cells from @p val into the corresponding positions of @p row.
     *
     * Columns are decoded in declaration order, skipping primary-key positions.
     * Returns @ref db_error::trailing_garbage if bytes remain after all value columns are decoded.
     *
     * @param schema Provides column types and primary-key membership.
     * @param row    Destination row (modified in-place); size must equal `schema.cols_.size()`.
     * @param val    Raw value bytes as stored in the @ref KeyValue layer.
     * @return Empty error code on success; a @ref db_error otherwise.
     */
    static std::error_code decode_val(const Schema &schema, Row &row, std::span<const std::byte> val);
};
