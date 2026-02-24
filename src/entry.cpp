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
    uint32_t vlen = deleted ? 0 : static_cast<uint32_t>(val.size());

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

    // Compute the checksum and add to the header
    uint32_t cksum = crc32_ieee(std::span(buf).subspan<KLEN_OFFSET>());
    auto cksum_bytes = pack_le<uint32_t>(cksum);
    std::copy(cksum_bytes.begin(), cksum_bytes.end(), buf.begin() + CKSUM_OFFSET);

    return buf;
}
