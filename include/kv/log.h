// include/kv/log.h
#pragma once

/**
 * @file log.h
 * @brief Append-only, file-backed log of encoded @ref Entry records.
 */

#include "core/platform.h"
#include "kv/entry_codec.h"
#include <string>       // std::string
#include <system_error> // std::error_code

/** @brief Sentinel returned by @ref Log::read when the end of the log is reached. */
struct LogEOF {};

/**
 * @brief Result type of @ref Log::read.
 *
 * Contains either a decoded @ref Entry, a @ref LogEOF sentinel on clean
 * end-of-log, or an `std::error_code` on a hard read failure.
 */
using ReadResult = std::expected<std::variant<Entry, LogEOF>, std::error_code>;

/**
 * @brief Append-only, file-backed log of @ref Entry records.
 *
 * Manages a single file whose layout is:
 * ```
 * [ file_header(6) | entry ... | entry ... | ... ]
 * ```
 * New entries are always appended; existing entries are never mutated.
 * On open, the file header is validated (magic number + format version).
 * If the file does not yet exist it is created and the header is written.
 *
 * Tail corruption (bad checksum, truncated header/payload) is treated
 * silently as EOF on @ref read so that a crash mid-write does not
 * permanently poison the log.
 *
 * @note Not thread-safe. Callers must serialise concurrent access externally.
 */
class Log {
    std::string filename_;
    FileHandle  fh_;

public:
    /**
     * @brief Constructs a Log bound to the file at @p fname.
     *
     * Does not open the file; call @ref open before any I/O.
     *
     * @param fname Path to the log file (stored by value).
     */
    explicit Log(std::string fname) : filename_(std::move(fname)) {}

    Log(Log &&)            = default;
    Log &operator=(Log &&) = default;

    /**
     * @brief Opens (or creates) the log file and validates its header.
     *
     * If the file already exists its magic number and format version are
     * checked; a brand-new file gets a freshly written header.
     * Returns immediately without re-opening if the file is already open.
     *
     * Possible errors: `std::errc::is_a_directory`, @ref db_error::bad_magic,
     * @ref db_error::unsupported_version, @ref db_error::truncated_header,
     * or any OS-level I/O error.
     *
     * @return Empty error code on success; a descriptive error otherwise.
     */
    std::error_code open();

    /**
     * @brief Flushes and closes the underlying file handle.
     * @return Empty error code on success; `std::errc::io_error` otherwise.
     */
    std::error_code close();

    /**
     * @brief Encodes @p ent and appends it to the log.
     *
     * Seeks to EOF before writing so concurrent readers are not disturbed,
     * then calls @ref platform_sync to make the write durable.
     *
     * @param ent The entry to persist.
     * @return Empty error code on success; an I/O error otherwise.
     * @pre The log must be open; calling this on a closed log is undefined behaviour.
     */
    std::error_code write(const Entry &ent);

    /**
     * @brief Decodes and returns the next @ref Entry from the current file position.
     *
     * Tail corruption (@ref db_error::bad_checksum, @ref db_error::truncated_header,
     * @ref db_error::truncated_payload) is silently converted to @ref LogEOF so
     * that a crash-interrupted final write does not prevent the log from loading.
     *
     * @return A @ref ReadResult containing an @ref Entry, @ref LogEOF, or an error.
     */
    ReadResult read();

    /**
     * @brief Seeks the file position to the first entry (just past the file header).
     *
     * Call this before iterating over entries with @ref read.
     *
     * @return Empty error code on success; an I/O error otherwise.
     */
    std::error_code seek_to_first_entry();

    /** @return `true` if the underlying file handle is currently open. */
    bool is_open() const noexcept { return fh_.is_open(); }

    /** @brief Closes the file silently if still open; prefer @ref close for error handling. */
    ~Log();
};
