#pragma once

#include "table/cell.h"
#include <vector>
#include <string>


struct ColumnHeader {
    std::string name_;
    Cell::Type type_;
};

struct Schema {
    uint32_t id_;
    std::string name_;
    std::vector<ColumnHeader> cols_;
    std::vector<size_t> pkey_;
    std::vector<bool> pkey_map_;

    private:

    void compute_metadata() {
        pkey_map_.assign(cols_.size(), false);
        for (auto idx : pkey_) {
            if (idx < pkey_map_.size()) {
                pkey_map_[idx] = true;
            }
        }
    }

    public:

    Schema(uint32_t id, std::string name, std::vector<ColumnHeader> cols, std::vector<size_t> pkey)
        : id_(id), name_(std::move(name)), cols_(std::move(cols)), pkey_(std::move(pkey)) {
            compute_metadata();
        }

    bool is_pkey(size_t col_idx) const noexcept {
        if (col_idx >= pkey_map_.size()) return false;
        return pkey_map_[col_idx];
    }
};
