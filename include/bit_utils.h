#pragma once

#include <bit>          // std::bit_cast, std::has_unique_object_representations_v
#include <concepts>     // std::integral
#include <array>        // std::array
#include <cstddef>      // std::byte
#include <algorithm>    // std::ranges::reverse

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
    auto val = std::bit_cast<T>(
        std::bit_cast<std::array<std::byte, sizeof(T)>>(buf)
    );
    if constexpr (std::endian::native != std::endian::little)
        val = byteswap(val);
    return val;
}