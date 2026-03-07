// include/kv/log_format.h
#pragma once

/**
 * @file log_format.h
 * @brief Compile-time constants that define the on-disk log file header.
 *
 * Every valid kvdb log file begins with a 6-byte header:
 * ```
 * [ magic(4) | version(2) ]
 * ```
 * Both fields are little-endian.
 */

#include <cstdint>  // uint32_t, uint16_t
#include <cstddef>  // size_t

namespace log_format {

/**
 * @brief Four-byte file signature (`'K','V','D','B'` = `0x4B564442`).
 *
 * Any file that does not start with this value is not a valid kvdb log
 * and will be rejected with @ref db_error::bad_magic.
 */
inline constexpr uint32_t MAGIC = 0x4B564442;

/**
 * @brief Monotonically increasing format revision number.
 *
 * Must be incremented whenever the Entry wire format changes in a
 * backward-incompatible way.  Files whose stored version exceeds this
 * constant are rejected with @ref db_error::unsupported_version.
 */
inline constexpr uint16_t FORMAT_VERSION = 2;

/** @brief Size of the file header in bytes (`sizeof(magic) + sizeof(version)`). */
inline constexpr size_t HEADER_SIZE = 6;

} // namespace log_format
