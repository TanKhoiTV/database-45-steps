// include/kv/entry.h
#pragma once

/**
 * @file entry.h
 * @brief Plain data type representing one logical database record.
 */

#include "core/types.h" // bytes

/**
 * @brief A single key-value record, optionally marked as a tombstone.
 *
 * `Entry` is the canonical unit of data flowing through the codec and log
 * layers.  When `deleted_` is `true` the entry acts as a tombstone: the
 * key is present but the value is meaningless and should be treated as empty.
 */
struct Entry {
    bytes key_;      ///< The record's binary key.
    bytes val_;      ///< The record's binary value; ignored when `deleted_` is `true`.
    bool  deleted_;  ///< `true` if this entry is a deletion tombstone.

    Entry() = default;

    /**
     * @brief Constructs an Entry, taking ownership of @p key and @p val.
     * @param key     Binary key.
     * @param val     Binary value; pass `{}` for tombstones.
     * @param deleted `true` to mark the entry as a deletion tombstone.
     */
    Entry(bytes key, bytes val, bool deleted)
        : key_(std::move(key)), val_(std::move(val)), deleted_(deleted) {}

    /**
     * @brief Equality comparison; all three fields must match.
     * @param other The entry to compare against.
     * @return `true` if key, value, and deleted flag are all equal.
     */
    bool operator==(const Entry &other) const noexcept {
        return key_ == other.key_ && val_ == other.val_ && deleted_ == other.deleted_;
    }
};
