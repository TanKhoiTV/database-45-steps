#pragma once

#include "reader.h"
#include <windows.h>
#include <span>
#include <system_error>
#include <cstddef>

class FileHandle {
    private:

    HANDLE h_ = INVALID_HANDLE_VALUE;
    bool at_eof_ = false;

    public:

    void swap(FileHandle &other) noexcept;

    FileHandle() = default;

    /** @note Closes silently, errors are unrecoverable; prefer explicit closing. */
    ~FileHandle();

    // No copying - two handles owning the same file descriptor would double-close
    FileHandle(const FileHandle &) = delete;
    FileHandle &operator=(const FileHandle &) = delete;

    // Move: transfer ownership, leave source in a safe empty state
    FileHandle(FileHandle &&other) noexcept;
    FileHandle &operator=(FileHandle &&other) noexcept;

    bool is_open()  const noexcept { return h_ != INVALID_HANDLE_VALUE; }
    bool at_eof()   const noexcept { return at_eof_; }

    std::error_code read(std::span<std::byte> buf, size_t &bytes_read) {
        return platform_read(*this, buf, bytes_read);
    }

    private:

    friend std::error_code platform_open_file(const std::string &path, FileHandle &out);
    friend std::error_code platform_write(FileHandle &fh, std::span<const std::byte> buf);
    friend std::error_code platform_read(FileHandle &fh, std::span<std::byte> buf, size_t &bytes_read);
    friend std::error_code platform_seek(FileHandle &fh, long offset, int whence);
    friend std::error_code platform_sync(FileHandle &fh);
    friend std::error_code platform_close(FileHandle &fh);
};

inline void swap(FileHandle &a, FileHandle &b) noexcept {
    a.swap(b);
}

static_assert(Reader<FileHandle>, "FileHandle must satisfy the Reader concept");
