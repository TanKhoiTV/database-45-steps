// entry.h
#pragma once

#include "core/types.h"

/**
 * @brief Represents a single database entry for serialization
 *
 */
struct Entry {
    bytes key_;
    bytes val_;
    bool deleted_;

    Entry() = default;
    Entry(bytes key, bytes val, bool deleted)
        : key_(std::move(key)), val_(std::move(val)), deleted_(deleted) {}

    bool operator==(const Entry &other) const noexcept {
        return key_ == other.key_ && val_ == other.val_ && deleted_ == other.deleted_;
    }
};
