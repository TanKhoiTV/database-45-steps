# Creating a Database from Scratch in 45 Steps

Inspired by Trial of Code's [Code a database in 45 steps (Go)](https://trialofcode.org/database/), this is a C++ adaptation built for self-learning. The goal is to closely mirror the Go implementation while using idiomatic modern C++ in place of Go-specific idioms.

## Features

- **C++23**: Uses modern C++ concepts like `std::span`, `std::bit_cast`, `std::endian`, `std::variant`, `std::expected`, `std::optional`, and many more.
- **Binary safe**: Raw `std::byte` vectors as keys and values throughout.
- **Durable writes**: Every `Set` and `Del` call fsyncs to disk before returning, mirroring Go's `os.File.Sync()`.
- **Platform abstraction**: POSIX and Windows file I/O are isolated behind a `FileHandle` RAII class and a set of `platform_*` functions, selected at build time.
- **File format versioning**: Log files begin with a magic number and format version header, so format mismatches are detected on open rather than producing silent corruption.
- **Reader concept**: `Entry::Decode` is generic over any type satisfying the `Reader` concept, enabling in-memory decoding in tests without touching the filesystem.
- **Doxygen support**: Documented source for automated documentation generation.

## Limitations

- **Not thread-safe**: Do not share a `KV` instance across threads or run multiple instances against the same file.
- **Append-only**: No compaction yet. The log grows indefinitely; deleted keys leave tombstones on disk.
- **Sequential access only**: No indexing or range queries beyond full log replay on open.

---

## How It Works

This database stores data in memory while running, and saves to disk when it closes. Sounds simple, but to ensure data integrity, this database is constructed with two principles in mind: **Durability** and **Atomicity**.

Durability means any data that get written to file must reach the disk, the actual storage. It might sound surprising to you though, that writing to a file normally does not guarantee the file will not disappear when a power outage happens _even if_ it happens after all data was successfully written. The database make sure that if data was successfully written, it will not disappear no matter what happens afterwards. _Read more on how the OS cache works_.

Atomicity means the data is read and written in small enough chunks that, while not compromising performance, ensure a piece of data is never in a half-usable state; that piece of data is either **normal** or **corrupted**, no in-between, even when an outage or a crash happens mid-write.

### In-memory store

The `KV` class holds all live key-value pairs in an `std::unordered_map`. Every `Get` is a hash map lookup in O(1), no disk access. Reads are fast because they never touch the filesystem after the initial load.

### The append-only log

Writes never modify existing data on disk. Instead, every `Set` and `Del` appends a new record to the end of a log file. This design is called an **append-only log** or **write-ahead log**.

```txt
disk:  [header][Set k1=v1][Set k2=v2][Del k1][Set k1=v3]
                                                ↑ latest
```

The last record for any key is the truth. A `Del` record is called a **tombstone**: it marks a deleted key without erasing earlier records.

Writing to the end of a file is the fastest possible disk operation. It also makes crash recovery straightforward: a partially written record at the tail (from a crash mid-write) is detected by its checksum and silently skipped on the next open. A report or warning system will be implemented to handle this explicitly.

### Recovery on open

When `KV::Open` is called, it replays the entire log from the beginning to rebuild the in-memory map:

```txt
read [Set k1=v1]  →  mem[k1] = v1
read [Set k2=v2]  →  mem[k2] = v2
read [Del k1]     →  mem.erase(k1)
read [Set k1=v3]  →  mem[k1] = v3
replay complete   →  mem = { k2:v2, k1:v3 }
```

After replay the in-memory state is identical to what it was before the program closed. This is why the log is the source of truth, the map is just a cache of it.

### Durability

After every write, `fsync` is called on the file descriptor. This tells the OS to flush its own internal buffers to the physical storage device. Without `fsync`, the OS might hold the data in memory for seconds before writing it, creating potential data loss in a power failure. With `fsync`, each record is durable before `Set` or `Del` returns.

### Entry wire format

Each record on disk is called an Entry and stored as a contiguous sequence of bytes. The following is the layout of the bytes:

```txt
[ checksum(4) | key size(4) | value size(4) | flag(1) | key | val ]
```

All integers are little-endian. `checksum` is a computed CRC32 hash over everything from `key size` onward. `flag = 1` marks a tombstone; tombstones omit the value bytes.

The log file itself starts with a 6-byte header:

```txt
[ magic(4) | format_version(2) ]
```

`magic` is used to recognize if the log is generated by the database and not just some random bytes. `format_version` is used to recognize logs that may not have compatible format with the current version of the database.

### Architecture layout

The headers are ordered/included in one-direction.

```txt
KV              → public API: Get, Set, Del
 └── Log        → append entries, replay on open
      └── Entry → encode/decode the binary wire format
           └── FileHandle   → C++ Class wrapper around a raw OS file descriptor
                └── platform_unix / platform_windows  → OS-specific I/O calls
```

---

## API Reference

All keys and values are `bytes` (`std::vector<std::byte>`), making the store binary-safe, any sequence of bytes is a valid key or value, including those containing null bytes or non-UTF-8 content.

Errors are returned as `std::error_code`. A default-constructed (falsy) `std::error_code` means success. Check with `if (err) { ... }`.

### `KV::KV(const std::string &path)`

Constructs a KV instance for the log file at `path`. Does not open the file.

```cpp
KV kv("/var/data/mydb.kvlog");
```

### `std::error_code KV::open()`

Opens or creates the log file and replays it to rebuild in-memory state. Must be called before any other operation. Safe to call on an already-open instance and returns immediately without re-replaying.

