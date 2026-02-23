#pragma once

#include "types.h"
#include <span>
#include <system_error>
#include <concepts>

template<typename T>
concept Reader = requires(T r, std::span<std::byte> buf, size_t &n) {
    { r.read(buf, n) } -> std::same_as<std::error_code>;
};