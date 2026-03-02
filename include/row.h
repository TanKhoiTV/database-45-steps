// include/row.h
#pragma once

#include "cell.h"
#include <vector>
#include <string>

using Row = std::vector<Cell>;

struct ColumnHeader {
    std::string name;
    Cell::Type type;
};

struct Schema {
    uint32_t id;
    std::string name;
    std::vector<ColumnHeader> cols;
    std::vector<size_t> pkey;

    bool is_pkey(size_t col_idx) const noexcept {
        return std::ranges::contains(pkey, col_idx);
    }
};
