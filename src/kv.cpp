#include "kv.h"


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

std::pair<std::optional<bytes>, std::error_code> KV::get(const bytes &key) const {
    auto item = mem.find(key);
    if (item == mem.end()) return { std::nullopt, {} };
    return { item->second, {} };
}

std::pair<bool, std::error_code> KV::set(const bytes &key, const bytes &val) {
    if (auto err = log.write(Entry{key, val, false}); err) {
        return { false, err };
    }

    auto [item, inserted] = mem.try_emplace(key, val);
    if (inserted) return { true, {} };

    bool updated = (item->second != val);
    if (updated) item->second = val;
    return { updated, {} };
}

std::pair<bool, std::error_code> KV::del(const bytes &key) {
    if (auto err = log.write(Entry{key, {}, true}); err)
        return { false, err };

    return { mem.erase(key) > 0, {} };
}
