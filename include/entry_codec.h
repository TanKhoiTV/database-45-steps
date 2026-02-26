#pragma once

#include "entry.h"
#include "types.h"
#include "db_error.h"
#include "platform.h"
#include "reader.h"
#include "bit_utils.h"
#include <cstdint>
#include <variant>
#include <span>
#include <array>
#include <expected>

struct EntryEOF {};

using DecodeResult = std::expected<std::variant<Entry, EntryEOF>, std::error_code>;

/**
 * @brief Handles Entry serialization format and logic.
 *
 */
struct EntryCodec {
    // Header layout constants
    static constexpr size_t CKSUM_OFFSET = 0;
    static constexpr size_t KLEN_OFFSET = CKSUM_OFFSET + 4;
    static constexpr size_t VLEN_OFFSET = KLEN_OFFSET + 4;
    static constexpr size_t FLAG_OFFSET = VLEN_OFFSET + 4;
    static constexpr size_t HEADER_SIZE = FLAG_OFFSET + 1;

    // Safety Limits
    static constexpr size_t MAX_KEY_SIZE = 1024;            // 1 KB limit
    static constexpr size_t MAX_VAL_SIZE = 1024 * 1024;     // 1 MB limit

    /**
     * @brief Serializes the entry into a byte buffer.
     * Format: `[ cksum(4) | klen(4) | vlen(4) | flag(1) | key | val ]`.
     * @return bytes containing the encoded entry.
     */
    static bytes encode(const Entry &ent);

    /**
     * @brief Deserializes an entry by reading from a FileHandle.
     *
     * @param reader
     * @return std::error_code
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
    ent.deleted = is_deleted;
    ent.key.assign(payload.begin(), payload.begin() + klen);
    if (!is_deleted) ent.val.assign(payload.begin() + klen, payload.end());

    return ent;
}
