#include "kv.h"


std::error_code KV::Open() {
    if (log.is_open()) return {};
    if (auto err = log.Open(); err) return err;

    mem.clear();

    if (auto err = log.SeekToStart(); err) return err;

    Entry ent;
    while (true) {
        auto [res, err] = log.Read(ent);
        switch (res) {
            case Log::ReadResult::eof: return {};
            case Log::ReadResult::fail: return err;
            case Log::ReadResult::ok:
                if (ent.deleted) mem.erase(ent.key);
                else mem[ent.key] = std::move(ent.val);
                break;
        }
    }

    return {};
}

std::error_code KV::Close() { return log.Close(); }

std::pair<std::optional<bytes>, std::error_code> KV::Get(const bytes &key) const {
    auto item = mem.find(key);
    if (item == mem.end()) return { std::nullopt, {} };
    return { item->second, {} };
}

std::pair<bool, std::error_code> KV::Set(const bytes &key, const bytes &val) {
    if (auto err = log.Write(Entry{key, val, false}); err) {
        return { false, err };
    }

    auto [item, inserted] = mem.try_emplace(key, val);
    if (inserted) return { true, {} };

    bool updated = (item->second != val);
    if (updated) item->second = val;
    return { updated, {} };
}

std::pair<bool, std::error_code> KV::Del(const bytes &key) {
    if (auto err = log.Write(Entry{key, {}, true}); err)
        return { false, err };

    return { mem.erase(key) > 0, {} };
}