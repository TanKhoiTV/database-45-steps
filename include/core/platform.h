// include/core/platform.h
#pragma once

/**
 * @file platform.h
 * @brief Platform-agnostic file I/O interface.
 *
 * Selects the correct platform header at compile time and exposes a uniform
 * set of free functions that wrap OS-specific file operations.
 * All functions return an `std::error_code`; an empty (default-constructed)
 * code signals success.
 */

#include <system_error> // std::error_code
#include <string>       // std::string
#include <span>         // std::span
#include "core/reader.h"

#if defined(_WIN32)
    #include "platform_windows.h"
#else
    #include "platform_unix.h"
#endif

/**
 * @brief Opens (or creates) the file at @p path for read/write access.
 * @param path Filesystem path to the file.
 * @param out  Receives the open handle on success.
 * @return Empty error code on success; OS error otherwise.
 */
std::error_code platform_open_file(const std::string &path, FileHandle &out);

/**
 * @brief Writes all bytes in @p buf to @p fh at the current file position.
 * @param fh  An open file handle.
 * @param buf Data to write; must be written in full.
 * @return Empty error code on success; OS error otherwise.
 */
std::error_code platform_write(FileHandle &fh, std::span<const std::byte> buf);

/**
 * @brief Reads up to `buf.size()` bytes from @p fh into @p buf.
 * @param fh         An open file handle.
 * @param buf        Destination span.
 * @param bytes_read Set to the number of bytes actually read (0 on EOF).
 * @return Empty error code on success or EOF; OS error otherwise.
 */
std::error_code platform_read(FileHandle &fh, std::span<std::byte> buf, size_t &bytes_read);

/**
 * @brief Repositions the file pointer of @p fh.
 * @param fh     An open file handle.
 * @param offset Byte offset relative to @p whence.
 * @param whence `SEEK_SET`, `SEEK_CUR`, or `SEEK_END`.
 * @return Empty error code on success; OS error otherwise.
 */
std::error_code platform_seek(FileHandle &fh, long offset, int whence);

/**
 * @brief Flushes OS and hardware write caches for @p fh (`fsync` / `FlushFileBuffers`).
 * @param fh An open file handle.
 * @return Empty error code on success; OS error otherwise.
 */
std::error_code platform_sync(FileHandle &fh);

/**
 * @brief Closes @p fh and releases the underlying OS resource.
 * @param fh The handle to close; left in a safe, unopened state.
 * @return Empty error code on success; OS error otherwise.
 */
std::error_code platform_close(FileHandle &fh);
