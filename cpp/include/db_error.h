#pragma once

#include <system_error>     // std::error_code, std::error_category
#include <string>           // std::string

/**
 * @brief Error codes specific to the database engine.
 * 
 * Integrates with standard error codes `std::error_code`.
 */
enum class db_error {
    ok = 0,
    truncated_header,
    truncated_payload,
    key_too_large,
    value_too_large,
    io_failure
};

/**
 * @brief Bridges `db_error` into `std::error_code`.
 * 
 * @note Must be in the same namespace as `db_error` for ADL to find it.
 * The `std::error_category` machinery calls `name()` and `message()` at runtime.
 */
struct DBErrorCategory : std::error_category {
    const char *name() const noexcept override { 
        return "KVDatabase"; 
    }

    std::string message(int ev) const override {
        switch (static_cast<db_error>(ev)) {
            case db_error::ok:                  return "Success";
            case db_error::truncated_header:    return "Entry header is incomplete or file is truncated";
            case db_error::truncated_payload:   return "Key or value payload is missing expected bytes";
            case db_error::key_too_large:       return "Key size exceeds limit";
            case db_error::value_too_large:     return "Value size exceeds limit";
            case db_error::io_failure:          return "I/O failure";
            default:                            return "Unknown database error";
        }
    }
};

/**
 * @brief DBErrorCategory singleton.
 * 
 * Defined inline so the same instance is shared across all tranlation units.
 * The static local guarantees it's constructed exactly once (thread-safe since C++11)
 * 
 * @return The singleton instance.
 */
inline const DBErrorCategory &db_category() {
    static DBErrorCategory instance;
    return instance;
}

/**
 * @brief Constructs an `std::error_code` from a `db_error` value.
 * 
 * Found via ADL when writing `std::error_code ec = db_error::truncated_header;`
 * or when returning a `db_error` from a function returning `std::error_code`.
 * 
 * @param e A `db_error`.
 * @return std::error_code The standard error code equivalent
 */
inline std::error_code make_error_code(db_error e) {
    return { static_cast<int>(e), db_category() };
}

/**
 * @brief Tells `std::error_code` that `db_error` values 
 * can be implicitly converted to `std::error_code`. 
 */
template <>
struct std::is_error_code_enum<db_error> : std::true_type {};