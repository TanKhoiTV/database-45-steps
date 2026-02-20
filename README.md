Creating a Database from scratch in 45 steps
=========================

Inspired by Trial of Code's [Code a database in 45 steps (Go)](https://trialofcode.org/database/), this repository is an attempt at coding a database from scratch with minimal dependencies with C++.

**Note**: The `cpp/` folder is the root of the project. At the same folder level as `cpp`, there is an untracked folder (which is invisible to you) containing Go files from Trial of Code.

## Features

- C++17 Standards: Uses `std::byte`, `std::optional`, and `std::string_view` for memory safety.
- Binary Safe: Supports raw byte vectors as keys and values.
- Doxygen Support: Documented source code for automated manual generation.

---

## Getting started

### Usage

You only need the `database.h` header. If you plan to run commands within this repository, proceed from here.

### Prerequisites

- A C++17 compatible GCC compiler.
- `make`.
- Google's testing library `libgtest-dev`.
- `Doxygen` to generate documentation.

An example Bash command:

    sudo apt install build-essential libgtest-dev doxygen

### Testing

Build:

    make 

Run:

    make test

Reset (remove the executable):

    make clean

### Generate docs

Generate:

    make doc

## Project Structure

With `cpp/` as root:

- `database.h`: Core library (header-only).
- `test_kv.cpp`: Unit tests using GoogleTest.
- `Makefile`: Build automation for testing and documentation.
- `Doxyfile`: Configuration for Doxygen.

## License

[MIT License](https://choosealicense.com/licenses/mit/).

## Project History

### Log-based KV

2026-02-20: Project created. Added KV interface.