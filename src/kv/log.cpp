#include "core/types.h"
#include "core/db_error.h"
#include "core/bit_utils.h"
#include "kv/log.h"
#include "kv/log_format.h"
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

std::error_code Log::open() {
    if (fh_.is_open()) return {};

    // Error handling: File name is a directory instead of file
    if (std::filesystem::exists(filename_) && std::filesystem::is_directory(filename_))
        return make_error_code(std::errc::is_a_directory);

    if (auto err = platform_open_file(filename_, fh_)) return err;

    std::error_code fs_err;
    auto size = std::filesystem::file_size(filename_, fs_err);
    if (fs_err) return fs_err;

    if (size == 0) return write_file_header(fh_);

    if (auto err = platform_seek(fh_, 0, SEEK_SET); err) return err;
    return read_and_validate_file_header(fh_);
}

std::error_code Log::close() {
    return platform_close(fh_);
}

std::error_code Log::write(const Entry &ent) {
    if (auto err = platform_seek(fh_, 0, SEEK_END); err) return err;

    bytes data = EntryCodec::encode(ent);
    if (auto err = platform_write(fh_, std::span<std::byte>(data)); err)
        return err;
    return platform_sync(fh_);
}

ReadResult Log::read() {
    auto result = EntryCodec::decode(fh_);

    // Treat tail corruption as EOF silently, future implementation should have a flag to trigger a warning.
    if (!result.has_value()) {
        auto err = result.error();
        if (err == db_error::bad_checksum || err == db_error::truncated_header || err == db_error::truncated_payload)
            return LogEOF{};

        return std::unexpected(err);
    }

    return std::visit(
        []<typename T>(T &&val) -> ReadResult {
            if constexpr (std::is_same_v<std::decay_t<T>, Entry>)
                return std::forward<T>(val);
            else
                return LogEOF{};
        },
        std::move(result.value())
    );
}

std::error_code Log::seek_to_first_entry() {
    return platform_seek(fh_, log_format::HEADER_SIZE, SEEK_SET);
}

Log::~Log() {
    if (fh_.is_open()) platform_close(fh_);
}
