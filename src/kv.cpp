#include "kv.h"
#include "types.h"

std::error_code KeyValue::open() {
    if (log_.is_open()) return {};
    if (auto err = log_.open(); err) return err;

    mem_.clear();

    if (auto err = log_.seek_to_first_entry(); err) return err;

    while (true) {
        auto result = log_.read();
        if (!result.has_value())
            return result.error();
        if (std::holds_alternative<LogEOF>(result.value()))
            return {};

        auto &ent = std::get<Entry>(result.value());
        if (ent.deleted_) mem_.erase(ent.key_);
        else mem_[ent.key_] = ent.val_;
    }

    return {};
}

std::error_code KeyValue::close() { return log_.close(); }

std::expected<std::optional<bytes>, std::error_code> KeyValue::get(std::span<const std::byte> key) const {
    auto it = mem_.find(to_bytes(key));
    if (it == mem_.end()) return std::nullopt;
    return it->second;
}

std::expected<bool, std::error_code> KeyValue::set_ex(std::span<const std::byte> key, std::span<const std::byte> val, WriteMode mode) {
    auto my_key = to_bytes(key);
    auto my_val = to_bytes(val);

    auto it = mem_.find(my_key);
    bool exist = (it != mem_.end());
    bool updated = false;

    switch (mode) {
        case WriteMode::Upsert: updated = !exist || (it->second != my_val); break;
        case WriteMode::Insert: updated = !exist; break;
        case WriteMode::Update: updated = exist && (it->second != my_val); break;
    }

    if (!updated) return false;

    if (auto err = log_.write(Entry(my_key, my_val, false)); err) {
        return std::unexpected(err);
    }
    mem_.insert_or_assign(my_key, my_val);
    return updated;
}

std::expected<bool, std::error_code> KeyValue::set(std::span<const std::byte> key, std::span<const std::byte> val) {
    return set_ex(key, val, WriteMode::Upsert);
}

std::expected<bool, std::error_code> KeyValue::del(std::span<const std::byte> key) {
    auto my_key = to_bytes(key);
    auto it = mem_.find(my_key);

    if (it == mem_.end()) {
        return false;
    }
    if (auto err = log_.write(Entry(my_key, {}, true)); err)
        return std::unexpected(err);
    mem_.erase(it);
    return true;
}
