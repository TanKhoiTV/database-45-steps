# Creating a Database from Scratch in 45 Steps

Inspired by Trial of Code's [Code a database in 45 steps (Go)](https://trialofcode.org/database/),
this is a C++ adaptation built for self-learning. The goal is to closely mirror the Go
implementation while using idiomatic modern C++ in place of Go-specific idioms.

## Features

- **C++20**: Uses `std::span`, `std::bit_cast`, `std::endian`, and concepts for
  type-safe, portable code.
- **Binary safe**: Raw `std::byte` vectors as keys and values throughout.
- **Durable writes**: Every `Set` and `Del` call fsyncs to disk before returning,
  mirroring Go's `os.File.Sync()`.
- **Platform abstraction**: POSIX and Windows file I/O (to be implemented) are isolated behind a
  `FileHandle` RAII class and a set of `platform_*` functions, selected at build time.
- **File format versioning**: Log files begin with a magic number and format version
  header, so format mismatches are detected on open rather than producing silent corruption.
- **Reader concept**: `Entry::Decode` is generic over any type satisfying the `Reader`
  concept, enabling in-memory decoding in tests without touching the filesystem.
- **Doxygen support**: Documented source for automated documentation generation.

## Limitations

- **Not thread-safe**: Do not share a `KV` instance across threads or run multiple
  instances against the same file.
- **Append-only**: No compaction yet. The log grows indefinitely; deleted keys leave
  tombstones on disk.
- **Sequential access only**: No indexing or range queries beyond full log replay on open.

---

## Getting Started

### Prerequisites

- GCC 13+ or Clang 16+ with C++20 support.
- CMake 3.20+.
- Google Test (`libgtest-dev`).
- Doxygen (optional, for documentation).
- Valgrind (optional, for memory checks).
```bash
sudo apt install build-essential cmake libgtest-dev doxygen valgrind
```

### Build and Test

Configure (once):
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

Clean:
```bash
rm -rf build/
```

---

## Project Structure
```
kvdb/
├── CMakeLists.txt          # Build system — edit this, not anything in build/
├── Doxyfile                # Doxygen configuration
├── include/
│   ├── version.h.in        # Version template, filled in by CMake
│   ├── types.h             # bytes alias (std::vector<std::byte>)
│   ├── db_error.h          # Custom error codes integrated with std::error_code
│   ├── bit_utils.h         # byteswap, pack_le, unpack_le utilities
│   ├── reader.h            # Reader concept
│   ├── platform.h          # FileHandle RAII class + platform_* declarations
│   ├── log_format.h        # On-disk magic number and format version constants
│   ├── entry.h             # Entry struct: encoding, decoding, wire format
│   ├── log.h               # Log: append-only file-backed entry store
│   └── kv.h                # KV: in-memory store backed by Log
├── src/
│   ├── entry.cpp           # Entry::Encode and non-template helpers
│   ├── log.cpp             # Log::Open, Write, Read, format header I/O
│   ├── kv.cpp              # KV::Open, Close, Get, Set, Del
│   ├── platform_unix.cpp   # POSIX implementation (Linux, macOS)
│   └── platform_windows.cpp # Win32 implementation (currently empty)
└── test/
    ├── test_utils.h        # BufferReader and other test infrastructure
    └── test_kv.cpp         # GoogleTest unit tests
```

### Include hierarchy (low to high)
```
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
```
[magic(4) | format_version(2)]
```

Followed by zero or more entries:
```
[klen(4) | vlen(4) | flag(1) | key(klen) | val(vlen)]
```

All multi-byte integers are little-endian. A `flag` of `1` marks a tombstone
(deleted key); tombstones omit the value payload.

---

## License

[MIT License](https://choosealicense.com/licenses/mit/)

---

## Project History

| Version | Date       | Description                                              |
|---------|------------|----------------------------------------------------------|
| 0.4.0   | 2026-02-23 | C++20 upgrade. Multi-file refactor. Platform abstraction via RAII FileHandle. Reader concept. File format header with magic number and version. CMake build system. |
| 0.3.0   | 2026-02-22 | Sequential logging. Custom std::error_code integration.  |
| 0.2.0   | 2026-02-21 | Entry serialization with binary wire format.             |
| 0.1.0   | 2026-02-20 | Initial KV in-memory store.                              |