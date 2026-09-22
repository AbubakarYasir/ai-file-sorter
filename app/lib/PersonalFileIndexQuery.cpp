#include "PersonalFileIndexQuery.hpp"

#include <memory>
#include <utility>

#include <sqlite3.h>

namespace {

constexpr int kEntryTypeFile = 0;
constexpr int kEntryTypeDirectory = 1;
constexpr int kEntryTypeProtectedProject = 2;

struct StatementDeleter {
    void operator()(sqlite3_stmt* statement) const noexcept
    {
        if (statement) {
            sqlite3_finalize(statement);
        }
    }
};

using StatementPtr = std::unique_ptr<sqlite3_stmt, StatementDeleter>;

StatementPtr prepare(sqlite3* db, const char* sql)
{
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) != SQLITE_OK) {
        return {};
    }
    return StatementPtr(raw);
}

bool table_exists(sqlite3* db, const char* name)
{
    auto statement = prepare(
        db,
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=? LIMIT 1;");
    if (!statement) {
        return false;
    }
    sqlite3_bind_text(statement.get(), 1, name, -1, SQLITE_TRANSIENT);
    return sqlite3_step(statement.get()) == SQLITE_ROW;
}

bool column_exists(sqlite3* db, const char* table, const char* column)
{
    const std::string sql = std::string("PRAGMA table_info(") + table + ");";
    auto statement = prepare(db, sql.c_str());
    if (!statement) {
        return false;
    }
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
        const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 1));
        if (name && std::string(name) == column) {
            return true;
        }
    }
    return false;
}

std::optional<std::uint64_t> scalar_uint64(sqlite3* db, const char* sql)
{
    auto statement = prepare(db, sql);
    if (!statement || sqlite3_step(statement.get()) != SQLITE_ROW) {
        return std::nullopt;
    }
    const sqlite3_int64 value = sqlite3_column_int64(statement.get(), 0);
    if (value < 0) {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(value);
}

} // namespace

struct PersonalFileIndexQuery::Impl {
    sqlite3* db{nullptr};

    ~Impl()
    {
        if (db) {
            sqlite3_close(db);
        }
    }
};

PersonalFileIndexQuery::PersonalFileIndexQuery(std::string database_path)
    : database_path_(std::move(database_path))
{
    impl_ = new Impl();

    if (sqlite3_open_v2(
            database_path_.c_str(),
            &impl_->db,
            SQLITE_OPEN_READONLY,
            nullptr) != SQLITE_OK) {
        delete impl_;
        impl_ = nullptr;
    }
}

PersonalFileIndexQuery::~PersonalFileIndexQuery()
{
    delete impl_;
}

bool PersonalFileIndexQuery::is_open() const noexcept
{
    return impl_ && impl_->db;
}

const std::string& PersonalFileIndexQuery::database_path() const noexcept
{
    return database_path_;
}

std::optional<PersonalFileIndexStats> PersonalFileIndexQuery::stats() const
{
    if (!is_open() ||
        !table_exists(impl_->db, "personal_index_entries") ||
        !table_exists(impl_->db, "personal_index_scan_runs") ||
        !column_exists(impl_->db, "personal_index_entries", "observation_state") ||
        !column_exists(impl_->db, "personal_index_entries", "policy_state")) {
        return std::nullopt;
    }

    PersonalFileIndexStats result;

    const auto total_entries = scalar_uint64(
        impl_->db,
        "SELECT COUNT(*) FROM personal_index_entries;");
    const auto present_entries = scalar_uint64(
        impl_->db,
        "SELECT COUNT(*) FROM personal_index_entries WHERE observation_state='present';");
    const auto present_files = scalar_uint64(
        impl_->db,
        "SELECT COUNT(*) FROM personal_index_entries "
        "WHERE observation_state='present' AND entry_type=0;");
    const auto present_directories = scalar_uint64(
        impl_->db,
        "SELECT COUNT(*) FROM personal_index_entries "
        "WHERE observation_state='present' AND entry_type=1;");
    const auto protected_projects = scalar_uint64(
        impl_->db,
        "SELECT COUNT(*) FROM personal_index_entries "
        "WHERE observation_state='present' AND entry_type=2;");
    const auto missing_entries = scalar_uint64(
        impl_->db,
        "SELECT COUNT(*) FROM personal_index_entries WHERE observation_state='missing';");
    const auto unknown_entries = scalar_uint64(
        impl_->db,
        "SELECT COUNT(*) FROM personal_index_entries WHERE observation_state='unknown';");
    const auto policy_skipped_entries = scalar_uint64(
        impl_->db,
        "SELECT COUNT(*) FROM personal_index_entries WHERE policy_state<>'included';");
    const auto hashed_files = scalar_uint64(
        impl_->db,
        "SELECT COUNT(*) FROM personal_index_entries "
        "WHERE entry_type=0 AND hash_state='complete' AND sha256 IS NOT NULL;");
    const auto present_bytes = scalar_uint64(
        impl_->db,
        "SELECT COALESCE(SUM(size_bytes), 0) FROM personal_index_entries "
        "WHERE observation_state='present' AND entry_type=0;");

    if (!total_entries || !present_entries || !present_files ||
        !present_directories || !protected_projects || !missing_entries ||
        !unknown_entries || !policy_skipped_entries || !hashed_files || !present_bytes) {
        return std::nullopt;
    }

    result.total_entries = *total_entries;
    result.present_entries = *present_entries;
    result.present_files = *present_files;
    result.present_directories = *present_directories;
    result.present_protected_projects = *protected_projects;
    result.missing_entries = *missing_entries;
    result.stale_entries = *missing_entries;
    result.unknown_entries = *unknown_entries;
    result.policy_skipped_entries = *policy_skipped_entries;
    result.hashed_files = *hashed_files;
    result.present_bytes = *present_bytes;

    auto latest_run = prepare(
        impl_->db,
        "SELECT id, status, errors FROM personal_index_scan_runs "
        "ORDER BY id DESC LIMIT 1;");
    if (!latest_run) {
        return std::nullopt;
    }

    const int step = sqlite3_step(latest_run.get());
    if (step == SQLITE_DONE) {
        return result;
    }
    if (step != SQLITE_ROW) {
        return std::nullopt;
    }

    result.latest_run_id = sqlite3_column_int64(latest_run.get(), 0);
    const auto* status = reinterpret_cast<const char*>(
        sqlite3_column_text(latest_run.get(), 1));
    result.latest_run_status = status ? status : "";

    const sqlite3_int64 latest_errors = sqlite3_column_int64(latest_run.get(), 2);
    if (latest_errors < 0) {
        return std::nullopt;
    }
    result.latest_run_errors = static_cast<std::uint64_t>(latest_errors);

    return result;
}
