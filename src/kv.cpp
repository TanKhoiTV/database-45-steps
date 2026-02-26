#include "kv.h"
#include "types.h"

std::error_code KV::open() {
    if (log.is_open()) return {};
    if (auto err = log.open(); err) return err;

    mem.clear();

    if (auto err = log.seek_to_first_entry(); err) return err;

    while (true) {
        auto result = log.read();
        if (!result.has_value())
            return result.error();
        if (std::holds_alternative<LogEOF>(result.value()))
            return {};

        auto &ent = std::get<Entry>(result.value());
        if (ent.deleted) mem.erase(ent.key);
        else mem[ent.key] = ent.val;
    }

    return {};
}

std::error_code KV::close() { return log.close(); }

std::pair<std::optional<bytes>, std::error_code> KV::get(std::span<const std::byte> key) const {
    auto item = mem.find(bytes(key.begin(), key.end()));
    if (item == mem.end()) return { std::nullopt, {} };
    return { item->second, {} };
}

std::pair<bool, std::error_code> KV::set(std::span<const std::byte> key, std::span<const std::byte> val) {
    if (auto err = log.write(Entry{to_bytes(key), to_bytes(val), false}); err) {
        return { false, err };
    }

    auto key_bytes = to_bytes(key);
    auto val_bytes = to_bytes(val);

    auto [item, inserted] = mem.try_emplace(key_bytes, val_bytes);
    if (inserted) return { true, {} };

    bool updated = (item->second != val_bytes);
    if (updated) item->second = val_bytes;
    return { updated, {} };
}

std::pair<bool, std::error_code> KV::del(std::span<const std::byte> key) {
    if (auto err = log.write(Entry{to_bytes(key), {}, true}); err)
        return { false, err };

    return { mem.erase(to_bytes(key)) > 0, {} };
}
