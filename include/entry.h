// entry.h
#pragma once

#include "types.h"
#include "db_error.h"
#include "platform.h"
#include "reader.h"
#include <cstdint>
#include <utility>

/**
 * @brief Represents a single database entry for serialization
 * 
 */
struct Entry {
    // Header layout constants
    static constexpr size_t HEADER_SIZE = 9;
    static constexpr size_t KLEN_OFFSET = 0;
    static constexpr size_t VLEN_OFFSET = 4;
    static constexpr size_t FLAG_OFFSET = 8;

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
        // uint8_t header[HEADER_SIZE];
        std::array<std::byte, HEADER_SIZE> header;
        size_t bytes_read = 0;

        if (auto err = reader.read(std::span<std::byte>(header), bytes_read); err)
            return { Entry::DecodeResult::fail, err };

        if (bytes_read == 0)
            return { Entry::DecodeResult::eof, {} };
        else if (bytes_read < HEADER_SIZE)
            return { Entry::DecodeResult::fail, db_error::truncated_header };

        // Unpack the header
        uint32_t klen = unpack_le<uint32_t>(std::span<const std::byte, 4>(header).subspan<KLEN_OFFSET, 4>());
        uint32_t vlen = unpack_le<uint32_t>(std::span<const std::byte, 4>(header).subspan<VLEN_OFFSET, 4>());
        deleted = (header[FLAG_OFFSET] != std::byte{0});

        // Impose data limits
        if (klen > MAX_KEY_SIZE) 
            return { Entry::DecodeResult::fail, db_error::key_too_large };
        if (vlen > MAX_VAL_SIZE) 
            return { Entry::DecodeResult::fail, db_error::value_too_large };

        // Unpack the key
        key.resize(klen);
        if (auto err = reader.read(std::span<std::byte>(key), bytes_read); err)
            return  { Entry::DecodeResult::fail, err };
        if (bytes_read < klen)
            return { Entry::DecodeResult::fail, db_error::truncated_payload };
            
        // Unpack the value data if it exists
        if (!deleted) {
            val.resize(vlen);
            if (auto err = reader.read(std::span<std::byte>(val), bytes_read); err)
                return { Entry::DecodeResult::fail, err };
            if (bytes_read < vlen)
                return { Entry::DecodeResult::fail, db_error::truncated_payload };
        } else {
            val.clear();
        }
            
        return { Entry::DecodeResult::ok, std::error_code{} };
    }
};