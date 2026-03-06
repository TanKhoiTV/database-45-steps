#pragma once

#include "kv.h"
#include "row.h"
#include "row_codec.h"
#include "schema.h"
#include "schema_codec.h"
#include <system_error>
#include <string>

class Table {
    private:

    KeyValue &data_store_;
    Schema schema_;

    Table(KeyValue &kv, Schema schema) :  data_store_(kv), schema_(schema) {}

    public:

    static std::expected<Table, std::error_code> open(KeyValue &data_store, const KeyValue &schema_store, const std::string &name);

    static std::expected<Table, std::error_code> create(KeyValue &data_store, KeyValue &schema_store, Schema schema);

    static std::expected<Table, std::error_code> open_or_create(KeyValue &data_store, KeyValue &schema_store, Schema schema);

    std::expected<bool, std::error_code> Select(Row &row) const;
    std::expected<bool, std::error_code> Insert(const Row &row);
    std::expected<bool, std::error_code> Update(const Row &row);
    std::expected<bool, std::error_code> Upsert(const Row &row);
    std::expected<bool, std::error_code> Delete(const Row &row);

    const Schema &schema() const noexcept { return schema_; }
    Row new_row() const { return RowCodec::new_row(schema_); }
};
