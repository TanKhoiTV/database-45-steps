// include/table/cell.h
#pragma once

/**
 * @file cell.h
 * @brief A single typed value that occupies one column in a @ref Row.
 */

#include "core/types.h"  // bytes, to_bytes
#include <cstdint>       // int64_t
#include <variant>       // std::variant, std::monostate, std::holds_alternative, std::get
#include <string_view>   // std::string_view

/**
 * @brief A discriminated union over all column value types supported by the engine.
 *
 * `Cell` is the atomic unit of data within a @ref Row.  It wraps an
 * `std::variant` whose active alternative is one of:
 * - `std::monostate` — SQL NULL / empty / no-type
 * - `I64Type`        — a 64-bit signed integer
 * - `StrType`        — an arbitrary binary string (@ref bytes)
 *
 * Construction is only possible via the named factory methods (`make_*`) to
 * prevent accidental implicit conversions.
 *
 * All special members are defaulted; the class is cheap to copy and move
 * because `bytes` (a `std::vector`) already has efficient move semantics.
 */
class Cell {
public:
    /** @brief Tag that identifies which alternative is active. */
    enum class Type {
        no_type,  ///< `std::monostate` — represents a NULL / absent value.
        i64,      ///< 64-bit signed integer.
        str,      ///< Binary string (@ref bytes).
    };

    using I64Type      = int64_t;         ///< Underlying type for @ref Type::i64.
    using StrType      = bytes;           ///< Underlying type for @ref Type::str.
    /** @brief The full variant type; index matches the integer value of @ref Type. */
    using TypeVariants = std::variant<std::monostate, I64Type, StrType>;

private:
    TypeVariants value_;

    explicit Cell(std::monostate v) : value_(v) {}
    explicit Cell(I64Type v)        : value_(v) {}
    explicit Cell(StrType v)        : value_(std::move(v)) {}

public:
    Cell(const Cell &) noexcept            = default;
    Cell &operator=(const Cell &) noexcept = default;
    Cell(Cell &&) noexcept                 = default;
    Cell &operator=(Cell &&) noexcept      = default;

    /** @brief Constructs a NULL cell (`Type::no_type`). */
    static Cell make_empty() { return Cell(std::monostate{}); }

    /**
     * @brief Constructs an integer cell.
     * @param val The `int64_t` value to store.
     */
    static Cell make_i64(int64_t val) { return Cell(I64Type{val}); }

    /**
     * @brief Constructs a binary-string cell from an owned @ref bytes buffer.
     * @param val Buffer to move into the cell.
     */
    static Cell make_str(bytes val) { return Cell(StrType{std::move(val)}); }

    /**
     * @brief Constructs a binary-string cell by copying a string view.
     * @param strv Source characters; converted via @ref to_bytes.
     */
    static Cell make_str(std::string_view strv) {
        return Cell(StrType{std::move(to_bytes(strv))});
    }

    /**
     * @brief Returns the zero-based variant index (matches `static_cast<size_t>(Type)`).
     * @return Index of the active alternative.
     */
    size_t index() const noexcept { return value_.index(); }

    /** @return Const reference to the underlying variant for visitor dispatch. */
    const TypeVariants &value() const noexcept { return value_; }

    /** @return `true` if the active alternative is `std::monostate` (NULL). */
    bool is_empty() const noexcept {
        return std::holds_alternative<std::monostate>(value_);
    }

    /** @return `true` if the active alternative is @ref I64Type. */
    bool is_i64() const noexcept {
        return std::holds_alternative<I64Type>(value_);
    }

    /** @return `true` if the active alternative is @ref StrType. */
    bool is_str() const noexcept {
        return std::holds_alternative<StrType>(value_);
    }

    /**
     * @brief Returns the stored integer.
     * @throws `std::bad_variant_access` if the active alternative is not `i64`.
     */
    int64_t as_i64() const { return std::get<I64Type>(value_); }

    /**
     * @brief Returns a const reference to the stored string.
     * @throws `std::bad_variant_access` if the active alternative is not `str`.
     */
    const bytes &as_str() const { return std::get<StrType>(value_); }

    /** @brief Equality is delegated to the underlying variant's `operator==`. */
    bool operator==(const Cell &other) const noexcept = default;
};
