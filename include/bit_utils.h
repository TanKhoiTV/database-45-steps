#pragma once

#include <bit>          // std::bit_cast, std::has_unique_object_representations_v
#include <concepts>     // std::integral
#include <array>        // std::array
#include <cstddef>      // std::byte
#include <algorithm>    // std::ranges::reverse
#include <cstdint>      // uint32_t

template <std::integral T>
constexpr T byteswap(T value) noexcept {
    static_assert(std::has_unique_object_representations_v<T>, "T may not have padding bits");
    auto value_representation = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    std::ranges::reverse(value_representation);
    return std::bit_cast<T>(value_representation);
}

template <std::integral T>
std::array<std::byte, sizeof(T)> pack_le(T val) {
    if constexpr (std::endian::native != std::endian::little)
        val = byteswap(val);
    return std::bit_cast<std::array<std::byte, sizeof(T)>>(val);
}

template <std::integral T>
T unpack_le(std::span<const std::byte, sizeof(T)> buf) {
    std::array<std::byte, sizeof(T)> arr;
    std::copy(buf.begin(), buf.end(), arr.begin());
    auto val = std::bit_cast<T>(arr);
    if constexpr (std::endian::native != std::endian::little)
        val = byteswap(val);
    return val;
}

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