#pragma once

#include "log.h"
#include "entry.h"
#include "types.h"
#include <unordered_map>
#include <optional>
#include <system_error>
#include <utility>


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

    Log log;
    std::unordered_map<bytes, bytes, ByteVectorHash> mem;

    public:

    explicit KV(const std::string &path) : log(path) {}

    // Disable copying to avoid issues with the file handle
    KV(const KV &) = delete;
    KV &operator=(const KV &) = delete;

    /**
     * @brief Clears any existing in-memory data.
     * Then reads from the internal log to initialize the database.
     * @return An error code. 
     */
    std::error_code Open();

    /**
     * @brief Closes the database.
     * 
     * @return An error code (if there's any).
     */
    std::error_code Close();

    /**
     * @brief Retrieves a value by key.
     * Returns a pair containing the value (if found) and an error code.
     * @param key 
     * @return `pair<optional<bytes>, error>`. This method returns no error code for now. 
     */
    std::pair<std::optional<bytes>, std::error_code> Get(const bytes &key) const;

    /**
     * @brief Inserts or updates a value.
     * 
     * @param key 
     * @param val 
     * @return `true` if the key was newly added or the value was different.
     */
    std::pair<bool, std::error_code> Set(const bytes &key, const bytes &val);

    /**
     * @brief Removes a key from the store.
     * 
     * @param key 
     * @return `true` if the key existed and was successfully deleted.
     */
    std::pair<bool, std::error_code> Del(const bytes &key);
};