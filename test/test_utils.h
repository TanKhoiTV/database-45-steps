#pragma once

#include "reader.h"
#include "types.h"
#include <span>
#include <cstddef>
#include <algorithm>
#include <system_error>

struct BufferReader {
    std::span<const std::byte> src;
    size_t pos = 0;

    std::error_code read(std::span<std::byte> buf, size_t &bytes_read) {
        size_t available = src.size() - pos;
        bytes_read = std::min(buf.size(), available);
        std::copy_n(src.data() + pos, bytes_read, buf.data());
        pos += bytes_read;
        return {};
    }
};

static_assert(Reader<BufferReader>, "BufferReader must satisfy the Reader concept");