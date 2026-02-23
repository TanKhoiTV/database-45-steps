#pragma once

#include "reader.h"
#include "types.h"
#include <span>
#include <cstddef>
#include <algorithm>
#include <system_error>
#include <iomanip>
#include <fstream>

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

// Debug helper
void dump_file(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    std::cerr << "File: " << path << " (" 
              << std::filesystem::file_size(path) << " bytes)\n";
    int i = 0;
    for (std::istreambuf_iterator<char> it(f), end; it != end; ++it, ++i)
        std::cerr << std::hex << std::setw(2) << std::setfill('0') 
                  << (static_cast<unsigned char>(*it) & 0xFF) << " ";
    std::cerr << std::dec << "\n";
}