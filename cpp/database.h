/**
 * @file database.h
 * @author Tran Van Tan Khoi (tranvantankhoi@gmail.com)
 * @brief A basic in-memory Key-Value database implementation
 * @version 0.3.0-alpha.4
 * @date 2026-02-20
 * 
 * @copyright Copyright (c) under MIT license, 2026
 * 
 */

#pragma once

#include <unordered_map>    // std::unordered_map
#include <vector>           // std::vector
#include <string>           // std::string
#include <system_error>     // std::error_code
#include <cstddef>          // std::byte
#include <optional>         // std::optional
#include <algorithm>        // std::copy
#include <cstdint>          // uint32_t
#include <iostream>         // std::istream
#include <fstream>          // std::fstream
#include <filesystem>       // std::filesystem
#include <cerrno>           // errno

/** Aliases for common types */

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


class Log {
    public:

    explicit Log(std::string fname) : filename(std::move(fname)) {}

    string filename;
    std::fstream fs;    // File Stream

    error Open() {
        // Error handling: File name is a directory instead of file
        if (std::filesystem::exists(filename) && std::filesystem::is_directory(filename))
            return std::make_error_code(std::errc::is_a_directory);

        // Try opening the file for reads and writes in binary, with writes append to end of file. Append also creates the file if it doesn't exist.
        fs.open(filename, std::ios::in | std::ios::out | std::ios::binary | std::ios::app);
        
        // Handling common errors
        if (!fs.is_open()) {
            switch (errno) {
                case EACCES: return std::make_error_code(std::errc::permission_denied);
                case ENOENT: return std::make_error_code(std::errc::no_such_file_or_directory);
                case ENOSPC: return std::make_error_code(std::errc::no_space_on_device);
                default:    return std::make_error_code(std::errc::io_error);
            }
        }

        fs.clear(); // Clear any potential EOF bits from the check
        return {};
    }

    error Close() {
        if (!fs.is_open())
            return {};
        
        fs.close();
        if (fs.fail()) {
            return std::make_error_code(std::errc::io_error);
        }
        return {};
    }

    error Write(const Entry &ent) {
        bytes data_bytes = ent.Encode();
        fs.write(reinterpret_cast<const char *>(data_bytes.data()), data_bytes.size());
        if (fs.fail()) {
            switch errno {
                case ENOSPC:    return std::make_error_code(std::errc::no_space_on_device);
                case EIO:       return std::make_error_code(std::errc::io_error);
                default:        return std::make_error_code(std::errc::broken_pipe);
            }
        }
        fs.flush();
        return {};
    }

    std::pair<bool, error> Read(Entry &ent) {
        error err = ent.Decode(fs);

        if (!err)
            return { false, {} };
        
        if (fs.eof()) {
            fs.clear(); // Clears the EOF bit so the stream is usable later
            return { true, {}} ;
        }

        return { false, err };
    }

    ~Log() {
        if (fs.is_open())
            fs.close();
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

    KV(const KV &) = delete;
    KV &operator=(const KV &) = delete;

    /**
     * @brief Clears any existing in-memory data.
     * Then reads from the internal log to initialize the database.
     * @return An error code. 
     */
    error Open() {
        if (error err = log.Open(); err)
            return err;

        mem.clear();

        // Read from the beginning by moving the read position
        log.fs.seekg(0, std::ios::beg);

        Entry ent;
        while (true) {
            auto [eof, err] = log.Read(ent);
            if (err) return err;
            if (eof) break;

            if (ent.deleted) mem.erase(ent.key);
            else mem[ent.key] = ent.val;
        }

        return {};
    }

    /**
     * @brief Closes the database.
     * 
     * @return An error code.
     */
    error Close() { return log.Close(); }

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
        if (error err = log.Write(Entry{key, val, false}); err) {
            return { false, err };
        }

        auto [item, inserted] = mem.try_emplace(key);
        if (inserted) {
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
        if (error err = log.Write(Entry{key, {}, true}); err) {
            return { false, err };
        }

        return { mem.erase(key) > 0, {} };
    }
};


/**
 * @brief Represents a single database entry for serialization
 * 
 */
struct Entry {
    // Header layout constants
    static constexpr size_t HEADER_SIZE = 9;
    static constexpr size_t KLEN_OFFSET = 0;
    static constexpr size_t VLEN_OFFSET = 4;
    static constexpr size_t FLAG_OFFSET = 8;

    // Safety Limits
    static constexpr size_t MAX_KEY_SIZE = 1024;
    static constexpr size_t MAX_VAL_SIZE = 1024 * 1024;

    bytes key;
    bytes val;
    bool deleted;

    Entry() : deleted(false) {}

    Entry(bytes _key, bytes _val, bool _deleted)
        : key(std::move(_key)), val(std::move(_val)), deleted(_deleted) {}

    // Pack/Load a 32-bit integer (4 bytes) into a buffer in Little Endian
    static void pack_u32(bytes &buf, size_t offset, uint32_t val) {
        buf[offset]     = static_cast<std::byte>(val & 0xFF);
        buf[offset + 1] = static_cast<std::byte>((val >> 8) & 0xFF);
        buf[offset + 2] = static_cast<std::byte>((val >> 16) & 0xFF);
        buf[offset + 3] = static_cast<std::byte>((val >> 24) & 0xFF);
    }

    // Unpack a Little Endian 32-bit integer from buffer
    static uint32_t unpack_u32(const uint8_t *buf) {
        return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
    }

    /**
     * @brief Serializes the entry into a byte buffer.
     * The format is [4-byte key size | 4-byte value size | 1-byte 'deleted' flag | key payload | value payload].
     * @return bytes containing the encoded entry.
     */
    bytes Encode() const {
        uint32_t klen = static_cast<uint32_t>(key.size());
        uint32_t vlen = static_cast<uint32_t>(val.size());

        bytes buf(HEADER_SIZE + klen + (deleted ? 0 : vlen));

        pack_u32(buf, KLEN_OFFSET, klen);
        pack_u32(buf, VLEN_OFFSET, vlen);
        buf[FLAG_OFFSET] = static_cast<std::byte>(deleted ? 1 : 0);

        // Copying key and value data
        std::copy(key.begin(), key.end(), buf.begin() + HEADER_SIZE);
        if (!deleted) {
            std::copy(val.begin(), val.end(), buf.begin() + HEADER_SIZE + klen);
        }

        return buf;
    }

    /**
     * @brief Deserializes an entry from a stream.
     * 
     * @param `is` The input stream
     * @return An error if reading fails.
     */
    error Decode(std::istream &is) {
        uint8_t header[HEADER_SIZE];
        if (!is.read(reinterpret_cast<char *>(header), HEADER_SIZE)) {
            return std::make_error_code(std::errc::illegal_byte_sequence);
        }

        // Unpack the header
        uint32_t klen = unpack_u32(header + KLEN_OFFSET);
        uint32_t vlen = unpack_u32(header + VLEN_OFFSET);
        deleted = (header[FLAG_OFFSET] != 0);

        // Impose data limits
        if (klen > MAX_KEY_SIZE || vlen > MAX_VAL_SIZE) {
            return std::make_error_code(std::errc::result_out_of_range);
        }

        // Unpack the key
        key.resize(klen);
        if (!is.read(reinterpret_cast<char *>(key.data()), klen))
            return std::make_error_code(std::errc::io_error);
        
        // Unpack the value data if it exists
        if (!deleted) {
            val.resize(vlen);
            if (!is.read(reinterpret_cast<char *>(val.data()), vlen))
                return std::make_error_code(std::errc::io_error);
        }
        
        return {};
    }
};