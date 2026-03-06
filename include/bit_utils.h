#pragma once

#include <bit>          // std::bit_cast, std::has_unique_object_representations_v
#include <concepts>     // std::integral
#include <array>        // std::array
#include <cstddef>      // std::byte
#include <algorithm>    // std::ranges::reverse
#include <cstdint>      // uint32_t
#include <optional>
#include <string>
#include <string_view>

template <std::integral T>
std::array<std::byte, sizeof(T)> pack_le(T val) {
    if constexpr (std::endian::native != std::endian::little)
        val = std::byteswap(val);
    return std::bit_cast<std::array<std::byte, sizeof(T)>>(val);
}

template <std::integral T>
T unpack_le(std::span<const std::byte, sizeof(T)> buf) {
    auto val = std::bit_cast<T>(*reinterpret_cast<const std::array<std::byte, sizeof(T)> *>(buf.data()));
    if constexpr (std::endian::native != std::endian::little)
        val = std::byteswap(val);
    return val;
}

inline void push_u32(bytes &out, uint32_t v) {
    auto b = pack_le<uint32_t>(v);
    out.insert(out.end(), b.begin(), b.end());
}

inline void push_str(bytes &out, std::string_view s) {
    push_u32(out, static_cast<uint32_t>(s.size()));
    auto p = reinterpret_cast<const std::byte *>(s.data());
    out.insert(out.end(), p, p + s.size());
}

inline std::optional<uint32_t> read_u32(std::span<const std::byte> &buf) {
    if (buf.size() < sizeof(uint32_t)) return std::nullopt;
    auto v = unpack_le<uint32_t>(buf.first<sizeof(uint32_t)>());
    buf = buf.subspan<sizeof(uint32_t)>();
    return v;
}

inline std::optional<std::string> read_str(std::span<const std::byte> &buf) {
    auto len = read_u32(buf);
    if (!len || buf.size() < *len) return std::nullopt;
    std::string s(reinterpret_cast<const char *>(buf.data()), *len);
    buf = buf.subspan(*len);
    return s;
}

// --- CRC32 Hashing utility ---

inline constexpr uint32_t crc32_init() {
    return 0xFFFFFFFFu;
}

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

inline constexpr uint32_t crc32_final(uint32_t crc) {
    return crc ^ 0xFFFFFFFFu;
}

inline uint32_t crc32_ieee(std::span<const std::byte> data) {
    return crc32_final(crc32_update(crc32_init(), data));
}
