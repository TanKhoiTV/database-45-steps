#pragma once

#include <system_error>     // std::error_code
#include <string>           // std::string
#include <span>             // std::span
#include "reader.h"

#if defined(_WIN32)
    #include "platform_windows.h"
#else
    #include "platform_unix.h"
#endif


std::error_code platform_open_file(const std::string &path, FileHandle &out);
std::error_code platform_write(FileHandle &fh, std::span<const std::byte> buf);
std::error_code platform_read(FileHandle &fh, std::span<std::byte> buf, size_t &bytes_read);
std::error_code platform_seek(FileHandle &fh, long offset, int whence);
std::error_code platform_sync(FileHandle &fh);
std::error_code platform_close(FileHandle &fh);
