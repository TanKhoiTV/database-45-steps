// include/kv/kv.h
#pragma once

/**
 * @file kv.h
 * @brief Public interface of the @ref KeyValue store.
 */

#include "core/types.h"     // bytes, to_bytes
#include "kv/log.h"         // Log
#include <unordered_map>    // std::unordered_map
#include <expected>         // std::expected
#include <optional>         // std::optional
#include <system_error>     // std::error_code
#include <span>             // std::span

/**
 * @brief Persistent, log-structured key-value store with an in-memory index.
 *
 * `KeyValue` combines an append-only @ref Log with a hash-map index:
 * - **Writes** append an encoded @ref Entry to the log *and* update the index atomically
 *   (log first; a crash before the index update is recovered on next @ref open).
 * - **Reads** are served entirely from the in-memory index — no disk I/O.
 * - **Recovery** replays the full log on @ref open to rebuild the index.
 *
 * Binary keys and values of arbitrary content are supported.
 *
 * @note Not copyable (owns a @ref Log which owns a file handle).
 *       Move-construction and move-assignment are implicitly available.
 * @note Not thread-safe. Callers must serialise concurrent access externally.
 */
class KeyValue {
    /**
     * @brief FNV-inspired hash adapter so that @ref bytes can key an `std::unordered_map`.
     *
     * Reinterprets the byte vector as a `std::string_view` and delegates to
     * the standard string-view hasher, which typically uses a fast platform hash.
     */
    struct ByteVectorHash {
        size_t operator()(const bytes &v) const noexcept {
            return std::hash<std::string_view>{}(
                std::string_view(reinterpret_cast<const char*>(v.data()), v.size())
            );
        }
    };

    Log log_;
    std::unordered_map<bytes, bytes, ByteVectorHash> mem_; ///< In-memory key→value index.

public:
    /**
     * @brief Constructs a KeyValue store backed by the file at @p path.
     *
     * Does not open the file; call @ref open to initialise the store.
     *
     * @param path Filesystem path to the log file.
     */
    explicit KeyValue(const std::string &path) : log_(path) {}

    /** @brief Deleted – the underlying @ref Log owns a non-copyable file handle. */
    KeyValue(const KeyValue &)            = delete;
    /** @brief Deleted – see copy constructor. */
    KeyValue &operator=(const KeyValue &) = delete;

    /**
     * @brief Opens the backing log and replays it to rebuild the in-memory index.
     *
     * Clears any previously loaded state before replaying, so calling `open`
     * a second time performs a full reload.
     *
     * @return Empty error code on success; a log or I/O error otherwise.
     */
    std::error_code open();

    /**
     * @brief Flushes and closes the backing log.
     * @return Empty error code on success; an I/O error otherwise.
     */
    std::error_code close();

    /**
     * @brief Looks up @p key in the in-memory index.
     * @param key Binary key to search for.
     * @return `std::optional<bytes>` with the associated value if the key exists,
     *         `std::nullopt` if not found, or an `std::error_code` on failure.
     */
    std::expected<std::optional<bytes>, std::error_code> get(std::span<const std::byte> key) const;

    /**
     * @brief Controls the insertion/update behaviour of @ref set_ex.
     */
    enum class WriteMode {
        Upsert, ///< Write the value whether or not the key already exists (default).
        Insert, ///< Write only if the key does **not** yet exist.
        Update, ///< Write only if the key already exists and the value differs.
    };

    /**
     * @brief Conditionally writes @p val for @p key according to @p mode.
     *
     * Persists to the log and updates the index only when the mode condition
     * is satisfied.
     *
     * @param key  Binary key.
     * @param val  Binary value to store.
     * @param mode Controls when the write is actually performed.
     * @return `true` if a write was performed, `false` if the mode condition
     *         was not met, or an `std::error_code` on I/O failure.
     */
    std::expected<bool, std::error_code> set_ex(std::span<const std::byte> key, std::span<const std::byte> val, WriteMode mode);

    /**
     * @brief Inserts or unconditionally updates the value for @p key (`Upsert`).
     * @param key Binary key.
     * @param val Binary value to store.
     * @return `true` if the stored value changed, `false` if it was already
     *         identical, or an `std::error_code` on I/O failure.
     */
    std::expected<bool, std::error_code> set(std::span<const std::byte> key, std::span<const std::byte> val);

    /**
     * @brief Removes @p key from the store by appending a tombstone entry.
     * @param key Binary key to delete.
     * @return `true` if the key existed and was removed, `false` if the key
     *         was not present, or an `std::error_code` on I/O failure.
     */
    std::expected<bool, std::error_code> del(std::span<const std::byte> key);
};
