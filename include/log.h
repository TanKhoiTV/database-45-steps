#pragma once

#include "platform.h"
#include "entry_codec.h"
#include <string>
#include <system_error>

struct LogEOF {};

using ReadResult = std::expected<std::variant<Entry, LogEOF>, std::error_code>;

/**
 * @brief Simple file-backed append/read log for encoded Entry objects
 *
 * Manages a file stream used to append encoded entries and decode entries from the file.
 *
 * @note This class is not thread-safe. Callers must synchronize access if the same Log instance is used concurrently.
 */
class Log {
    private:

    std::string filename;
    FileHandle fh;

    public:

    /**
     * @brief Construct a new Log object
     *
     * @param fname Path to the log file. The path is stored by value.
     *
     * The constructor does not open the file; call `open()` to create/open the underlying file stream.
     */
    explicit Log(std::string fname) : filename(std::move(fname)) {}

    Log(Log &&) = default;
    Log &operator=(Log &&) = default;


    /**
     * @brief open the log file for appending and reading.
     *
     * Perform these checks/steps:
     *
     * - Returns an error if @p filename exists and is a directory.
     *
     * - Attempts to open the file with: in | out | binary | app; creates the file if it doesn't exist.
     *
     * - On failure maps common @c errno values to `std::errc`-derived errors.
     *
     * - Clears any EOF flags on success so subsequent streaming operations are usable.
     *
     * @return error Default-constructed (no error) on success; otherwise an error code describing the failure.
     */
    std::error_code open();

    /**
     * @brief close the underlying file stream.
     *
     * @return error No error on success; `io_error` otherwise.
     */
    std::error_code close();

    /**
     * @brief Append an encoded Entry to the log file
     *
     * @param ent Entry to encode and write
     * @return error
     * @note The method assumes the file stream is opened.
     * Behavior is undefined or an I/O error is triggered if called on a closed stream.
     */
    std::error_code write(const Entry &ent);

    /**
     * @brief Read and decode the next entry from the file stream.
     *
     * @param[out] ent Entry object to be populated with decoded data.
     * @return `std::pair<bool, error>` The boolean signifies EOF in which case no error is returned.
     */
    ReadResult read();

    std::error_code seek_to_first_entry();

    bool is_open() const noexcept { return fh.is_open(); }

    ~Log();
};
