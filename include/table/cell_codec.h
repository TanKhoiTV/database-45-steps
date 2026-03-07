// include/table/cell_codec.h
#pragma once

/**
 * @file cell_codec.h
 * @brief Binary serialisation and deserialisation of individual @ref Cell values.
 *
 * Wire formats by type:
 * | Type           | Encoding                                  |
 * |----------------|-------------------------------------------|
 * | `no_type`      | single @ref null_byte sentinel            |
 * | `i64`          | 8 raw bytes, little-endian `int64_t`      |
 * | `str`          | `uint32_t` length (LE) followed by data   |
 *
 * @ref read_cell_type reads the 1-byte type tag that precedes a cell in
 * contexts where the type is not known from the schema (e.g. schema encoding).
 */

#include "table/cell.h"     // Cell
#include <expected>         // std::expected
#include <system_error>     // std::error_code
#include <optional>         // std::optional
#include <span>             // std::span

/**
 * @brief Stateless codec for @ref Cell values.
 *
 * All methods are static; the struct exists to group the codec logic with
 * its shared constant @ref null_byte.
 */
class CellCodec {
public:
    /**
     * @brief Sentinel byte written in place of a NULL (`no_type`) cell.
     *
     * The value `0x02` was chosen to avoid collision with `0x00` (used as
     * the key-prefix separator by @ref RowCodec) and with valid type-tag bytes.
     */
    static constexpr std::byte null_byte = static_cast<std::byte>(0x02);

    /**
     * @brief Appends the binary encoding of @p c to @p out.
     *
     * @param c        The cell to encode.
     * @param expected The schema type expected for this column position.
     * @param out      Destination buffer; bytes are appended in-place.
     * @return Empty error code on success; @ref db_error::type_mismatch if
     *         the cell's active type does not match @p expected;
     *         @ref db_error::unsupported_type for unhandled variant alternatives.
     */
    static std::error_code encode(const Cell &c, Cell::Type expected, bytes &out);

    /**
     * @brief Decodes one cell of type @p t from the front of @p buf and advances it.
     *
     * @param buf In/out span; shrunk by the number of bytes consumed on success.
     * @param t   The expected cell type (from the schema).
     * @return The decoded @ref Cell, or an `std::error_code` on failure
     *         (@ref db_error::expect_more_data if the buffer is too short).
     */
    static std::expected<Cell, std::error_code> decode(std::span<const std::byte> &buf, Cell::Type t);

    /**
     * @brief Reads and advances past the 1-byte type tag at the front of @p buf.
     *
     * Used by @ref SchemaCodec to recover the type of each column during
     * schema deserialisation.
     *
     * @param buf In/out span; shrunk by 1 on success.
     * @return The decoded @ref Cell::Type, or `std::nullopt` if the buffer is
     *         empty or the tag byte does not map to a known type.
     */
    static std::optional<Cell::Type> read_cell_type(std::span<const std::byte> &buf);
};
