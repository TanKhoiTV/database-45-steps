#pragma once

#include "types.h"
#include <cstdint>
#include <variant>
#include <string_view>
#include <algorithm>


class Cell {
    public:

    enum class Type {
        no_type,
        i64,
        str
    };

    using I64Type = int64_t;
    using StrType = bytes;

    std::variant<std::monostate, I64Type, StrType> value_;

    private:

    explicit Cell(std::monostate v) : value_(v) {}
    explicit Cell(I64Type v) : value_(v) {}
    explicit Cell(StrType v) : value_(std::move(v)) {}

    public:

    static Cell make_empty() { return Cell(std::monostate{}); }
    static Cell make_i64(int64_t val) { return Cell(I64Type{val}); }
    static Cell make_str(bytes val) { return Cell(StrType{std::move(val)}); }
    static Cell make_str(std::string_view strv) {
        auto casted_pointer = reinterpret_cast<const std::byte *>(strv.data());
        bytes b(casted_pointer, casted_pointer + strv.size());
        return Cell(StrType{std::move(b)});
    }

    bool is_empty() const noexcept {
        return std::holds_alternative<std::monostate>(value_);
    }

    bool is_i64() const noexcept {
        return std::holds_alternative<I64Type>(value_);
    }

    bool is_str() const noexcept {
        return std::holds_alternative<StrType>(value_);
    }

    int64_t as_i64() const { return std::get<I64Type>(value_); }
    const bytes &as_str() const { return std::get<StrType>(value_); }

    bool operator==(const Cell &other) const noexcept = default;
};
