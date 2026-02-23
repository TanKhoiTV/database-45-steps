#pragma once

#include <system_error>     // std::error_code
#include <string>           // std::string

struct FileHandle;

std::error_code platform_open_file(const std::string &path, FileHandle &out);
std::error_code platform_write(FileHandle &fh, const void *buf, size_t len);
std::error_code platform_read(FileHandle &fh, void *buf, size_t len, size_t &bytes_read);
std::error_code platform_seek(FileHandle &fh, long offset, int whence);
std::error_code platform_sync(FileHandle &fh);
std::error_code platform_close(FileHandle &fh);
bool            platform_eof(const FileHandle &fh);

