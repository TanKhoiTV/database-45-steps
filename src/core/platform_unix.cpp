// src/core/platform_unix.cpp

/**
 * @file platform_unix.cpp
 * @brief POSIX implementation of the platform file I/O interface.
 *
 * All syscall errors are converted to `std::error_code` values via
 * @ref errno_to_error so callers never need to inspect `errno` directly.
 */

#include "core/platform_unix.h"
#include <fcntl.h>   // ::open, O_RDWR, O_CREAT, O_RDONLY, O_DIRECTORY
#include <unistd.h>  // ::read, ::write, ::close, ::lseek, ::fsync
#include <cerrno>    // errno

// ---- FileHandle ----

FileHandle::~FileHandle() {
    if (fd_ >= 0) ::close(fd_);
}

void FileHandle::swap(FileHandle &other) noexcept {
    std::swap(fd_, other.fd_);
    std::swap(at_eof_, other.at_eof_);
}

FileHandle::FileHandle(FileHandle &&other) noexcept : fd_(-1), at_eof_(false) {
    this->swap(other);
}

FileHandle &FileHandle::operator=(FileHandle &&other) noexcept {
    FileHandle temp(std::move(other));
    this->swap(temp);
    return *this;
}

// ---- Helpers ----

/**
 * @brief Converts the current value of `errno` to an `std::error_code`.
 * @return An `std::error_code` in the generic category matching `errno`.
 */
inline std::error_code errno_to_error() {
    return std::make_error_code(static_cast<std::errc>(errno));
}

// ---- Platform functions ----

/**
 * @brief Opens or creates the file at @p path with `O_RDWR | O_CREAT 0644`.
 *
 * Also opens the parent directory and calls `fsync` on it so the new
 * directory entry is durable even on power loss.
 */
std::error_code platform_open_file(const std::string &path, FileHandle &out) {
    int fd = ::open(path.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0) return errno_to_error();

    out.fd_ = fd;

    std::string dir = path.substr(0, path.find_last_of('/'));
    if (dir.empty()) dir = ".";

    int dirfd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY);
    if (dirfd >= 0) {
        ::fsync(dirfd);
        ::close(dirfd);
    }

    return {};
}

/** @brief Writes @p buf in full; returns an error if fewer bytes are written. */
std::error_code platform_write(FileHandle &fh, std::span<const std::byte> buf) {
    ssize_t written = ::write(fh.fd_, buf.data(), buf.size_bytes());
    if (written < 0 || static_cast<size_t>(written) != buf.size_bytes())
        return errno_to_error();
    return {};
}

/** @brief Reads up to `buf.size()` bytes; sets `at_eof_` when the syscall returns 0. */
std::error_code platform_read(FileHandle &fh, std::span<std::byte> buf, size_t &bytes_read) {
    ssize_t n = ::read(fh.fd_, buf.data(), buf.size_bytes());
    if (n < 0) return errno_to_error();
    bytes_read = static_cast<size_t>(n);
    fh.at_eof_ = (n == 0);
    return {};
}

/** @brief Seeks to the given position; clears `at_eof_` on success. */
std::error_code platform_seek(FileHandle &fh, long offset, int whence) {
    if (::lseek(fh.fd_, offset, whence) < 0)
        return errno_to_error();
    fh.at_eof_ = false;
    return {};
}

/** @brief Flushes kernel and disk write-back caches via `fsync(2)`. */
std::error_code platform_sync(FileHandle &fh) {
    if (::fsync(fh.fd_) < 0) return errno_to_error();
    return {};
}

/** @brief Closes the fd and resets it to -1; no-op if already closed. */
std::error_code platform_close(FileHandle &fh) {
    if (fh.fd_ < 0) return {};
    int rc = ::close(fh.fd_);
    fh.fd_ = -1;
    return rc < 0 ? errno_to_error() : std::error_code{};
}
