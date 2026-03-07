// include/table/table.h
#pragma once

/**
 * @file table.h
 * @brief High-level relational table built on top of the @ref KeyValue store.
 */

#include "kv/kv.h"                  // KeyValue
#include "table/row.h"              // Row
#include "table/row_codec.h"        // RowCodec
#include "table/schema.h"           // Schema
#include "table/schema_codec.h"     // SchemaCodec
#include <system_error>             // std::error_code
#include <string>                   // std::string
#include <expected>                 // std::expected

/**
 * @brief A named, schema-typed table that stores @ref Row objects in a @ref KeyValue store.
 *
 * `Table` provides the familiar CRUD operations (Select, Insert, Update, Upsert, Delete)
 * on top of the binary KV layer.  Each row is encoded by @ref RowCodec into a
 * primary-key-derived KV key and a value containing the remaining columns.
 *
 * Instances are obtained exclusively through the static factory methods:
 * - @ref open            — look up an existing table by name.
 * - @ref create          — register a brand-new table; fails if it already exists.
 * - @ref open_or_create  — open if present, create otherwise.
 *
 * @note `Table` holds a non-owning reference to the @ref KeyValue store.
 *       The store must outlive any `Table` objects that reference it.
 */
class Table {
    KeyValue &kv_;
    Schema    schema_;

    /** @brief Private constructor; use the static factory methods instead. */
    Table(KeyValue &kv, Schema schema) : kv_(kv), schema_(std::move(schema)) {}

public:
    /**
     * @brief Opens an existing table by name.
     * @param kv   The backing key-value store.
     * @param name The table name to look up.
     * @return A `Table` on success; @ref db_error::table_not_found if the
     *         table does not exist, or another error on I/O failure.
     */
    static std::expected<Table, std::error_code> open(KeyValue &kv, const std::string &name);

    /**
     * @brief Creates and registers a new table.
     * @param kv     The backing key-value store.
     * @param schema Fully populated schema (name, columns, primary key).
     *               The numeric `id_` is assigned by the store's counter.
     * @return A `Table` on success; @ref db_error::table_already_exists if
     *         a table with the same name already exists, or another error on I/O failure.
     */
    static std::expected<Table, std::error_code> create(KeyValue &kv, Schema schema);

    /**
     * @brief Opens the table if it exists, or creates it otherwise.
     * @param kv     The backing key-value store.
     * @param schema Schema used for creation if the table is new; the stored
     *               schema is used if the table already exists.
     * @return A `Table` on success; an error on I/O failure.
     */
    static std::expected<Table, std::error_code> open_or_create(KeyValue &kv, Schema schema);

    /**
     * @brief Looks up the row whose primary-key cells match those set in @p row.
     *
     * Non-key cells in @p row are ignored on input and populated on success.
     *
     * @param[in,out] row On entry: primary-key cells are set.
     *                    On success: all cells are populated from the store.
     * @return `true` if the row was found and @p row was populated; `false` if
     *         no matching row exists; or an error on I/O failure.
     */
    std::expected<bool, std::error_code> Select(Row &row) const;

    /**
     * @brief Inserts @p row as a new entry; fails if the primary key already exists.
     * @param row Fully populated row.
     * @return `true` if the row was inserted; `false` if the key already existed;
     *         or an error on I/O failure.
     */
    std::expected<bool, std::error_code> Insert(const Row &row);

    /**
     * @brief Updates @p row; fails if the primary key does not already exist.
     * @param row Fully populated row with the new values.
     * @return `true` if the row was updated; `false` if the key was not found;
     *         or an error on I/O failure.
     */
    std::expected<bool, std::error_code> Update(const Row &row);

    /**
     * @brief Inserts or updates @p row unconditionally.
     * @param row Fully populated row.
     * @return `true` if the stored value changed; `false` if it was already
     *         identical; or an error on I/O failure.
     */
    std::expected<bool, std::error_code> Upsert(const Row &row);

    /**
     * @brief Removes the row whose primary key matches @p row.
     * @param row Only primary-key cells need to be populated.
     * @return `true` if the row existed and was deleted; `false` if it was
     *         not found; or an error on I/O failure.
     */
    std::expected<bool, std::error_code> Delete(const Row &row);

    /** @return Const reference to the table's schema. */
    const Schema &schema() const noexcept { return schema_; }

    /**
     * @brief Allocates a blank @ref Row sized for this table's schema.
     * @return A `Row` of `schema().cols_.size()` NULL cells.
     */
    Row new_row() const { return RowCodec::new_row(schema_); }
};
