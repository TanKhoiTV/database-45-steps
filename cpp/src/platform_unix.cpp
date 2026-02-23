#include "platform.h"
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

struct FileHandle {
    int fd = -1;
    bool at_eof = false;
};

/** @brief Constructs an `std::error_code` from the current value of `errno`. */
inline std::error_code errno_to_error() {
    return std::make_error_code(static_cast<std::errc>(errno));
}

std::error_code platform_open_file(const std::string &path, FileHandle &out) {
    int fd = ::open(path.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd < 0) return errno_to_error();

    out.fd = fd;

    std::string dir = path.substr(0, path.find_last_of('/'));
    if (dir.empty()) dir = ".";

    int dirfd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY);
    if (dirfd >= 0) {
        ::fsync(dirfd);
        ::close(dirfd);
    }

    return {};
}

std::error_code platform_write(FileHandle &fh, const void *buf, size_t len) {
    ssize_t written = ::write(fh.fd, buf, len);
    if (written < 0 || static_cast<size_t>(written) != len)
        return errno_to_error();
    return {};
}

std::error_code platform_read(FileHandle &fh, void *buf, size_t len, size_t &bytes_read) {
    ssize_t n = ::read(fh.fd, buf, len);
    if (n < 0) return errno_to_error();
    bytes_read = static_cast<size_t>(n);
    fh.at_eof = (n == 0);
    return {};
}

std::error_code platform_seek(FileHandle &fh, long offset, int whence) {
    if (::lseek(fh.fd, offset, whence) < 0)
        return errno_to_error();
    fh.at_eof = false;
    return {};
}

std::error_code platform_sync(FileHandle &fh) {
    if (::fsync(fh.fd) < 0) return errno_to_error();
    return {};
}

std::error_code platform_close(FileHandle &fh) {
    if (fh.fd < 0) return {};
    int rc = ::close(fh.fd);
    fh.fd = -1;
    return rc < 0 ? errno_to_error() : std::error_code{};
}

bool platform_eof(const FileHandle &fh) { return fh.at_eof; }