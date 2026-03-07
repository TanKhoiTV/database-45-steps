// include/core/types.h
#pragma once

#include <vector>       // std::vector
#include <cstddef>      // std::byte
#include <span>         // std::span
#include <string_view>  // std::string_view

/**
 * @file types.h
 * @brief Fundamental type aliases used throughout the database engine.
 */

/** @brief Contiguous buffer of raw bytes.
 *  Used as the primary data container for keys and values throughout the database. */
using bytes = std::vector<std::byte>;

/**
 * @brief Copies a byte span into an owned @ref bytes buffer.
 * @param data View over the source bytes.
 * @return A new @ref bytes containing a copy of @p data.
 */
inline bytes to_bytes(std::span<const std::byte> data) {
    return bytes(data.begin(), data.end());
}

/**
 * @brief Reinterprets a string view as a @ref bytes buffer.
 * @param sv The string view to convert.
 * @return A new @ref bytes whose contents are the UTF-8 code units of @p sv.
 */
inline bytes to_bytes(std::string_view sv) {
    auto casted_pointer = reinterpret_cast<const std::byte *>(sv.data());
    bytes b(casted_pointer, casted_pointer + sv.size());
    return b;
}
