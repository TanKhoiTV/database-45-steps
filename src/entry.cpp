// entry.cpp
#include "entry.h"
#include "bit_utils.h"
#include <algorithm>
#include <array>

Entry::Entry() : deleted(false) {}

Entry::Entry(bytes _key, bytes _val, bool _deleted)
     : key(std::move(_key)), val(std::move(_val)), deleted(_deleted) {}


bytes Entry::Encode() const {
    uint32_t klen = static_cast<uint32_t>(key.size());
    uint32_t vlen = static_cast<uint32_t>(val.size());

    bytes buf(HEADER_SIZE + klen + (deleted ? 0 : vlen));

    auto klen_bytes = pack_le<uint32_t>(klen);
    auto vlen_bytes = pack_le<uint32_t>(vlen);
    std::copy(klen_bytes.begin(), klen_bytes.end(), buf.begin() + KLEN_OFFSET);
    std::copy(vlen_bytes.begin(), vlen_bytes.end(), buf.begin() + VLEN_OFFSET);

    buf[FLAG_OFFSET] = static_cast<std::byte>(deleted ? 1 : 0);

    // Copying key and value data
    std::copy(key.begin(), key.end(), buf.begin() + HEADER_SIZE);
    if (!deleted) {
        std::copy(val.begin(), val.end(), buf.begin() + HEADER_SIZE + klen);
    }

    return buf;
}

template <Reader R>
std::pair<Entry::DecodeResult, std::error_code> Entry::Decode(R &reader) {
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
    if (bytes_read < klen);
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