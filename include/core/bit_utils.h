// include/core/bit_utils.h
#pragma once

/**
 * @file bit_utils.h
 * @brief Low-level serialisation helpers: little-endian integer packing,
 *        length-prefixed string encoding, and IEEE 802.3 CRC-32 hashing.
 */

#include <bit>          // std::bit_cast, std::endian, std::byteswap
#include <concepts>     // std::integral
#include <array>        // std::array
#include <cstddef>      // std::byte
#include <cstdint>      // uint8_t, uint32_t
#include <optional>     // std::optional
#include <string>       // std::string
#include <string_view>  // std::string_view
#include <span>         // std::span
#include "core/types.h" // bytes

// ---- Little-endian integer packing ----

/**
 * @brief Serialises an integral value to a fixed-size little-endian byte array.
 * @tparam T Any integral type.
 * @param val The value to serialise.
 * @return `std::array<std::byte, sizeof(T)>` in little-endian order.
 */
template <std::integral T>
std::array<std::byte, sizeof(T)> pack_le(T val) {
    if constexpr (std::endian::native != std::endian::little)
        val = std::byteswap(val);
    return std::bit_cast<std::array<std::byte, sizeof(T)>>(val);
}

/**
 * @brief Deserialises a little-endian byte span into an integral value.
 * @tparam T Any integral type.
 * @param buf A fixed-size span of exactly `sizeof(T)` bytes.
 * @return The deserialised value in native byte order.
 */
template <std::integral T>
T unpack_le(std::span<const std::byte, sizeof(T)> buf) {
    auto val = std::bit_cast<T>(*reinterpret_cast<const std::array<std::byte, sizeof(T)> *>(buf.data()));
    if constexpr (std::endian::native != std::endian::little)
        val = std::byteswap(val);
    return val;
}

// ---- Byte buffer append helpers ----

/**
 * @brief Appends a `uint32_t` in little-endian encoding to @p out.
 * @param out  Destination buffer; bytes are appended in-place.
 * @param v    The value to append.
 */
inline void push_u32(bytes &out, uint32_t v) {
    auto b = pack_le<uint32_t>(v);
    out.insert(out.end(), b.begin(), b.end());
}

/**
 * @brief Appends a length-prefixed string to @p out.
 *
 * Layout: `[uint32_t length][char... data]`, length encoded little-endian.
 *
 * @param out Destination buffer; bytes are appended in-place.
 * @param s   The string to append.
 */
inline void push_str(bytes &out, std::string_view s) {
    push_u32(out, static_cast<uint32_t>(s.size()));
    auto p = reinterpret_cast<const std::byte *>(s.data());
    out.insert(out.end(), p, p + s.size());
}

// ---- Byte buffer read helpers ----

/**
 * @brief Reads a little-endian `uint32_t` from the front of @p buf and advances it.
 * @param buf  In/out span; shrunk by `sizeof(uint32_t)` on success.
 * @return The parsed value, or `std::nullopt` if fewer than 4 bytes remain.
 */
inline std::optional<uint32_t> read_u32(std::span<const std::byte> &buf) {
    if (buf.size() < sizeof(uint32_t)) return std::nullopt;
    auto v = unpack_le<uint32_t>(buf.first<sizeof(uint32_t)>());
    buf = buf.subspan<sizeof(uint32_t)>();
    return v;
}

/**
 * @brief Reads a length-prefixed string from the front of @p buf and advances it.
 *
 * Expects the layout written by @ref push_str.
 *
 * @param buf  In/out span; shrunk by `4 + length` bytes on success.
 * @return The parsed string, or `std::nullopt` if the buffer is too short.
 */
inline std::optional<std::string> read_str(std::span<const std::byte> &buf) {
    auto len = read_u32(buf);
    if (!len || buf.size() < *len) return std::nullopt;
    std::string s(reinterpret_cast<const char *>(buf.data()), *len);
    buf = buf.subspan(*len);
    return s;
}

// ---- CRC-32 (IEEE 802.3 polynomial) ----

/**
 * @brief Returns the initial CRC-32 accumulator value.
 * @return `0xFFFFFFFF`
 */
inline constexpr uint32_t crc32_init() {
    return 0xFFFFFFFFu;
}

/**
 * @brief Folds @p data into a running CRC-32 accumulator.
 *
 * Uses the reflected IEEE 802.3 polynomial `0xEDB88320`.
 * The lookup table is built once at first call via a `constexpr` lambda.
 *
 * @param crc  Running accumulator (start with @ref crc32_init()).
 * @param data Bytes to process.
 * @return Updated accumulator.
 */
inline uint32_t crc32_update(uint32_t crc, std::span<const std::byte> data) noexcept {
    static constexpr auto table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j)
                c = (c >> 1) ^ (c & 1 ? 0xEDB88320u : 0u);
            t[i] = c;
        }
        return t;
    }();

    for (std::byte b : data)
        crc = (crc >> 8) ^ table[static_cast<uint8_t>(b) ^ (crc & 0xFF)];
    return crc;
}

/**
 * @brief Finalises a CRC-32 accumulator by XOR-ing with `0xFFFFFFFF`.
 * @param crc The value returned by the last @ref crc32_update call.
 * @return The final CRC-32 digest.
 */
inline constexpr uint32_t crc32_final(uint32_t crc) {
    return crc ^ 0xFFFFFFFFu;
}

/**
 * @brief Computes the IEEE 802.3 CRC-32 of @p data in a single call.
 * @param data Bytes to hash.
 * @return 32-bit CRC digest.
 */
inline uint32_t crc32_ieee(std::span<const std::byte> data) {
    return crc32_final(crc32_update(crc32_init(), data));
}
