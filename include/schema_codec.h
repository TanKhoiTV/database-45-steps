#pragma once

#include "schema.h"
#include <expected>
#include <system_error>

class SchemaCodec {
    public:

    static constexpr std::byte SCHEMA_NAME_PREFIX = static_cast<std::byte>(0xff);
    static constexpr std::byte COUNTER_ID_PREFIX = static_cast<std::byte>(0xfe);

    static bytes encode(const Schema &schema);
    static std::expected<Schema, std::error_code> decode(std::span<const std::byte> buf);
};
