// include/core/platform_windows.h
#pragma once

/**
 * @file platform_windows.h
 * @brief Win32 file handle wrapping a Windows `HANDLE`.
 */

#include "core/reader.h"
#include <windows.h>    // HANDLE, INVALID_HANDLE_VALUE
#include <span>         // std::span
#include <system_error> // std::error_code
#include <cstddef>      // std::byte
#include <string>       // std::string (forward-used by friend declarations)

/**
 * @brief Owns a Win32 file `HANDLE` and exposes the platform I/O interface.
 *
 * Non-copyable (duplicating a `HANDLE` without `DuplicateHandle` is unsafe);
 * movable so that ownership can be transferred.
 *
 * Satisfies the @ref Reader concept via its `read` member.
 */
class FileHandle {
    HANDLE h_     = INVALID_HANDLE_VALUE;
    bool   at_eof_ = false;

public:
    void swap(FileHandle &other) noexcept;

    FileHandle() = default;

    /** @brief Closes the handle silently; prefer @ref platform_close for error handling. */
    ~FileHandle();

    /** @brief Deleted – duplicating a raw HANDLE without DuplicateHandle is unsafe. */
    FileHandle(const FileHandle &) = delete;
    /** @brief Deleted – see copy constructor. */
    FileHandle &operator=(const FileHandle &) = delete;

    /** @brief Transfers ownership from @p other; leaves @p other with an invalid handle. */
    FileHandle(FileHandle &&other) noexcept;
    /** @brief Move-assigns by swapping through a temporary; safely closes any existing handle. */
    FileHandle &operator=(FileHandle &&other) noexcept;

    /** @return `true` if the handle is valid (not `INVALID_HANDLE_VALUE`). */
    bool is_open() const noexcept { return h_ != INVALID_HANDLE_VALUE; }
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

/**
 * @brief Non-member swap for ADL-based swap idiom.
 * @param a First handle.
 * @param b Second handle.
 */
inline void swap(FileHandle &a, FileHandle &b) noexcept { a.swap(b); }

static_assert(Reader<FileHandle>, "FileHandle must satisfy the Reader concept");
