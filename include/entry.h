// entry.h
#pragma once

#include "types.h"

/**
 * @brief Represents a single database entry for serialization
 *
 */
struct Entry {
    bytes key;
    bytes val;
    bool deleted;

    Entry() = default;
    Entry(bytes _key, bytes _val, bool _deleted)
        : key(std::move(_key)), val(std::move(_val)), deleted(_deleted) {}

    bool operator==(const Entry &other) const noexcept {
        return key == other.key && val == other.val && deleted == other.deleted;
    }
};
