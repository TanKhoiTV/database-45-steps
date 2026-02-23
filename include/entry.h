// entry.h
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
    std::pair<DecodeResult, std::error_code> Decode(R &reader);
    
    private:

    /** @brief Packs/Loads a 32-bit integer (4 bytes) into a buffer in Little Endian */
    static void pack_u32(bytes &buf, size_t offset, uint32_t val);
    
    /** @brief Unpacks Little Endian bytes from buffer */
    static uint32_t unpack_u32(std::span<const std::byte, 4> buf);
};