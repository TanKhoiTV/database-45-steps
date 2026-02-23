#pragma once
    
#include <system_error>     // std::error_code
#include <string>           // std::string
#include <span>             // std::span

class FileHandle {
    private:

    int fd_ = -1;
    bool at_eof_ = false;

    void swap(FileHandle &other) noexcept;

    public:

    FileHandle() = default;

    /** @note Closes silently, errors are unrecoverable; prefer explicit closing. */
    ~FileHandle();

    // No copying - two handles owning the same file descriptor would double-close
    FileHandle(const FileHandle &) = delete;
    FileHandle &operator=(const FileHandle &) = delete;

    // Move: transfer ownership, leave source in a safe empty state
    FileHandle(FileHandle &&other) noexcept;
    FileHandle &operator=(FileHandle &&other) noexcept;

    bool is_open()  const noexcept { return fd_ >= 0; }
    bool at_eof()   const noexcept { return at_eof_; }

    private:

    friend std::error_code platform_open_file(const std::string &path, FileHandle &out);
    friend std::error_code platform_write(FileHandle &fh, std::span<const std::byte> buf);
    friend std::error_code platform_read(FileHandle &fh, std::span<std::byte> buf, size_t &bytes_read);
    friend std::error_code platform_seek(FileHandle &fh, long offset, int whence);
    friend std::error_code platform_sync(FileHandle &fh);
    friend std::error_code platform_close(FileHandle &fh);
};

std::error_code platform_open_file(const std::string &path, FileHandle &out);
std::error_code platform_write(FileHandle &fh, std::span<const std::byte> buf);
std::error_code platform_read(FileHandle &fh, std::span<std::byte> buf, size_t &bytes_read);
std::error_code platform_seek(FileHandle &fh, long offset, int whence);
std::error_code platform_sync(FileHandle &fh);
std::error_code platform_close(FileHandle &fh);
