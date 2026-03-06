#include <gtest/gtest.h>
#include <filesystem>
#include <iomanip>
#include "kv.h"
#include "table.h"
#include "row.h"
#include "test_utils.h"
#include "schema.h"
#include "schema_codec.h"

const std::string test_db   = (std::filesystem::temp_directory_path() / "kvdb_table_test").string();

// ---------------------------------------------------------------------------
// Fixture — opens and closes data_store + schema_store around each test
// ---------------------------------------------------------------------------

class TableTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::remove(test_db);
        ASSERT_FALSE(kv.open())   << "Failed to open KV";
    }

    void TearDown() override {
        kv.close();
        std::filesystem::remove(test_db);
    }

    KeyValue kv{test_db};
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static Schema make_link_schema() {
    return Schema(
        1,
        "link",
        {
            { "time", Cell::Type::i64 },
            { "src",  Cell::Type::str },
            { "dst",  Cell::Type::str },
        },
        { 1, 2 }   // (src, dst)
    );
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_F(TableTest, CreateAndOpen) {
    // Create succeeds
    auto created = Table::create(kv, make_link_schema());
    ASSERT_TRUE(created.has_value()) << created.error().message();

    // Open succeeds after create
    auto opened = Table::open(kv, "link");
    ASSERT_TRUE(opened.has_value()) << opened.error().message();

    // Schema round-trips correctly
    EXPECT_EQ(opened.value().schema().name_,      "link");
    EXPECT_EQ(opened.value().schema().cols_.size(), 3u);
    EXPECT_EQ(opened.value().schema().pkey_,      (std::vector<size_t>{1, 2}));

    // Creating the same table again fails
    auto duplicate = Table::create(kv, make_link_schema());
    ASSERT_FALSE(duplicate.has_value());
    EXPECT_EQ(duplicate.error(), make_error_code(db_error::table_already_exists));

    // Opening a nonexistent table fails
    auto missing = Table::open(kv, "nonexistent");
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error(), make_error_code(db_error::table_not_found));
}

TEST_F(TableTest, OpenOrCreate) {
    // First call creates
    auto first = Table::open_or_create(kv, make_link_schema());
    ASSERT_TRUE(first.has_value()) << first.error().message();

    // Second call opens — same schema
    auto second = Table::open_or_create(kv, make_link_schema());
    ASSERT_TRUE(second.has_value()) << second.error().message();
    EXPECT_EQ(first.value().schema().id_, second.value().schema().id_);
}

TEST_F(TableTest, SelectInsertUpdateDelete) {
    auto result = Table::create(kv, make_link_schema());
    ASSERT_TRUE(result.has_value()) << result.error().message();
    Table &table = result.value();

    // Build a row: time=123, src="a", dst="b"
    Row row = table.new_row();
    row[0] = Cell::make_i64(123);
    row[1] = Cell::make_str(to_bytes("a"));
    row[2] = Cell::make_str(to_bytes("b"));

    // 1. Select before insert — not found
    Row query = table.new_row();
    query[1] = Cell::make_str(to_bytes("a"));
    query[2] = Cell::make_str(to_bytes("b"));

    auto sel1 = table.Select(query);
    ASSERT_TRUE(sel1.has_value()) << sel1.error().message();
    EXPECT_FALSE(sel1.value());

    // 2. Insert — succeeds
    auto ins = table.Insert(row);
    ASSERT_TRUE(ins.has_value()) << ins.error().message();
    EXPECT_TRUE(ins.value());

    // 3. Insert duplicate — fails silently (returns false, no error)
    auto ins_dup = table.Insert(row);
    ASSERT_TRUE(ins_dup.has_value()) << ins_dup.error().message();
    EXPECT_FALSE(ins_dup.value());

    // 4. Select after insert — found, non-pk columns populated
    Row out = table.new_row();
    out[1] = Cell::make_str(to_bytes("a"));
    out[2] = Cell::make_str(to_bytes("b"));

    auto sel2 = table.Select(out);
    ASSERT_TRUE(sel2.has_value()) << sel2.error().message();
    ASSERT_TRUE(sel2.value());
    EXPECT_EQ(out[0].as_i64(), 123);    // time column populated by Select
    EXPECT_EQ(out[1].as_str(), to_bytes("a"));
    EXPECT_EQ(out[2].as_str(), to_bytes("b"));

    // 5. Update — change time to 456
    row[0] = Cell::make_i64(456);
    auto upd = table.Update(row);
    ASSERT_TRUE(upd.has_value()) << upd.error().message();
    EXPECT_TRUE(upd.value());

    // 6. Select after update — reflects new value
    Row out2 = table.new_row();
    out2[1] = Cell::make_str(to_bytes("a"));
    out2[2] = Cell::make_str(to_bytes("b"));

    auto sel3 = table.Select(out2);
    ASSERT_TRUE(sel3.has_value()) << sel3.error().message();
    ASSERT_TRUE(sel3.value());
    EXPECT_EQ(out2[0].as_i64(), 456);

    // 7. Update nonexistent key — fails silently
    Row ghost = table.new_row();
    ghost[0] = Cell::make_i64(999);
    ghost[1] = Cell::make_str(to_bytes("x"));
    ghost[2] = Cell::make_str(to_bytes("y"));

    auto upd_ghost = table.Update(ghost);
    ASSERT_TRUE(upd_ghost.has_value()) << upd_ghost.error().message();
    EXPECT_FALSE(upd_ghost.value());

    // 8. Delete — succeeds
    auto del = table.Delete(row);
    ASSERT_TRUE(del.has_value()) << del.error().message();
    EXPECT_TRUE(del.value());

    // 9. Select after delete — not found
    Row out3 = table.new_row();
    out3[1] = Cell::make_str(to_bytes("a"));
    out3[2] = Cell::make_str(to_bytes("b"));

    auto sel4 = table.Select(out3);
    ASSERT_TRUE(sel4.has_value()) << sel4.error().message();
    EXPECT_FALSE(sel4.value());

    // 10. Delete already deleted — fails silently
    auto del2 = table.Delete(row);
    ASSERT_TRUE(del2.has_value()) << del2.error().message();
    EXPECT_FALSE(del2.value());
}

