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
    auto it = mem.find(to_bytes(key));
    if (it == mem.end()) return { std::nullopt, {} };
    return { it->second, {} };
}

std::pair<bool, std::error_code> KV::set(std::span<const std::byte> key, std::span<const std::byte> val, UpdateMode mode) {
    auto my_key = to_bytes(key);
    auto my_val = to_bytes(val);

    auto it = mem.find(my_key);
    bool exist = (it != mem.end());
    bool updated = false;

    switch (mode) {
        case UpdateMode::Upsert: updated = !exist || (it->second != my_val); break;
        case UpdateMode::Insert: updated = !exist; break;
        case UpdateMode::Update: updated = exist && (it->second != my_val); break;
    }

    if (!updated) {
        return { false, {} };
    }

    if (auto err = log.write(Entry(my_key, my_val, false)); err) {
        return { false, err };
    }
    mem.insert_or_assign(my_key, my_val);
    return { updated, {} };
}

std::pair<bool, std::error_code> KV::del(std::span<const std::byte> key) {
    auto my_key = to_bytes(key);
    auto it = mem.find(my_key);

    if (it == mem.end()) {
        return { false, {} };
    }
    if (auto err = log.write(Entry(my_key, {}, true)); err)
        return { false, err };
    mem.erase(it);
    return { true, {} };
}
