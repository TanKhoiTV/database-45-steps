#include "log.h"
#include "db_error.h"
#include <filesystem>

explicit Log::Log(std::string fname) : filename(std::move(fname)) {}

std::error_code Log::Open() {
    if (fh.is_open()) return {};

    // Error handling: File name is a directory instead of file
    if (std::filesystem::exists(filename) && std::filesystem::is_directory(filename))
        return make_error_code(std::errc::is_a_directory);

    return platform_open_file(filename, fh);
}

std::error_code Log::Close() {
    return platform_close(fh);
}

std::error_code Log::Write(const Entry &ent) {
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

std::error_code Log::SeekToStart() {
    return platform_seek(fh, 0, SEEK_SET);
}

Log::~Log() {
    if (fh.is_open()) platform_close(fh);
}