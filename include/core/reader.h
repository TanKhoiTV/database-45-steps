// include/core/reader.h
#pragma once

/**
 * @file reader.h
 * @brief Concept describing any type that can supply raw bytes on demand.
 */

#include <span>         // std::span
#include <system_error> // std::error_code
#include <concepts>     // requires

/**
 * @brief Models a sequential byte source.
 *
 * A type satisfies `Reader` when it exposes a `read` member that attempts to
 * fill @p buf, stores the number of bytes actually read in @p n, and returns
 * an `std::error_code` indicating success or the reason for failure.
 * On EOF the call succeeds (`error_code{}`) and sets `n` to zero.
 *
 * @tparam T The candidate type to test.
 */
template<typename T>
concept Reader = requires(T r, std::span<std::byte> buf, size_t &n) {
    { r.read(buf, n) } -> std::same_as<std::error_code>;
};
