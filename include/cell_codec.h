#pragma once

#include "cell.h"
#include <expected>
#include <system_error>

class CellCodec {
    public:

    static constexpr std::byte null_byte = static_cast<std::byte>(0x02);

    static std::error_code encode(const Cell &c, Cell::Type expected, bytes &out);

    static std::expected<Cell, std::error_code> decode(std::span<const std::byte> &buf, Cell::Type t);
};
