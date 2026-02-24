// entry.h
#pragma once

#include "types.h"
#include "db_error.h"
#include "platform.h"
#include "reader.h"
#include "bit_utils.h"
#include <cstdint>
#include <utility>
#include <span>
#include <array>
#include <algorithm>

/**
 * @brief Represents a single database entry for serialization
 * 
 */
struct Entry {
    // Header layout constants
    static constexpr size_t HEADER_SIZE = 13;
    static constexpr size_t CKSUM_OFFSET = 0;
    static constexpr size_t KLEN_OFFSET = 4;
    static constexpr size_t VLEN_OFFSET = 8;
    static constexpr size_t FLAG_OFFSET = 12;

    // Safety Limits
    static constexpr size_t MAX_KEY_SIZE = 1024;            // 1 KB limit
    static constexpr size_t MAX_VAL_SIZE = 1024 * 1024;     // 1 MB limit

    bytes key;
    bytes val;
    bool deleted;

    Entry();
    Entry(bytes _key, bytes _val, bool _deleted);
    
    /**
     * @brief Serializes the entry into a byte buffer.
     * Format: `[klen(4)|vlen(4)|flag(1)|key|val]`.
     * @return bytes containing the encoded entry.
     */
    bytes Encode() const;

    enum class DecodeResult { ok, eof, fail };
    
    /**
     * @brief Deserializes an entry by reading from a FileHandle.
     * 
     * @param reader 
     * @return std::error_code 
     */
    template <Reader R>
    std::pair<DecodeResult, std::error_code> Decode(R &reader) {
        // Read the header
        std::array<std::byte, HEADER_SIZE> header;
        size_t bytes_read = 0;

        if (auto err = reader.read(std::span(header), bytes_read); err)
            return { DecodeResult::fail, err };
        if (bytes_read == 0)
            return { DecodeResult::eof, {} };
        else if (bytes_read < HEADER_SIZE)
            return { DecodeResult::fail, db_error::truncated_header };

        // Unpack the header
        uint32_t stored_cksum = unpack_le<uint32_t>(std::span<const std::byte>(header).subspan<CKSUM_OFFSET, 4>());
        uint32_t klen = unpack_le<uint32_t>(std::span<const std::byte>(header).subspan<KLEN_OFFSET, 4>());
        uint32_t vlen = unpack_le<uint32_t>(std::span<const std::byte>(header).subspan<VLEN_OFFSET, 4>());
        uint32_t is_deleted = (header[FLAG_OFFSET] != std::byte{0});

        // Impose data limits
        if (klen > MAX_KEY_SIZE) 
            return { DecodeResult::fail, db_error::key_too_large };
        if (vlen > MAX_VAL_SIZE) 
            return { DecodeResult::fail, db_error::value_too_large };

        // Read the payload into a buffer
        size_t payload_size = klen + (is_deleted ? 0 : vlen);
        bytes payload(payload_size);
        if (payload_size > 0) {
            if (auto err = reader.read(std::span(payload), bytes_read); err)
                return { DecodeResult::fail, err };
            if (bytes_read < payload_size)
                return { DecodeResult::fail, db_error::truncated_payload };
        }

        // Verify the checksum
        uint32_t c_cksum = crc32_init();
        c_cksum = crc32_update(c_cksum, std::span<const std::byte>(header).subspan<KLEN_OFFSET>());
        c_cksum = crc32_update(c_cksum, std::span<const std::byte>(payload));
        if (crc32_final(c_cksum) != stored_cksum)
            return { DecodeResult::fail, db_error::bad_checksum };

        // Unpack the payload
        deleted = is_deleted;
        key.assign(payload.begin(), payload.begin() + klen);
        if (!deleted)
            val.assign(payload.begin() + klen, payload.end());
        else val.clear();

        return { DecodeResult::ok, {} };
    }
};