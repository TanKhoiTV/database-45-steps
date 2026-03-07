// src/core/platform_windows.cpp

/**
 * @file platform_windows.cpp
 * @brief Win32 implementation of the platform file I/O interface.
 *
 * Win32 `HANDLE` errors are retrieved via `GetLastError()` and wrapped in an
 * `std::error_code` (system category) by @ref last_win32_error.
 * NTFS guarantees directory-entry durability, so no directory `fsync` is needed.
 */

#include "core/platform_windows.h"
#include <windows.h> // CreateFileW, ReadFile, WriteFile, …
#include <string>    // std::string, std::wstring

// ---- FileHandle ----

FileHandle::~FileHandle() {
    if (h_ != INVALID_HANDLE_VALUE) CloseHandle(h_);
}

void FileHandle::swap(FileHandle &other) noexcept {
    std::swap(h_, other.h_);
    std::swap(at_eof_, other.at_eof_);
}

FileHandle::FileHandle(FileHandle &&other) noexcept : h_(INVALID_HANDLE_VALUE), at_eof_(false) {
    this->swap(other);
}

FileHandle &FileHandle::operator=(FileHandle &&other) noexcept {
    FileHandle temp(std::move(other));
    this->swap(temp);
    return *this;
}

// --- Helpers ---

/**
 * @brief Wraps the current `GetLastError()` value in an `std::error_code`.
 * @return An `std::error_code` in the system category.
 */
static std::error_code last_win32_error() {
    return std::error_code(
        static_cast<int>(GetLastError()),
        std::system_category()
    );
}

/**
 * @brief Converts a UTF-8 `std::string` to a `std::wstring` for Win32 wide APIs.
 * @param path UTF-8 encoded path.
 * @return Wide-character equivalent, or an empty string on conversion failure.
 */
static std::wstring to_wide(const std::string &path) {
    if (path.empty()) return {};

    int size = MultiByteToWideChar(
        CP_UTF8, 0,
        path.c_str(), static_cast<int>(path.size()),
        nullptr, 0
    );
    if (size <= 0) return {};

    std::wstring wide(size, L'\0');

    MultiByteToWideChar(
        CP_UTF8, 0,
        path.c_str(), static_cast<int>(path.size()),
        wide.data(), size
    );
    return wide;
}

// ---- Platform functions ----

/**
 * @brief Opens or creates the file at @p path via `CreateFileW` with
 *        `GENERIC_READ | GENERIC_WRITE` and `OPEN_ALWAYS` disposition.
 */
std::error_code platform_open_file(const std::string &path, FileHandle &out) {
    HANDLE h = CreateFileW(
        to_wide(path).c_str(),
        GENERIC_READ | GENERIC_WRITE,   // O_RDWR equivalent
        FILE_SHARE_READ,                // allow concurrent readers
        nullptr,                        // default security attributes
        OPEN_ALWAYS,                    // create if not exists, open if it does
        FILE_ATTRIBUTE_NORMAL,          // no special attributes
        nullptr                         // no template file
    );

    if (h == INVALID_HANDLE_VALUE)
        return last_win32_error();

    out.h_ = h;

    // No fsync, NTFS guarantees directory durability

    return {};
}

/** @brief Writes @p buf in full via `WriteFile`; returns an error on short write. */
std::error_code platform_write(FileHandle &fh, std::span<const std::byte> buf) {
    DWORD written = 0;
    if (!WriteFile(fh.h_, buf.data(), static_cast<DWORD>(buf.size_bytes()), &written, nullptr))
        return last_win32_error();

    if (written != static_cast<DWORD>(buf.size_bytes()))
        return std::make_error_code(std::errc::io_error);

    return {};
}

/** @brief Reads up to `buf.size()` bytes via `ReadFile`; sets `at_eof_` when 0 bytes return. */
std::error_code platform_read(FileHandle &fh, std::span<std::byte> buf, size_t &bytes_read) {
    DWORD n = 0;
    if (!ReadFile(fh.h_, buf.data(), static_cast<DWORD>(buf.size_bytes()), &n, nullptr))
        return last_win32_error();

    bytes_read = static_cast<size_t>(n);
    fh.at_eof_ = (n == 0);
    return {};
}

/**
 * @brief Seeks via `SetFilePointer`; maps POSIX @p whence constants to Win32 move methods.
 * @param whence `SEEK_SET`, `SEEK_CUR`, or `SEEK_END`.
 */
std::error_code platform_seek(FileHandle &fh, long offset, int whence) {
    DWORD method;
    switch (whence) {
        case SEEK_SET: method = FILE_BEGIN;     break;
        case SEEK_CUR: method = FILE_CURRENT;   break;
        case SEEK_END: method = FILE_END;       break;
        default: return std::make_error_code(std::errc::invalid_argument);
    }

    DWORD result = SetFilePointer(fh.h_, offset, nullptr, method);
    if (result == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
        return last_win32_error();

    fh.at_eof_ = false;
    return {};
}

/** @brief Flushes OS write-back caches via `FlushFileBuffers`. */
std::error_code platform_sync(FileHandle &fh) {
    if (!FlushFileBuffers(fh.h_))
        return last_win32_error();
    return {};
}

/** @brief Closes the handle and resets it to `INVALID_HANDLE_VALUE`; no-op if already closed. */
std::error_code platform_close(FileHandle &fh) {
    if (!fh.is_open()) return {};
    BOOL ok = CloseHandle(fh.h_);
    fh.h_ = INVALID_HANDLE_VALUE;
    return ok ? std::error_code{} : last_win32_error();
}
