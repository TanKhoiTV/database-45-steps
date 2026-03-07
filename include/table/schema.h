// include/table/schema.h
#pragma once

/**
 * @file schema.h
 * @brief Table schema: column definitions and primary-key metadata.
 */

#include "table/cell.h"  // Cell::Type
#include <vector>        // std::vector
#include <string>        // std::string
#include <cstdint>       // uint32_t

/**
 * @brief Name and type descriptor for a single table column.
 */
struct ColumnHeader {
    std::string name_;  ///< Column name (UTF-8).
    Cell::Type  type_;  ///< Value type stored in this column.
};

/**
 * @brief Immutable description of a table's columns and primary key.
 *
 * The constructor calls @ref compute_metadata to derive @ref pkey_map_ from
 * @ref pkey_, so the two are always consistent after construction.
 *
 * @note `id_` is assigned externally (e.g. by a monotonic counter in the
 *       @ref KeyValue store) and must be unique across all tables in a database.
 */
struct Schema {
    uint32_t                 id_;       ///< Unique, stable numeric table identifier.
    std::string              name_;     ///< Human-readable table name (UTF-8).
    std::vector<ColumnHeader> cols_;   ///< Ordered column definitions.
    std::vector<size_t>      pkey_;    ///< Ordered column indices that form the primary key.
    std::vector<bool>        pkey_map_; ///< `pkey_map_[i]` is `true` iff column `i` is part of the primary key. Derived from `pkey_` by @ref compute_metadata.

    /**
     * @brief Constructs a Schema and derives @ref pkey_map_.
     * @param id   Unique numeric table ID.
     * @param name Human-readable table name.
     * @param cols Column definitions in declaration order.
     * @param pkey Ordered indices into @p cols that form the primary key.
     */
    Schema(uint32_t id, std::string name, std::vector<ColumnHeader> cols, std::vector<size_t> pkey)
        : id_(id), name_(std::move(name)), cols_(std::move(cols)), pkey_(std::move(pkey)) {
        compute_metadata();
    }

    /**
     * @brief Returns whether column @p col_idx is part of the primary key.
     * @param col_idx Zero-based column index.
     * @return `true` if @p col_idx is a primary-key column; `false` if out of
     *         range or not a key column.
     */
    bool is_pkey(size_t col_idx) const noexcept {
        if (col_idx >= pkey_map_.size()) return false;
        return pkey_map_[col_idx];
    }

private:
    /**
     * @brief Rebuilds @ref pkey_map_ from @ref pkey_.
     *
     * Called once by the constructor.  Out-of-range indices in @ref pkey_ are
     * silently ignored (they exceed `cols_.size()` and have no map entry).
     */
    void compute_metadata() {
        pkey_map_.assign(cols_.size(), false);
        for (auto idx : pkey_) {
            if (idx < pkey_map_.size()) {
                pkey_map_[idx] = true;
            }
        }
    }
};
