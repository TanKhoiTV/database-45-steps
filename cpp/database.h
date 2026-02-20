/**
 * @file database.h
 * @author Tran Van Tan Khoi (tranvantankhoi@gmail.com)
 * @brief A basic in-memory Key-Value database implementation
 * @version 0.1
 * @date 2026-02-20
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <unordered_map>    // std::unordered_map
#include <vector>           // std::vector
#include <string>           // std::string
#include <system_error>     // std::error_code
#include <cstddef>          // std::byte
#include <optional>         // std::optional

/**
 * @brief Alises for common types
 */
using bytes = std::vector<std::byte>;
using error = std::error_code;
using std::string;

/**
 * @brief A functor allowing `bytes` to be used as keys in hash maps.
 */
struct ByteVectorHash {
    size_t operator()(const bytes &v) const noexcept {
        return std::hash<std::string_view>{}(
            std::string_view(reinterpret_cast<const char*>(v.data()), v.size())
        );
    }
};

/**
 * @brief KV provides a simple in-memory key-value store with binary support.
 * It tracks state changes for Set and Del operations.
 */
class KV {
    private:

    std::unordered_map<bytes, bytes, ByteVectorHash> mem;

    public:

    /**
     * @brief Initializes the database. Clears any existing in-memory data.
     * 
     * @return An error code. This method returns no error code for now.
     */
    error Open() {
        mem.clear();
        return {};
    }

    /**
     * @brief Closes the database. Currently a no-op for the in-memory version.
     * 
     * @return An error code. This method returns no error code for now.
     */
    error Close() { return {}; }

    /**
     * @brief Retrieves a value by key.
     * Returns a pair containing the value (if found) and an error code.
     * @param key 
     * @return `pair<optional<bytes>, error>`. This method returns no error code for now. 
     */
    std::pair<std::optional<bytes>, error> Get(const bytes &key) const {
        auto item = mem.find(key);
        
        if (item == mem.end())
            return { std::nullopt, {} };
        return { item->second, {} };
    }

    /**
     * @brief Inserts or updates a value.
     * 
     * @param key 
     * @param val 
     * @return `true` if the key was newly added or the value was different.
     */
    std::pair<bool, error> Set(const bytes &key, const bytes &val) {
        auto item = mem.find(key);
        if (item == mem.end()) {
            mem.emplace(key, val);
            return { true, {} };
        }

        bool updated = (item->second != val);
        if (updated)
            item->second = val;
        return { updated, {} };
    }

    /**
     * @brief Removes a key from the store.
     * 
     * @param key 
     * @return `true` if the key existed and was successfully deleted.
     */
    std::pair<bool, error> Del(const bytes &key) {
        return { mem.erase(key) > 0, {} };
    }
};
