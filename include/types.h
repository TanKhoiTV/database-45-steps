#pragma once

#include <vector>   // std::vector
#include <cstddef>  // std::byte

/**
 * @brief Contiguous buffer of raw bytes.
 * Used as the primary data type for keys and values throughout the database.
 */
using bytes = std::vector<std::byte>;