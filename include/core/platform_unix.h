// include/core/platform_unix.h
#pragma once

/**
 * @file platform_unix.h
 * @brief POSIX file handle wrapping a raw file descriptor.
 */

#include "core/reader.h"
#include <span>         // std::span
#include <system_error> // std::error_code
#include <cstddef>      // std::byte
#include <string>       // std::string (forward-used by friend declarations)

/**
 * @brief Owns a POSIX file descriptor and exposes the platform I/O interface.
 *
 * Non-copyable (a duplicated fd would cause a double-close); movable so that
 * ownership can be transferred, e.g. when returning from factory functions.
 *
 * Satisfies the @ref Reader concept via its `read` member.
 */
class FileHandle {
    int  fd_     = -1;
    bool at_eof_ = false;

    void swap(FileHandle &other) noexcept;

public:
    FileHandle() = default;

    /** @brief Closes the descriptor silently; prefer @ref platform_close for error handling. */
    ~FileHandle();

    /** @brief Deleted – two handles sharing one fd would double-close on destruction. */
    FileHandle(const FileHandle &) = delete;
    /** @brief Deleted – see copy constructor. */
    FileHandle &operator=(const FileHandle &) = delete;

    /** @brief Transfers ownership from @p other; leaves @p other unopened. */
    FileHandle(FileHandle &&other) noexcept;
    /** @brief Move-assigns by swapping through a temporary; safely closes any existing fd. */
    FileHandle &operator=(FileHandle &&other) noexcept;

    /** @return `true` if the handle owns a valid file descriptor. */
    bool is_open() const noexcept { return fd_ >= 0; }
    /** @return `true` if the last read returned zero bytes (EOF). */
    bool at_eof()  const noexcept { return at_eof_; }

    /**
     * @brief Reads up to `buf.size()` bytes; delegates to @ref platform_read.
     * @param buf        Destination span.
     * @param bytes_read Set to the number of bytes actually transferred.
     * @return Empty error code on success or EOF; OS error otherwise.
     */
    std::error_code read(std::span<std::byte> buf, size_t &bytes_read) {
        return platform_read(*this, buf, bytes_read);
    }

private:
    friend std::error_code platform_open_file(const std::string &, FileHandle &);
    friend std::error_code platform_write    (FileHandle &, std::span<const std::byte>);
    friend std::error_code platform_read     (FileHandle &, std::span<std::byte>, size_t &);
    friend std::error_code platform_seek     (FileHandle &, long, int);
    friend std::error_code platform_sync     (FileHandle &);
    friend std::error_code platform_close    (FileHandle &);
};

static_assert(Reader<FileHandle>, "FileHandle must satisfy the Reader concept");