```cpp
if (auto err = kv.open(); err) {
    std::cerr << "open failed: " << err.message() << "\n";
}
```

### `std::error_code KV::close()`

Closes the underlying file handle. The destructor closes silently if this is not called, but calling it explicitly lets you observe and handle close errors.

```cpp
if (auto err = kv.close(); err) {
    std::cerr << "close failed: " << err.message() << "\n";
}
```

### `KV::get(key)`

Retrieves the value for `key`. Returns `std::nullopt` if the key does not exist. Never touches the disk and reads from the in-memory map only.

```cpp
auto [val, err] = kv.get(to_bytes("username"));
if (err) { /* handle */ }
if (val.has_value()) {
    // key exists
} else {
    // key not found
}
```

### `KV::set(key, val)`

Inserts or updates `key` with `val`. Appends to the log and fsyncs before updating the in-memory map. Returns `true` if the key was newly inserted or its value changed, `false` if the value was identical to what was already stored.

```cpp
auto [changed, err] = kv.set(to_bytes("username"), to_bytes("aris"));
if (err) { /* handle */ }
```

### `KV::del(key)`

Removes `key` by appending a tombstone entry. Fsyncs before updating memory. Returns `true` if the key existed and was removed, `false` if the key was not present.

```cpp
auto [existed, err] = kv.del(to_bytes("username"));
if (err) { /* handle */ }
```

---

## Getting Started

### Prerequisites

- GCC 13+ or Clang 17+ with C++23 support.
- CMake 3.20+.
- Doxygen (optional, for documentation).
- Valgrind (optional, for memory checks).

Google Test is needed but already included in CMake, no need to manually download anything.

A bash command to install everything:

```bash
sudo apt install build-essential cmake doxygen valgrind
```

### Build and Test

Configure (once everytime you make changes to the `CMakeLists.txt`):

```bash
cmake -S . -B build
```

Build:

```bash
cmake --build build
```

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

Re-run only failed tests:

```bash
ctest --test-dir build --rerun-failed --output-on-failure
```

Memory check:

```bash
cmake --build build --target memcheck
```

Generate docs:

```bash
cmake --build build --target doc
```

Clean (no CMake target exist yet, remove the folder manually):

```bash
rm -rf build/
```

---

## Project Structure

```txt
kvdb/
├── CMakeLists.txt          # Build system
├── Doxyfile                # Doxygen configuration
├── include/
│   ├── version.h.in        # Version template, filled in by CMake
│   ├── types.h             # bytes alias (std::vector<std::byte>)
│   ├── db_error.h          # Custom error codes integrated with std::error_code
│   ├── bit_utils.h         # byteswap, pack_le, unpack_le, crc32 utilities
│   ├── reader.h            # Reader concept
│   ├── platform.h          # Helper `platform_*` functions and platform-agnostic include.
│   ├── platform_unix.h     # FileHandle class definition on POSIX (Linux, macOS)
│   ├── platform_windows.h  # FileHandle class definition on Windows
│   ├── entry.h             # Entry struct: holds data (key, value, and flag)
│   ├── entry_codec.h       # Holds Entry serialization format
│   ├── log.h               # Log: append-only file-backed entry store
│   ├── log_format.h        # On-disk magic number and format version constants
│   └── kv.h                # KV: in-memory store backed by Log
├── src/
│   ├── entry_codec.cpp     # `EntryCodec::encode()`
│   ├── log.cpp             # Log::Open, Write, Read, format header I/O
│   ├── kv.cpp              # KV::Open, Close, Get, Set, Del
│   ├── platform_unix.cpp   # POSIX implementation (Linux, macOS)
│   └── platform_windows.cpp # Win32 implementation
└── test/
    ├── test_utils.h        # BufferReader and other test infrastructure
    └── test_kv.cpp         # GoogleTest unit tests
```

### Note on `version.h`

`include/version.h` is not present in the source tree, it is generated into `build/include/version.h` by CMake at configure time from `version.h.in`. To regenerate it after a version bump in `CMakeLists.txt`:

```bash
cmake -S . -B build
```

### Include hierarchy (low to high)

```txt
types.h   db_error.h   bit_utils.h   reader.h
                    ↓
               platform.h
                    ↓
                entry.h
                    ↓
                 log.h
                    ↓
                  kv.h
```

---

## On-disk Format

Each log file begins with a 6-byte header:

```txt
[ magic(4) | format_version(2) ]
```

Followed by zero or more entries:

```txt
[ checksum(4) | key size(4) | value size(4) | flag(1) | key | val ]
```

All multi-byte integers are little-endian. A `flag` of `1` marks a tombstone (deleted key); tombstones omit the value payload.

---

## License

[MIT License](https://choosealicense.com/licenses/mit/). Free for any usage with no warranty.

---

## Project History

| Version | Date       | Description                                                                                |
|---------|------------|----------------------------------------------------------                                  |
| 0.6.0   | 2026-02-25 | Upgrade the platform to C++23 with some internal changes and add Windows support.          |
| 0.5.0   | 2026-02-24 | Add integrity checks on every entry.                                                       |
| 0.4.0   | 2026-02-23 | Upgrade to C++20 from C++17 with multi-file refactor and add file syncs for durability.    |
| 0.3.0   | 2026-02-22 | Add sequential logging and custom database error codes.                                    |
| 0.2.0   | 2026-02-21 | Add entry serialization with binary wire format.                                           |
| 0.1.0   | 2026-02-20 | Initial Key-Value in-memory store.                                                         |
