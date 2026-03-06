#pragma once

#include <vector>   // std::vector
#include <cstddef>  // std::byte
#include <span>     // std::span
#include <string_view>

/**
 * @brief Contiguous buffer of raw bytes.
 * Used as the primary data type for keys and values throughout the database.
 */
using bytes = std::vector<std::byte>;

inline bytes to_bytes(std::span<const std::byte> data) {
    return bytes(data.begin(), data.end());
}

inline bytes to_bytes(std::string_view sv) {
    auto casted_pointer = reinterpret_cast<const std::byte *>(sv.data());
    bytes b(casted_pointer, casted_pointer + sv.size());
    return b;
}
