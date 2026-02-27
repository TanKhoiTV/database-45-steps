#include "cell_codec.h"
#include "bit_utils.h"
#include "db_error.h"
#include <cstddef>
#include <utility>

template<class... Ts> struct overloads : Ts... { using Ts::operator()...; };

std::error_code CellCodec::encode(const Cell &c, Cell::Type expected, bytes &out) {
    return std::visit(overloads{
        [&](std::monostate) -> std::error_code {
            if (expected != Cell::Type::no_type) return db_error::type_mismatch;
            out.push_back(null_byte);
            return {};
        },
        [&](Cell::I64Type val) -> std::error_code {
            if (expected != Cell::Type::i64) return db_error::type_mismatch;
            auto val_bytes = pack_le<Cell::I64Type>(val);
            out.insert(out.end(), val_bytes.begin(), val_bytes.end());
            return {};
        },
        [&](const Cell::StrType &val) -> std::error_code {
            if (expected != Cell::Type::str) return db_error::type_mismatch;
            auto len = static_cast<uint32_t>(val.size());
            auto len_bytes = pack_le<uint32_t>(len);
            out.insert(out.end(), len_bytes.begin(), len_bytes.end());
            out.insert(out.end(), val.begin(), val.end());
            return {};
        }
    }, c.value_);
}

std::expected<Cell, std::error_code> CellCodec::decode(std::span<const std::byte> &buf, Cell::Type t) {
    switch (t) {
        case Cell::Type::no_type: {
            buf = buf.subspan<1>();
            if (buf.empty()) {
                return std::unexpected(db_error::expect_more_data);
            }
            if (buf[0] != null_byte) {
                return std::unexpected(std::make_error_code(std::errc::illegal_byte_sequence));
            }
            return Cell::make_empty();
        }
        case Cell::Type::i64: {
            if (buf.size() < sizeof(Cell::I64Type)) {
                return std::unexpected(db_error::expect_more_data);
            }
            auto val = unpack_le<Cell::I64Type>(buf.first<sizeof(Cell::I64Type)>());
            buf = buf.subspan<sizeof(Cell::I64Type)>();
            return Cell::make_i64(val);
        }
        case Cell::Type::str: {
            constexpr auto len_byte_size = sizeof(uint32_t);
            if (buf.size() < len_byte_size) {
                return std::unexpected(db_error::expect_more_data);
            }
            auto len = unpack_le<uint32_t>(buf.first<len_byte_size>());
            if (buf.size() < sizeof(uint32_t) + len) {
                return std::unexpected(db_error::expect_more_data);
            }
            buf = buf.subspan(len_byte_size);
            bytes data(buf.begin(), buf.begin() + len);
            buf = buf.subspan(len);

            return Cell::make_str(std::move(data));
        }
        default: std::unreachable();
    }
}
