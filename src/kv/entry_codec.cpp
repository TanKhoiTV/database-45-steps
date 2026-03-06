#include "kv/entry_codec.h"
// #include "bit_utils.h"
// #include <algorithm>
// #include <array>

bytes EntryCodec::encode(const Entry &ent) {
    uint32_t klen = static_cast<uint32_t>(ent.key_.size());
    uint32_t vlen = ent.deleted_ ? 0 : static_cast<uint32_t>(ent.val_.size());

    bytes buf(HEADER_SIZE + klen + (ent.deleted_ ? 0 : vlen));

    auto klen_bytes = pack_le<uint32_t>(klen);
    auto vlen_bytes = pack_le<uint32_t>(vlen);
    std::copy(klen_bytes.begin(), klen_bytes.end(), buf.begin() + KLEN_OFFSET);
    std::copy(vlen_bytes.begin(), vlen_bytes.end(), buf.begin() + VLEN_OFFSET);

    buf[FLAG_OFFSET] = static_cast<std::byte>(ent.deleted_ ? 1 : 0);

    // Copying key and value data
    std::copy(ent.key_.begin(), ent.key_.end(), buf.begin() + HEADER_SIZE);
    if (!ent.deleted_) {
        std::copy(ent.val_.begin(), ent.val_.end(), buf.begin() + HEADER_SIZE + klen);
    }

    // Compute the checksum and add to the header
    uint32_t cksum = crc32_ieee(std::span(buf).subspan<KLEN_OFFSET>());
    auto cksum_bytes = pack_le<uint32_t>(cksum);
    std::copy(cksum_bytes.begin(), cksum_bytes.end(), buf.begin() + CKSUM_OFFSET);

    return buf;
}
