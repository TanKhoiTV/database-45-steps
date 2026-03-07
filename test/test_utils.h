// test/test_utils.h
#pragma once

/**
 * @file test_utils.h
 * @brief Shared test helpers: an in-memory @ref Reader and a hex file dumper.
 */

#include "core/types.h"     // bytes
#include "core/reader.h"    // Reader concept
#include <span>             // std::span
#include <cstddef>          // std::byte
#include <algorithm>        // std::copy_n, std::min
#include <system_error>     // std::error_code
#include <iomanip>          // std::hex, std::setw, std::setfill
#include <iostream>         // std::cerr
#include <fstream>          // std::ifstream, std::istreambuf_iterator
#include <filesystem>       // std::filesystem::file_size

/**
 * @brief In-memory @ref Reader backed by a byte span.
 *
 * Wraps a `std::span<const std::byte>` and advances an internal position
 * cursor on each `read` call, making it suitable for feeding pre-built
 * byte buffers into codec functions that expect a @ref Reader.
 *
 * EOF is signalled by setting `bytes_read = 0` (no error returned) when
 * the cursor has reached the end of the span.
 */
struct BufferReader {
    std::span<const std::byte> src;  ///< The underlying byte source.
    size_t pos = 0;

    /**
     * @brief Copies up to `buf.size()` bytes into @p buf and advances @ref pos.
     * @param buf        Destination span.
     * @param bytes_read Set to the number of bytes actually copied (0 on EOF).
     * @return Always returns an empty (success) error code.
     */
    std::error_code read(std::span<std::byte> buf, size_t &bytes_read) {
        size_t available = src.size() - pos;
        bytes_read = std::min(buf.size(), available);
        std::copy_n(src.data() + pos, bytes_read, buf.data());
        pos += bytes_read;
        return {};
    }
};

static_assert(Reader<BufferReader>, "BufferReader must satisfy the Reader concept");

/**
 * @brief Dumps the raw hex content of a file to `std::cerr`.
 *
 * Intended for diagnosing binary file corruption in failing tests.
 * Output format: one space-separated two-digit hex byte per byte, preceded
 * by a header line showing the path and total file size.
 *
 * @param path Filesystem path of the file to dump.
 */
inline void dump_file(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    std::cerr << "File: " << path << " ("
              << std::filesystem::file_size(path) << " bytes)\n";
    int i = 0;
    for (std::istreambuf_iterator<char> it(f), end; it != end; ++it, ++i)
        std::cerr << std::hex << std::setw(2) << std::setfill('0')
                  << (static_cast<unsigned char>(*it) & 0xFF) << " ";
    std::cerr << std::dec << "\n";
}
