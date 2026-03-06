#pragma once

#include <cstdint>
#include <cstddef>

/**
 * @brief On-disk log file header written once at file creation.
 * 
 * A file that does not begin with MAGIC is not a valid kvdb log.
 * FORMAT_VERSION must be bumped whenever the Entry wire format 
 * changes in a way that is not backward-compatible.
 */
namespace log_format {
    /**
     * @brief "KVDB" as a 4-byte magic number -- rules out 
     * accidental reads of arbitrary binary files
     */
    inline constexpr uint32_t   MAGIC           = 0x4B564442;
    inline constexpr uint16_t   FORMAT_VERSION  = 2;

    inline constexpr size_t     HEADER_SIZE     = 6;
}