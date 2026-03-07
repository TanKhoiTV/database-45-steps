// include/kv/entry_codec.h
#pragma once

/**
 * @file entry_codec.h
 * @brief Binary serialisation and deserialisation of @ref Entry objects.
 *
 * Wire format for a single entry:
 * ```
 * [ cksum(4) | klen(4) | vlen(4) | flag(1) | key(klen) | val(vlen) ]
 * ```
 * All multi-byte integers are little-endian.
 * The CRC-32 (IEEE 802.3) covers every byte from `klen` onward (i.e. the
 * checksum field itself is excluded from the digest).
 * Deleted entries (tombstones) omit the value payload (`vlen` is written as 0).
 */

#include "kv/entry.h"
#include "core/types.h"
#include "core/db_error.h"
#include "core/bit_utils.h"
#include "core/reader.h"
#include <cstdint>      // uint32_t
#include <variant>      // std::variant
#include <span>         // std::span
#include <array>        // std::array
#include <expected>     // std::expected

/** @brief Sentinel returned by @ref EntryCodec::decode when the stream is exhausted. */
struct EntryEOF {};

/**
 * @brief Result type of @ref EntryCodec::decode.
 *
 * Contains either a decoded @ref Entry, an @ref EntryEOF sentinel, or an
 * `std::error_code` on failure.
 */
using DecodeResult = std::expected<std::variant<Entry, EntryEOF>, std::error_code>;

/**
 * @brief Stateless codec for the Entry binary format.
 *
 * Both `encode` and `decode` are static; the struct exists solely to
 * namespace the layout constants alongside the logic that uses them.
 */
struct EntryCodec {
    /** @name Header layout (byte offsets within the fixed-size header) @{ */
    static constexpr size_t CKSUM_OFFSET = 0;                       ///< CRC-32 checksum field (4 bytes).
    static constexpr size_t KLEN_OFFSET  = CKSUM_OFFSET + 4;        ///< Key-length field (4 bytes).
    static constexpr size_t VLEN_OFFSET  = KLEN_OFFSET  + 4;        ///< Value-length field (4 bytes).
    static constexpr size_t FLAG_OFFSET  = VLEN_OFFSET  + 4;        ///< Deletion-flag field (1 byte).
    static constexpr size_t HEADER_SIZE  = FLAG_OFFSET  + 1;        ///< Total header size in bytes.
    /** @} */

    /** @name Safety limits @{ */
    static constexpr size_t MAX_KEY_SIZE = 1024;            ///< Maximum permitted key size (1 KiB).
    static constexpr size_t MAX_VAL_SIZE = 1024 * 1024;     ///< Maximum permitted value size (1 MiB).
    /** @} */

    /**
     * @brief Serialises @p ent into a heap-allocated byte buffer.
     *
     * The CRC-32 digest is computed over all header bytes starting at
     * @ref KLEN_OFFSET and over the entire payload, then written into
     * @ref CKSUM_OFFSET.  Tombstones (`deleted_ == true`) omit the value
     * bytes and write `vlen = 0`.
     *
     * @param ent The entry to encode.
     * @return A @ref bytes buffer containing the complete on-disk representation.
     */
    static bytes encode(const Entry &ent);

    /**
     * @brief Deserialises the next entry from @p reader.
     *
     * Reads the fixed-size header first, then the variable-length payload.
     * Validates the CRC-32 checksum before constructing the @ref Entry.
     * Returns @ref EntryEOF when the reader signals EOF on the very first
     * byte of the header (i.e. a clean end-of-log).
     *
     * @tparam R Any type satisfying the @ref Reader concept.
     * @param reader Source of raw bytes (typically a @ref FileHandle).
     * @return A @ref DecodeResult holding the decoded @ref Entry, @ref EntryEOF,
     *         or an `std::error_code` on failure.
     */
    template <Reader R> static DecodeResult decode(R &reader);
};

template <Reader R> DecodeResult EntryCodec::decode(R &reader) {
    // Read the header
    std::array<std::byte, EntryCodec::HEADER_SIZE> header;
    size_t bytes_read = 0;

    if (auto err = reader.read(std::span(header), bytes_read); err)
        return std::unexpected(err);
    if (bytes_read == 0)
        return EntryEOF{};
    else if (bytes_read < EntryCodec::HEADER_SIZE)
        return std::unexpected(db_error::truncated_header);

    // Unpack the header
    uint32_t stored_cksum = unpack_le<uint32_t>(std::span<const std::byte>(header).subspan<EntryCodec::CKSUM_OFFSET, 4>());
    uint32_t klen = unpack_le<uint32_t>(std::span<const std::byte>(header).subspan<EntryCodec::KLEN_OFFSET, 4>());
    uint32_t vlen = unpack_le<uint32_t>(std::span<const std::byte>(header).subspan<EntryCodec::VLEN_OFFSET, 4>());
    uint32_t is_deleted = (header[EntryCodec::FLAG_OFFSET] != std::byte{0});

    // Impose data limits
    if (klen > MAX_KEY_SIZE)
        return std::unexpected(db_error::key_too_large);
    if (vlen > MAX_VAL_SIZE)
        return std::unexpected(db_error::value_too_large);

    // Read the payload into a buffer
    size_t payload_size = klen + (is_deleted ? 0 : vlen);
    bytes payload(payload_size);
    if (payload_size > 0) {
        if (auto err = reader.read(std::span(payload), bytes_read); err)
            return std::unexpected(err);
        if (bytes_read < payload_size)
            return std::unexpected(db_error::truncated_payload);
    }

    // Verify the checksum
    uint32_t c_cksum = crc32_init();
    c_cksum = crc32_update(c_cksum, std::span<const std::byte>(header).subspan<EntryCodec::KLEN_OFFSET>());
    c_cksum = crc32_update(c_cksum, std::span<const std::byte>(payload));
    if (crc32_final(c_cksum) != stored_cksum)
        return std::unexpected(db_error::bad_checksum);

    // Unpack the payload
    Entry ent;
    ent.deleted_ = is_deleted;
    ent.key_.assign(payload.begin(), payload.begin() + klen);
    if (!is_deleted) ent.val_.assign(payload.begin() + klen, payload.end());

    return ent;
}
