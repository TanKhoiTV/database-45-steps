#include "table.h"
#include "row.h"
#include "bit_utils.h"
#include "schema_codec.h"

static bytes schema_registry_key(const std::string &name) {
    bytes key;
    for (char c : SchemaCodec::SCHEMA_KEY_PREFIX)
        key.push_back(static_cast<std::byte>(c));
    for (char c : name)
        key.push_back(static_cast<std::byte>(c));
    return key;
}

static std::expected<std::optional<Schema>, std::error_code> load_schema(const KeyValue &kv, const std::string &name) {
    bytes key = schema_registry_key(name);
    return kv.get(key)
        .and_then([](std::optional<bytes> opt) -> std::expected<std::optional<Schema>, std::error_code> {
            if (!opt.has_value()) return std::nullopt;
            return SchemaCodec::decode(opt.value())
                .transform([](Schema s) { return std::make_optional(std::move(s)); });
        });
}

static std::expected<bool, std::error_code> save_schema(KeyValue &kv, const Schema &schema) {
    bytes registry_key = schema_registry_key(schema.name_);

    return kv.set(registry_key, SchemaCodec::encode(schema));
}

/**
 * @brief Get the next unused ID (4 bytes)
 * @warning The ID counter is irreversibly updated every time the function is called and returned successfully.
 * @param kv
 * @return std::expected<uint32_t, std::error_code>
 */
static std::expected<uint32_t, std::error_code> get_next_id(KeyValue &kv) {
    static const bytes counter_key = to_bytes(SchemaCodec::COUNTER_KEY_PREFIX);

    return kv.get(counter_key)
        .and_then([](std::optional<bytes> opt) -> std::expected<uint32_t, std::error_code> {
            if (!opt.has_value()) return 1u;
            if ((*opt).size() < 4)
                return std::unexpected(db_error::expect_more_data);
            std::array<std::byte, 4> id;
            std::copy(opt.value().begin(), opt.value().begin() + 4, id.begin());
            return unpack_le<uint32_t>(std::span<const std::byte, 4>(id));
        })
        .and_then([&](uint32_t current_id) -> std::expected<uint32_t, std::error_code> {
            auto next_id = pack_le<uint32_t>(current_id + 1);
            bytes counter_bytes(4);
            std::copy(next_id.begin(), next_id.end(), counter_bytes.begin());
            return kv.set(counter_key, next_id)
                .transform([current_id](bool) { return current_id; });
        });
}

std::expected<Table, std::error_code> Table::open(KeyValue &kv, const std::string &name) {
    return load_schema(kv, name)
        .and_then([&kv](std::optional<Schema> opt) -> std::expected<Table, std::error_code> {
            if (!opt) return std::unexpected(db_error::table_not_found);
            return Table(kv, std::move(*opt));
        });
}

std::expected<Table, std::error_code> Table::create(KeyValue &kv, Schema schema) {
    return load_schema(kv, schema.name_)
        .and_then([](std::optional<Schema> opt) -> std::expected<void, std::error_code> {
            if (opt.has_value()) return std::unexpected(db_error::table_already_exists);
            return {};
        })
        .and_then([&]() { return get_next_id(kv); })
        .and_then([&](uint32_t new_id) -> std::expected<Table, std::error_code> {
            schema.id_ = new_id;
            if (auto res = save_schema(kv, schema); !res.has_value())
                return std::unexpected(res.error());
            return Table(kv, std::move(schema));
        });
}

std::expected<Table, std::error_code> Table::open_or_create(KeyValue &kv, Schema schema) {
    return Table::open(kv, schema.name_)
        .or_else([&](std::error_code ec) -> std::expected<Table, std::error_code> {
            if (ec == db_error::table_not_found) {
                return Table::create(kv, schema);
            }
            return std::unexpected(ec);
        });
}

std::expected<bool, std::error_code> Table::Select(Row &row) const {
    return RowCodec::encode_key(schema_, row)
        .and_then([this](const bytes &key) {
            return kv_.get(key);
        })
        .and_then([this, &row](std::optional<bytes> val_opt) -> std::expected<bool, std::error_code> {
            if (!val_opt.has_value()) return false;
            if (auto err = RowCodec::decode_val(schema_, row, val_opt.value()); err)
                return std::unexpected(err);
            return true;
        });
}

std::expected<bool, std::error_code> Table::Insert(const Row &row) {
    auto key = RowCodec::encode_key(schema_, row);
    if (!key.has_value()) return std::unexpected(key.error());

    auto val = RowCodec::encode_val(schema_, row);
    if (!val.has_value()) return std::unexpected(val.error());

    return kv_.set_ex(key.value(), val.value(), KeyValue::WriteMode::Insert);
}

std::expected<bool, std::error_code> Table::Update(const Row &row) {
    auto key = RowCodec::encode_key(schema_, row);
    if (!key.has_value()) return std::unexpected(key.error());

    auto val = RowCodec::encode_val(schema_, row);
    if (!val.has_value()) return std::unexpected(val.error());

    return kv_.set_ex(key.value(), val.value(), KeyValue::WriteMode::Update);
}

std::expected<bool, std::error_code> Table::Upsert(const Row &row) {
    auto key = RowCodec::encode_key(schema_, row);
    if (!key.has_value()) return std::unexpected(key.error());

    auto val = RowCodec::encode_val(schema_, row);
    if (!val.has_value()) return std::unexpected(val.error());

    return kv_.set_ex(key.value(), val.value(), KeyValue::WriteMode::Upsert);
}

std::expected<bool, std::error_code> Table::Delete(const Row &row) {
    auto key = RowCodec::encode_key(schema_, row);
    if (!key.has_value()) return std::unexpected(key.error());

    return kv_.del(key.value());
}
