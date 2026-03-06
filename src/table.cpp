#include "table.h"
#include "row.h"
#include "bit_utils.h"

static bytes schema_registry_key(const std::string &name) {
    bytes key{SchemaCodec::SCHEMA_NAME_PREFIX};
    for (char c : name) key.push_back(static_cast<std::byte>(c));
    return key;
}

static std::expected<std::optional<Schema>, std::error_code> load_schema(const KeyValue &schema_store, const std::string &name) {
    bytes key = schema_registry_key(name);
    return schema_store.get(key)
        .and_then([](std::optional<bytes> opt) -> std::expected<std::optional<Schema>, std::error_code> {
            if (!opt.has_value()) return std::nullopt;
            return SchemaCodec::decode(opt.value())
                .transform([](Schema s) { return std::make_optional(std::move(s)); });
        });
}

static std::expected<bool, std::error_code> save_schema(KeyValue &schema_store, const Schema &schema) {
    bytes registry_key = schema_registry_key(schema.name_);

    return schema_store.set(registry_key, SchemaCodec::encode(schema));
}

/**
 * @brief Get the next unused ID (4 bytes)
 * @warning The ID counter is irreversibly updated every time the function is called and returned successfully.
 * @param schema_store
 * @return std::expected<uint32_t, std::error_code>
 */
static std::expected<uint32_t, std::error_code> get_next_id(KeyValue &schema_store) {
    static const bytes counter_key = {SchemaCodec::COUNTER_ID_PREFIX};

    return schema_store.get(counter_key)
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
            return schema_store.set(counter_key, next_id)
                .transform([current_id](bool) { return current_id; });
        });
}

std::expected<Table, std::error_code> Table::open(KeyValue &data_store, const KeyValue &schema_store, const std::string &name) {
    return load_schema(schema_store, name)
        .and_then([&data_store](std::optional<Schema> opt) -> std::expected<Table, std::error_code> {
            if (!opt) return std::unexpected(db_error::table_not_found);
            return Table(data_store, std::move(*opt));
        });
}

std::expected<Table, std::error_code> Table::create(KeyValue &data_store, KeyValue &schema_store, Schema schema) {
    return load_schema(schema_store, schema.name_)
        .and_then([](std::optional<Schema> opt) -> std::expected<void, std::error_code> {
            if (opt.has_value()) return std::unexpected(db_error::table_already_exists);
            return {};
        })
        .and_then([&]() { return get_next_id(schema_store); })
        .and_then([&](uint32_t new_id) -> std::expected<Table, std::error_code> {
            schema.id_ = new_id;
            if (auto res = save_schema(schema_store, schema); !res.has_value())
                return std::unexpected(res.error());
            return Table(data_store, std::move(schema));
        });
}

std::expected<Table, std::error_code> Table::open_or_create(KeyValue &data_store, KeyValue &schema_store, Schema schema) {
    return Table::open(data_store, schema_store, schema.name_)
        .or_else([&](std::error_code ec) -> std::expected<Table, std::error_code> {
            if (ec == db_error::table_not_found) {
                return Table::create(data_store, schema_store, schema);
            }
            return std::unexpected(ec);
        });
}

std::expected<bool, std::error_code> Table::Select(Row &row) const {
    return RowCodec::encode_key(schema_, row)
        .and_then([this](const bytes &key) {
            return data_store_.get(key);
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

    return data_store_.set_ex(key.value(), val.value(), KeyValue::WriteMode::Insert);
}

std::expected<bool, std::error_code> Table::Update(const Row &row) {
    auto key = RowCodec::encode_key(schema_, row);
    if (!key.has_value()) return std::unexpected(key.error());

    auto val = RowCodec::encode_val(schema_, row);
    if (!val.has_value()) return std::unexpected(val.error());

    return data_store_.set_ex(key.value(), val.value(), KeyValue::WriteMode::Update);
}

std::expected<bool, std::error_code> Table::Upsert(const Row &row) {
    auto key = RowCodec::encode_key(schema_, row);
    if (!key.has_value()) return std::unexpected(key.error());

    auto val = RowCodec::encode_val(schema_, row);
    if (!val.has_value()) return std::unexpected(val.error());

    return data_store_.set_ex(key.value(), val.value(), KeyValue::WriteMode::Upsert);
}

std::expected<bool, std::error_code> Table::Delete(const Row &row) {
    auto key = RowCodec::encode_key(schema_, row);
    if (!key.has_value()) return std::unexpected(key.error());

    return data_store_.del(key.value());
}
