#include "log.h"
#include "log_format.h"
#include "db_error.h"
#include "bit_utils.h"
#include <filesystem>


static std::error_code write_file_header(FileHandle &fh) {
    std::array<std::byte, log_format::HEADER_SIZE> header;

    auto magic      = pack_le<uint32_t>(log_format::MAGIC);
    auto version    = pack_le<uint16_t>(log_format::FORMAT_VERSION);

    std::copy(magic.begin(), magic.end(), header.begin());
    std::copy(version.begin(), version.end(), header.begin() + 4);

    return platform_write(fh, std::span<const std::byte>(header));
}

static std::error_code read_and_validate_file_header(FileHandle &fh) {
    std::array<std::byte, log_format::HEADER_SIZE> header;
    size_t bytes_read = 0;

    if (auto err = platform_read(fh, std::span<std::byte>(header), bytes_read); err)
        return err;
    if (bytes_read < log_format::HEADER_SIZE)
        return db_error::truncated_header;

    uint32_t magic = unpack_le<uint32_t>(std::span<const std::byte>(header).subspan<0, 4>());
    uint16_t version = unpack_le<uint16_t>(std::span<const std::byte>(header).subspan<4, 2>());

    if (magic != log_format::MAGIC)
        return db_error::bad_magic;
    if (version > log_format::FORMAT_VERSION)
        return db_error::unsupported_version;
    
    return {};
}

std::error_code Log::Open() {
    if (fh.is_open()) return {};

    // Error handling: File name is a directory instead of file
    if (std::filesystem::exists(filename) && std::filesystem::is_directory(filename))
        return make_error_code(std::errc::is_a_directory);

    if (auto err = platform_open_file(filename, fh)) return err;

    std::error_code fs_err;
    auto size = std::filesystem::file_size(filename, fs_err);
    if (fs_err) return fs_err;

    if (size == 0) return write_file_header(fh);

    if (auto err = platform_seek(fh, 0, SEEK_SET); err) return err;
    return read_and_validate_file_header(fh);
}

std::error_code Log::Close() {
    return platform_close(fh);
}

std::error_code Log::Write(const Entry &ent) {
    if (auto err = platform_seek(fh, 0, SEEK_END); err) return err;

    bytes data = ent.Encode();
    if (auto err = platform_write(fh, std::span<std::byte>(data)); err)
        return err;
    return platform_sync(fh);
}

std::pair<Log::ReadResult, std::error_code> Log::Read(Entry &ent) {
    auto [res, err] = ent.Decode(fh);
    if (err) return { Log::ReadResult::fail, err };
    return { res == Entry::DecodeResult::eof ? Log::ReadResult::eof : Log::ReadResult::ok, {} };
}

std::error_code Log::SeekToFirstEntry() {
    return platform_seek(fh, log_format::HEADER_SIZE, SEEK_SET);
}

Log::~Log() {
    if (fh.is_open()) platform_close(fh);
}