TEST_F(TableTest, UpsertBehavior) {
    auto result = Table::create(kv, make_link_schema());
    ASSERT_TRUE(result.has_value()) << result.error().message();
    Table &table = result.value();

    Row row = table.new_row();
    row[0] = Cell::make_i64(1);
    row[1] = Cell::make_str(to_bytes("a"));
    row[2] = Cell::make_str(to_bytes("b"));

    // Upsert on nonexistent key — inserts
    auto ups1 = table.Upsert(row);
    ASSERT_TRUE(ups1.has_value()) << ups1.error().message();
    EXPECT_TRUE(ups1.value());

    // Upsert with same value — no change
    auto ups2 = table.Upsert(row);
    ASSERT_TRUE(ups2.has_value()) << ups2.error().message();
    EXPECT_FALSE(ups2.value());

    // Upsert with changed value — updates
    row[0] = Cell::make_i64(999);
    auto ups3 = table.Upsert(row);
    ASSERT_TRUE(ups3.has_value()) << ups3.error().message();
    EXPECT_TRUE(ups3.value());
}

TEST_F(TableTest, Persistence) {
    // Create and populate
    {
        auto result = Table::create(kv, make_link_schema());
        ASSERT_TRUE(result.has_value()) << result.error().message();
        Table &table = result.value();

        Row row = table.new_row();
        row[0] = Cell::make_i64(123);
        row[1] = Cell::make_str(to_bytes("a"));
        row[2] = Cell::make_str(to_bytes("b"));

        ASSERT_TRUE(table.Insert(row).value());
    }

    // Close and reopen stores
    ASSERT_FALSE(kv.close());
    ASSERT_FALSE(kv.open());

    // Verify schema round-trips correctly before touching the table
    auto reopened = Table::open(kv, "link");
    ASSERT_TRUE(reopened.has_value()) << reopened.error().message();

    const Schema &s = reopened.value().schema();
    std::cerr << "schema id:    " << s.id_   << "\n";
    std::cerr << "schema name:  " << s.name_ << "\n";
    std::cerr << "cols count:   " << s.cols_.size() << "\n";
    for (size_t i = 0; i < s.cols_.size(); ++i)
        std::cerr << "  col[" << i << "] name=" << s.cols_[i].name_
                << " type=" << static_cast<int>(s.cols_[i].type_) << "\n";
    std::cerr << "pkey count:   " << s.pkey_.size() << "\n";
    for (size_t i = 0; i < s.pkey_.size(); ++i)
    std::cerr << "  pkey[" << i << "] = " << s.pkey_[i] << "\n";

    // Reopen table — schema survives
    auto reopened2 = Table::open(kv, "link");
    ASSERT_TRUE(reopened2.has_value()) << reopened2.error().message();
    Table &table = reopened2.value();

    // Data survives
    Row query = table.new_row();
    query[1] = Cell::make_str(to_bytes("a"));
    query[2] = Cell::make_str(to_bytes("b"));

    // Dump the encoded key before Select
    auto key = RowCodec::encode_key(s, query);
    if (key.has_value()) {
        std::cerr << "encoded key (" << key.value().size() << " bytes): ";
        for (auto b : key.value())
            std::cerr << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(b) << " ";
        std::cerr << std::dec << "\n";
    }

    // Dump the raw value stored in KV
    auto raw = kv.get(key.value());
    if (raw.has_value() && raw.value().has_value()) {
        std::cerr << "raw value (" << raw.value().value().size() << " bytes): ";
        for (auto b : raw.value().value())
            std::cerr << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(b) << " ";
        std::cerr << std::dec << "\n";
    }

    auto sel = table.Select(query);
    ASSERT_TRUE(sel.has_value()) << sel.error().message();
    ASSERT_TRUE(sel.value());
    EXPECT_EQ(query[0].as_i64(), 123);
}
