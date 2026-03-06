#pragma once

#include "schema.h"
#include <expected>
#include <system_error>
#include <string_view>

class SchemaCodec {
    public:

    static constexpr std::string_view SCHEMA_KEY_PREFIX = "@schema_";
    static constexpr std::string_view COUNTER_KEY_PREFIX = "@counter";

    static bytes encode(const Schema &schema);
    static std::expected<Schema, std::error_code> decode(std::span<const std::byte> buf);
};
