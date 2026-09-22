#include "PersonalFileIndex.hpp"

#include "ProtectedProjectDetector.hpp"
#include "Utils.hpp"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QString>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

#include <sqlite3.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {

constexpr int kEntryTypeFile = 0;
constexpr int kEntryTypeDirectory = 1;
constexpr int kEntryTypeProtectedProject = 2;
constexpr int kCurrentSchemaVersion = 2;
constexpr std::size_t kHashReadChunkBytes = 1024 * 1024;

constexpr const char* kObservationPresent = "present";
constexpr const char* kPolicyIncluded = "included";
constexpr const char* kPolicyHidden = "hidden";
constexpr const char* kPolicyExcluded = "excluded";
constexpr const char* kPolicySystem = "system";
constexpr const char* kPolicyReparseSkipped = "reparse-skipped";
constexpr const char* kPolicyProtected = "protected";

struct StatementDeleter {
    void operator()(sqlite3_stmt* statement) const noexcept
    {
        if (statement) sqlite3_finalize(statement);
    }
};
using StatementPtr = std::unique_ptr<sqlite3_stmt, StatementDeleter>;

StatementPtr prepare(sqlite3* db, const char* sql)
{
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &raw, nullptr) != SQLITE_OK) return {};
    return StatementPtr(raw);
}

bool execute(sqlite3* db, const char* sql)
{
    char* error = nullptr;
    const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &error);
    if (error) sqlite3_free(error);
    return rc == SQLITE_OK;
}

std::int64_t now_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string lower_ascii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string comparison_name(const std::string& value)
{
#ifdef _WIN32
    return lower_ascii(value);
#else
    return value;
#endif
}

std::unordered_set<std::string> make_exclusion_set(const std::vector<std::string>& exclusions)
{
    std::unordered_set<std::string> result;
    result.reserve(exclusions.size());
    for (const auto& value : exclusions) {
        if (!value.empty()) result.insert(comparison_name(value));
    }
    return result;
}

bool is_hidden(const fs::path& path)
{
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_HIDDEN) != 0;
#else
    const std::string name = path.filename().string();
    return !name.empty() && name.front() == '.';
#endif
}

bool is_reparse_or_symlink(const fs::directory_entry& entry)
{
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(entry.path().c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return true;
    }
#endif
    std::error_code ec;
    return fs::is_symlink(entry.symlink_status(ec)) && !ec;
}

fs::path normalized_root(const fs::path& input)
{
    std::error_code ec;
    fs::path absolute = fs::absolute(input, ec);
    if (ec) absolute = input;
    fs::path canonical = fs::weakly_canonical(absolute, ec);
    return ec ? absolute.lexically_normal() : canonical;
}

std::string path_identity_key(const fs::path& input)
{
    std::string key = Utils::path_to_utf8(normalized_root(input).lexically_normal());
    std::replace(key.begin(), key.end(), '\\', '/');
    while (key.size() > 1 && key.back() == '/') {
#ifdef _WIN32
        if (key.size() == 3 && key[1] == ':') break;
#endif
        key.pop_back();
    }
#ifdef _WIN32
    return QString::fromUtf8(key.c_str()).toCaseFolded().toUtf8().toStdString();
#else
    return key;
#endif
}

bool same_or_descendant_key(const std::string& ancestor, const std::string& candidate)
{
    if (ancestor == candidate) return true;
    if (ancestor.empty() || candidate.size() <= ancestor.size()) return false;
    if (ancestor.back() == '/') return candidate.rfind(ancestor, 0) == 0;
    return candidate.rfind(ancestor + "/", 0) == 0;
}

std::string join_roots(const std::vector<std::string>& roots)
{
    std::ostringstream stream;
    bool first = true;
    for (const auto& root : roots) {
        if (!first) stream << '\n';
        first = false;
        stream << root;
    }
    return stream.str();
}

std::optional<std::string> sha256_file(const fs::path& path)
{
    QFile file(QString::fromUtf8(Utils::path_to_utf8(path).c_str()));
    if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(static_cast<qint64>(kHashReadChunkBytes));
        if (chunk.isEmpty() && file.error() != QFileDevice::NoError) return std::nullopt;
        hash.addData(chunk);
    }
    return hash.result().toHex().toStdString();
}

bool table_exists(sqlite3* db, const char* table)
{
    auto statement = prepare(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=? LIMIT 1;");
    if (!statement) return false;
    sqlite3_bind_text(statement.get(), 1, table, -1, SQLITE_TRANSIENT);
    return sqlite3_step(statement.get()) == SQLITE_ROW;
}

bool column_exists(sqlite3* db, const char* table, const char* column)
{
    const std::string sql = std::string("PRAGMA table_info(") + table + ");";
    auto statement = prepare(db, sql.c_str());
    if (!statement) return false;
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
        const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 1));
        if (name && std::string(name) == column) return true;
    }
    return false;
}

int schema_version(sqlite3* db)
{
    auto statement = prepare(db, "PRAGMA user_version;");
    if (!statement || sqlite3_step(statement.get()) != SQLITE_ROW) return -1;
    return sqlite3_column_int(statement.get(), 0);
}

bool backup_database(sqlite3* source, const std::string& backup_path)
{
    std::error_code ec;
    fs::remove(Utils::utf8_to_path(backup_path), ec);
    sqlite3* destination = nullptr;
    if (sqlite3_open(backup_path.c_str(), &destination) != SQLITE_OK) {
        if (destination) sqlite3_close(destination);
        return false;
    }
    sqlite3_backup* backup = sqlite3_backup_init(destination, "main", source, "main");
    if (!backup) {
        sqlite3_close(destination);
        return false;
    }
    const int step_rc = sqlite3_backup_step(backup, -1);
    const int finish_rc = sqlite3_backup_finish(backup);
    const bool ok = step_rc == SQLITE_DONE && finish_rc == SQLITE_OK;
    sqlite3_close(destination);
    return ok;
}

bool create_latest_tables(sqlite3* db)
{
    static constexpr const char* kSchema = R"SQL(
CREATE TABLE IF NOT EXISTS personal_index_scan_runs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    started_at_ms INTEGER NOT NULL,
    completed_at_ms INTEGER,
    status TEXT NOT NULL,
    roots TEXT NOT NULL,
    files_indexed INTEGER NOT NULL DEFAULT 0,
    directories_indexed INTEGER NOT NULL DEFAULT 0,
    protected_projects_indexed INTEGER NOT NULL DEFAULT 0,
    entries_skipped INTEGER NOT NULL DEFAULT 0,
    errors INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS personal_index_entries (
    full_path TEXT PRIMARY KEY,
    path_key TEXT NOT NULL DEFAULT '',
    parent_path TEXT NOT NULL,
    name TEXT NOT NULL,
    extension TEXT NOT NULL DEFAULT '',
    entry_type INTEGER NOT NULL,
    size_bytes INTEGER NOT NULL DEFAULT 0,
    created_at_ms INTEGER,
    modified_at_ms INTEGER,
    sha256 TEXT,
    hash_state TEXT NOT NULL DEFAULT 'not_requested',
    mime_type TEXT NOT NULL DEFAULT '',
    text_state TEXT NOT NULL DEFAULT 'not_analyzed',
    summary TEXT NOT NULL DEFAULT '',
    image_description TEXT NOT NULL DEFAULT '',
    content_language TEXT NOT NULL DEFAULT '',
    project_rule_id TEXT NOT NULL DEFAULT '',
    project_type TEXT NOT NULL DEFAULT '',
    project_reason TEXT NOT NULL DEFAULT '',
    scan_root TEXT NOT NULL,
    last_seen_run_id INTEGER NOT NULL,
    last_indexed_at_ms INTEGER NOT NULL,
    observation_state TEXT NOT NULL DEFAULT 'unknown',
    policy_state TEXT NOT NULL DEFAULT 'included',
    is_present INTEGER NOT NULL DEFAULT 1,
    error TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS personal_index_parent_idx ON personal_index_entries(parent_path);
CREATE INDEX IF NOT EXISTS personal_index_path_key_idx ON personal_index_entries(path_key);
CREATE INDEX IF NOT EXISTS personal_index_extension_idx ON personal_index_entries(extension);
CREATE INDEX IF NOT EXISTS personal_index_size_idx ON personal_index_entries(size_bytes);
CREATE INDEX IF NOT EXISTS personal_index_sha256_idx ON personal_index_entries(sha256);
CREATE INDEX IF NOT EXISTS personal_index_run_idx ON personal_index_entries(last_seen_run_id);
CREATE INDEX IF NOT EXISTS personal_index_scan_root_idx ON personal_index_entries(scan_root, observation_state);
)SQL";
    return execute(db, kSchema);
}

bool backfill_path_keys(sqlite3* db)
{
    auto select = prepare(db, "SELECT rowid, full_path FROM personal_index_entries;");
    auto update = prepare(db, "UPDATE personal_index_entries SET path_key=? WHERE rowid=?;");
    if (!select || !update) return false;
    while (sqlite3_step(select.get()) == SQLITE_ROW) {
        const sqlite3_int64 rowid = sqlite3_column_int64(select.get(), 0);
        const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(select.get(), 1));
        if (!text) return false;
        std::string key;
        try {
            key = path_identity_key(Utils::utf8_to_path(text));
        } catch (const std::exception&) {
            key = text;
            std::replace(key.begin(), key.end(), '\\', '/');
#ifdef _WIN32
            key = QString::fromUtf8(key.c_str()).toCaseFolded().toUtf8().toStdString();
#endif
        }
        sqlite3_reset(update.get());
        sqlite3_clear_bindings(update.get());
        sqlite3_bind_text(update.get(), 1, key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(update.get(), 2, rowid);
        if (sqlite3_step(update.get()) != SQLITE_DONE) return false;
    }
    return true;
}

#ifdef _WIN32
std::optional<fs::path> path_from_environment(const wchar_t* variable)
{
    const wchar_t* value = _wgetenv(variable);
    if (!value || value[0] == L'\0') return std::nullopt;
    return normalized_root(fs::path(value));
}

bool same_path_windows(const fs::path& left, const fs::path& right)
{
    return path_identity_key(left) == path_identity_key(right);
}

bool is_windows_system_root(const fs::path& path)
{
    static const std::vector<fs::path> environment_roots = [] {
        std::vector<fs::path> result;
        for (const wchar_t* variable : {L"WINDIR", L"ProgramFiles", L"ProgramFiles(x86)", L"ProgramData", L"LOCALAPPDATA", L"APPDATA"}) {
            if (auto value = path_from_environment(variable)) result.push_back(std::move(*value));
        }
        return result;
    }();
    for (const auto& protected_root : environment_roots) {
        if (same_path_windows(path, protected_root)) return true;
    }
    const fs::path root = path.root_path();
    if (root.empty()) return false;
    static const std::unordered_set<std::string> protected_names = {
        "$recycle.bin", "system volume information", "recovery", "perflogs"};
    if (!same_path_windows(path.parent_path(), root)) return false;
    return protected_names.contains(lower_ascii(Utils::path_to_utf8(path.filename())));
}
#else
bool is_windows_system_root(const fs::path&) { return false; }
#endif

struct EntryMetadata {
    std::string full_path;
    std::string path_key;
    std::string parent_path;
    std::string name;
    std::string extension;
    int entry_type{kEntryTypeFile};
    std::int64_t size_bytes{0};
    std::int64_t created_at_ms{-1};
    std::int64_t modified_at_ms{-1};
    std::string sha256;
    std::string hash_state{"not_requested"};
    std::string observation_state{kObservationPresent};
    std::string policy_state{kPolicyIncluded};
    std::string project_rule_id;
    std::string project_type;
    std::string project_reason;
    std::string error;
};

EntryMetadata build_metadata(const fs::path& path,
                             int entry_type,
                             bool compute_sha256,
                             const std::optional<ProtectedProjectMatch>& project_match,
                             const char* policy_state = kPolicyIncluded)
{
    EntryMetadata metadata;
    metadata.full_path = Utils::path_to_utf8(path);
    metadata.path_key = path_identity_key(path);
    metadata.parent_path = Utils::path_to_utf8(path.parent_path());
    metadata.name = Utils::path_to_utf8(path.filename());
    metadata.extension = lower_ascii(Utils::path_to_utf8(path.extension()));
    metadata.entry_type = entry_type;
    metadata.policy_state = policy_state;
    const QFileInfo info(QString::fromUtf8(metadata.full_path.c_str()));
    if (entry_type == kEntryTypeFile) metadata.size_bytes = info.exists() ? info.size() : 0;
    if (info.birthTime().isValid()) metadata.created_at_ms = info.birthTime().toMSecsSinceEpoch();
    if (info.lastModified().isValid()) metadata.modified_at_ms = info.lastModified().toMSecsSinceEpoch();
    if (project_match) {
        metadata.project_rule_id = project_match->id;
        metadata.project_type = project_match->name;
        metadata.project_reason = project_match->reason;
    }
    if (entry_type == kEntryTypeFile && compute_sha256) {
        if (auto hash = sha256_file(path)) {
            metadata.sha256 = std::move(*hash);
            metadata.hash_state = "complete";
        } else {
            metadata.hash_state = "error";
            metadata.error = "Unable to read file for SHA-256 hashing";
        }
    }
    return metadata;
}

void bind_text(sqlite3_stmt* statement, int index, const std::string& value)
{
    sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT);
}

} // namespace

struct PersonalFileIndex::Impl {
    sqlite3* db{nullptr};
    std::string database_path;
    ProtectedProjectDetector project_detector;

    ~Impl() { if (db) sqlite3_close(db); }

    bool initialize_schema()
    {
        if (!execute(db, "PRAGMA journal_mode=WAL;") ||
            !execute(db, "PRAGMA synchronous=NORMAL;") ||
            !execute(db, "PRAGMA foreign_keys=ON;")) return false;

        const int version = schema_version(db);
        if (version < 0 || version > kCurrentSchemaVersion) return false;
        const bool had_entries = table_exists(db, "personal_index_entries");
        const bool had_runs = table_exists(db, "personal_index_scan_runs");
        if (!had_entries && !had_runs) {
            return create_latest_tables(db) && execute(db, "PRAGMA user_version=2;");
        }

        const bool needs_migration =
            version < kCurrentSchemaVersion ||
            !column_exists(db, "personal_index_entries", "path_key") ||
            !column_exists(db, "personal_index_entries", "observation_state") ||
            !column_exists(db, "personal_index_entries", "policy_state");
        if (needs_migration) {
            const std::string backup_path = database_path + ".pre-migration-v" + std::to_string(version) + ".bak";
            if (!backup_database(db, backup_path) || !execute(db, "BEGIN IMMEDIATE;")) return false;

            // Do not create indexes that reference new columns until those columns
            // have been added to an existing legacy entries table.
            bool ok = !had_entries ? create_latest_tables(db) : true;
            if (ok && !column_exists(db, "personal_index_entries", "path_key")) {
                ok = execute(db, "ALTER TABLE personal_index_entries ADD COLUMN path_key TEXT NOT NULL DEFAULT ''; ");
            }
            if (ok && !column_exists(db, "personal_index_entries", "observation_state")) {
                ok = execute(db, "ALTER TABLE personal_index_entries ADD COLUMN observation_state TEXT NOT NULL DEFAULT 'unknown';");
            }
            if (ok && !column_exists(db, "personal_index_entries", "policy_state")) {
                ok = execute(db, "ALTER TABLE personal_index_entries ADD COLUMN policy_state TEXT NOT NULL DEFAULT 'included';");
            }
            if (ok) {
                ok = execute(db,
                    "UPDATE personal_index_entries "
                    "SET observation_state=CASE WHEN is_present=1 THEN 'present' ELSE 'missing' END, policy_state='included';");
            }
            if (ok) ok = backfill_path_keys(db);
            // Now the latest indexes are safe to create, and any missing run table
            // is created inside the same migration transaction.
            if (ok) ok = create_latest_tables(db);
            if (ok) ok = execute(db, "PRAGMA user_version=2;");

            if (!ok || !execute(db, "COMMIT;")) {
                execute(db, "ROLLBACK;");
                return false;
            }
            return true;
        }

        return create_latest_tables(db) && execute(db, "PRAGMA user_version=2;");
    }

    std::int64_t begin_run(const std::vector<std::string>& roots)
    {
        auto statement = prepare(db,
            "INSERT INTO personal_index_scan_runs(started_at_ms, status, roots) VALUES(?, 'running', ?);");
        if (!statement) return -1;
        sqlite3_bind_int64(statement.get(), 1, now_ms());
        bind_text(statement.get(), 2, join_roots(roots));
        if (sqlite3_step(statement.get()) != SQLITE_DONE) return -1;
        return sqlite3_last_insert_rowid(db);
    }

    void finish_run(const PersonalFileIndexScanSummary& summary, const char* status)
    {
        auto statement = prepare(db,
            "UPDATE personal_index_scan_runs SET completed_at_ms=?, status=?, files_indexed=?, "
            "directories_indexed=?, protected_projects_indexed=?, entries_skipped=?, errors=? WHERE id=?;");
        if (!statement) return;
        sqlite3_bind_int64(statement.get(), 1, now_ms());
        sqlite3_bind_text(statement.get(), 2, status, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(statement.get(), 3, static_cast<sqlite3_int64>(summary.files_indexed));
        sqlite3_bind_int64(statement.get(), 4, static_cast<sqlite3_int64>(summary.directories_indexed));
        sqlite3_bind_int64(statement.get(), 5, static_cast<sqlite3_int64>(summary.protected_projects_indexed));
        sqlite3_bind_int64(statement.get(), 6, static_cast<sqlite3_int64>(summary.entries_skipped));
        sqlite3_bind_int64(statement.get(), 7, static_cast<sqlite3_int64>(summary.errors));
        sqlite3_bind_int64(statement.get(), 8, summary.run_id);
        sqlite3_step(statement.get());
    }

    bool prepare_root_policy(const std::string& root)
    {
        auto statement = prepare(db, "UPDATE personal_index_entries SET policy_state='included' WHERE scan_root=?;");
        if (!statement) return false;
        bind_text(statement.get(), 1, root);
        return sqlite3_step(statement.get()) == SQLITE_DONE;
    }

    bool mark_unseen_missing(const std::string& root, std::int64_t run_id)
    {
        auto statement = prepare(db,
            "UPDATE personal_index_entries SET observation_state='missing', is_present=0 "
            "WHERE scan_root=? AND last_seen_run_id<>? AND policy_state='included';");
        if (!statement) return false;
        bind_text(statement.get(), 1, root);
        sqlite3_bind_int64(statement.get(), 2, run_id);
        return sqlite3_step(statement.get()) == SQLITE_DONE;
    }

    bool mark_descendants_unknown(const std::string& scan_root,
                                  const std::string& parent_key,
                                  std::int64_t run_id,
                                  const std::string& policy_state)
    {
        std::string prefix = parent_key;
        if (prefix.empty() || prefix.back() != '/') prefix.push_back('/');
        auto statement = prepare(db,
            "UPDATE personal_index_entries SET observation_state='unknown', policy_state=?, "
            "last_seen_run_id=?, last_indexed_at_ms=? "
            "WHERE scan_root=? AND substr(path_key, 1, length(?))=?;");
        if (!statement) return false;
        bind_text(statement.get(), 1, policy_state);
        sqlite3_bind_int64(statement.get(), 2, run_id);
        sqlite3_bind_int64(statement.get(), 3, now_ms());
        bind_text(statement.get(), 4, scan_root);
        bind_text(statement.get(), 5, prefix);
        bind_text(statement.get(), 6, prefix);
        return sqlite3_step(statement.get()) == SQLITE_DONE;
    }

    StatementPtr prepare_upsert()
    {
        return prepare(db, R"SQL(
INSERT INTO personal_index_entries(
    full_path, path_key, parent_path, name, extension, entry_type, size_bytes,
    created_at_ms, modified_at_ms, sha256, hash_state,
    project_rule_id, project_type, project_reason,
    scan_root, last_seen_run_id, last_indexed_at_ms,
    observation_state, policy_state, is_present, error)
VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
ON CONFLICT(full_path) DO UPDATE SET
    path_key=excluded.path_key,
    parent_path=excluded.parent_path,
    name=excluded.name,
    extension=excluded.extension,
    entry_type=excluded.entry_type,
    size_bytes=excluded.size_bytes,
    created_at_ms=excluded.created_at_ms,
    modified_at_ms=excluded.modified_at_ms,
    sha256=CASE WHEN excluded.hash_state='not_requested' THEN personal_index_entries.sha256 ELSE excluded.sha256 END,
    hash_state=CASE WHEN excluded.hash_state='not_requested' AND personal_index_entries.hash_state='complete'
                    THEN personal_index_entries.hash_state ELSE excluded.hash_state END,
    project_rule_id=excluded.project_rule_id,
    project_type=excluded.project_type,
    project_reason=excluded.project_reason,
    scan_root=excluded.scan_root,
    last_seen_run_id=excluded.last_seen_run_id,
    last_indexed_at_ms=excluded.last_indexed_at_ms,
    observation_state=excluded.observation_state,
    policy_state=excluded.policy_state,
    is_present=excluded.is_present,
    error=excluded.error;
)SQL");
    }

    StatementPtr prepare_delete_alias()
    {
        return prepare(db, "DELETE FROM personal_index_entries WHERE path_key=? AND full_path<>?;");
    }

    bool upsert(sqlite3_stmt* statement,
                sqlite3_stmt* delete_alias,
                const EntryMetadata& metadata,
                const std::string& scan_root,
                std::int64_t run_id)
    {
        sqlite3_reset(delete_alias);
        sqlite3_clear_bindings(delete_alias);
        bind_text(delete_alias, 1, metadata.path_key);
        bind_text(delete_alias, 2, metadata.full_path);
        if (sqlite3_step(delete_alias) != SQLITE_DONE) return false;

        sqlite3_reset(statement);
        sqlite3_clear_bindings(statement);
        bind_text(statement, 1, metadata.full_path);
        bind_text(statement, 2, metadata.path_key);
        bind_text(statement, 3, metadata.parent_path);
        bind_text(statement, 4, metadata.name);
        bind_text(statement, 5, metadata.extension);
        sqlite3_bind_int(statement, 6, metadata.entry_type);
        sqlite3_bind_int64(statement, 7, metadata.size_bytes);
        if (metadata.created_at_ms >= 0) sqlite3_bind_int64(statement, 8, metadata.created_at_ms); else sqlite3_bind_null(statement, 8);
        if (metadata.modified_at_ms >= 0) sqlite3_bind_int64(statement, 9, metadata.modified_at_ms); else sqlite3_bind_null(statement, 9);
        if (!metadata.sha256.empty()) bind_text(statement, 10, metadata.sha256); else sqlite3_bind_null(statement, 10);
        bind_text(statement, 11, metadata.hash_state);
        bind_text(statement, 12, metadata.project_rule_id);
        bind_text(statement, 13, metadata.project_type);
        bind_text(statement, 14, metadata.project_reason);
        bind_text(statement, 15, scan_root);
        sqlite3_bind_int64(statement, 16, run_id);
        sqlite3_bind_int64(statement, 17, now_ms());
        bind_text(statement, 18, metadata.observation_state);
        bind_text(statement, 19, metadata.policy_state);
        sqlite3_bind_int(statement, 20, metadata.observation_state == kObservationPresent ? 1 : 0);
        bind_text(statement, 21, metadata.error);
        return sqlite3_step(statement) == SQLITE_DONE;
    }
};

PersonalFileIndex::PersonalFileIndex(std::string config_dir)
{
    std::error_code ec;
    fs::create_directories(Utils::utf8_to_path(config_dir), ec);
    if (ec) return;
    database_path_ = config_dir + "/personal_file_index.db";
    impl_ = new Impl();
    impl_->database_path = database_path_;
    if (sqlite3_open(database_path_.c_str(), &impl_->db) != SQLITE_OK) {
        delete impl_;
        impl_ = nullptr;
        return;
    }
    sqlite3_busy_timeout(impl_->db, 5000);
    if (!impl_->initialize_schema()) {
        delete impl_;
        impl_ = nullptr;
    }
}

PersonalFileIndex::~PersonalFileIndex() { delete impl_; }
bool PersonalFileIndex::is_open() const noexcept { return impl_ && impl_->db; }
const std::string& PersonalFileIndex::database_path() const noexcept { return database_path_; }

std::vector<std::string> PersonalFileIndex::default_excluded_directory_names()
{
    return {".git", "node_modules", ".next", ".venv", "venv", "__pycache__", ".cache", ".idea"};
}

PersonalFileIndexScanSummary PersonalFileIndex::scan(const std::vector<std::string>& roots,
                                                     const PersonalFileIndexOptions& options)
{
    PersonalFileIndexScanSummary summary;
    if (!is_open()) {
        summary.errors = 1;
        summary.warnings.push_back("Personal file index database is not open");
        return summary;
    }
    if (roots.empty()) {
        summary.errors = 1;
        summary.warnings.push_back("No scan roots were provided");
        return summary;
    }
    if (options.follow_reparse_points) {
        summary.errors = 1;
        summary.warnings.push_back(
            "Following reparse points/symlinks is disabled in Phase 1 until cycle detection, "
            "root-boundary enforcement, and directory-identity tracking are implemented.");
        return summary;
    }

    struct RootInfo { fs::path path; std::string display; std::string key; };
    std::vector<RootInfo> normalized;
    normalized.reserve(roots.size());
    for (const auto& root : roots) {
        if (root.empty()) continue;
        try {
            fs::path path = normalized_root(Utils::utf8_to_path(root));
            normalized.push_back({path, Utils::path_to_utf8(path), path_identity_key(path)});
        } catch (const std::exception& ex) {
            summary.errors = 1;
            summary.warnings.push_back(std::string("Could not normalize scan root: ") + ex.what());
            return summary;
        }
    }
    if (normalized.empty()) {
        summary.errors = 1;
        summary.warnings.push_back("No valid scan roots were provided");
        return summary;
    }
    for (std::size_t i = 0; i < normalized.size(); ++i) {
        for (std::size_t j = i + 1; j < normalized.size(); ++j) {
            if (same_or_descendant_key(normalized[i].key, normalized[j].key) ||
                same_or_descendant_key(normalized[j].key, normalized[i].key)) {
                summary.errors = 1;
                summary.warnings.push_back(
                    "Duplicate or overlapping scan roots are not supported in Phase 1: '" +
                    normalized[i].display + "' and '" + normalized[j].display + "'.");
                return summary;
            }
        }
    }

    std::vector<std::string> normalized_roots;
    normalized_roots.reserve(normalized.size());
    for (const auto& root : normalized) normalized_roots.push_back(root.display);
    summary.run_id = impl_->begin_run(normalized_roots);
    if (summary.run_id < 0) {
        summary.errors = 1;
        summary.warnings.push_back("Could not create file-index scan run");
        return summary;
    }

    std::vector<std::string> exclusions = options.excluded_directory_names;
    if (exclusions.empty()) exclusions = default_excluded_directory_names();
    const auto exclusion_set = make_exclusion_set(exclusions);
    const std::size_t batch_size = std::max<std::size_t>(1, options.commit_batch_size);
    auto upsert = impl_->prepare_upsert();
    auto delete_alias = impl_->prepare_delete_alias();
    if (!upsert || !delete_alias) {
        ++summary.errors;
        summary.warnings.push_back("Could not prepare index persistence statements");
        impl_->finish_run(summary, "failed");
        return summary;
    }

    bool transaction_open = false;
    std::size_t entries_in_batch = 0;
    bool fatal_database_error = false;
    std::size_t partial_roots = 0;

    auto begin_transaction = [&]() {
        if (transaction_open) return true;
        if (!execute(impl_->db, "BEGIN IMMEDIATE;")) return false;
        transaction_open = true;
        entries_in_batch = 0;
        return true;
    };
    auto commit_transaction = [&]() {
        if (!transaction_open) return true;
        if (!execute(impl_->db, "COMMIT;")) {
            execute(impl_->db, "ROLLBACK;");
            transaction_open = false;
            return false;
        }
        transaction_open = false;
        entries_in_batch = 0;
        return true;
    };
    auto persist = [&](const EntryMetadata& metadata, const std::string& scan_root) {
        if (!begin_transaction()) {
            ++summary.errors;
            summary.warnings.push_back("Could not begin index write transaction");
            return false;
        }
        if (!impl_->upsert(upsert.get(), delete_alias.get(), metadata, scan_root, summary.run_id)) {
            ++summary.errors;
            summary.warnings.push_back("Could not index: " + metadata.full_path);
            return false;
        }
        ++entries_in_batch;
        if (entries_in_batch >= batch_size && !commit_transaction()) {
            ++summary.errors;
            summary.warnings.push_back("Could not commit an index batch");
            return false;
        }
        return true;
    };
    auto mark_skipped_subtree = [&](const EntryMetadata& metadata, const std::string& scan_root) {
        if (!begin_transaction()) {
            ++summary.errors;
            return false;
        }
        if (!impl_->mark_descendants_unknown(scan_root, metadata.path_key, summary.run_id, metadata.policy_state)) {
            ++summary.errors;
            summary.warnings.push_back("Could not preserve unknown state below skipped path: " + metadata.full_path);
            return false;
        }
        return true;
    };
    auto finalize_root_presence = [&](const std::string& root) {
        if (!commit_transaction()) {
            ++summary.errors;
            summary.warnings.push_back("Could not commit indexed entries before finalizing root: " + root);
            return false;
        }
        if (!begin_transaction()) {
            ++summary.errors;
            summary.warnings.push_back("Could not begin presence-finalization transaction for: " + root);
            return false;
        }
        if (!impl_->mark_unseen_missing(root, summary.run_id)) {
            ++summary.errors;
            summary.warnings.push_back("Could not finalize missing entries for: " + root);
            return false;
        }
        if (!commit_transaction()) {
            ++summary.errors;
            summary.warnings.push_back("Could not commit missing-entry state for: " + root);
            return false;
        }
        return true;
    };
    auto preserve_presence_for_partial_root = [&](const std::string& root) {
        ++partial_roots;
        summary.warnings.push_back(
            "Scan was incomplete for '" + root +
            "'; previous physical-observation state for unseen entries was preserved.");
    };

    for (const auto& root_info : normalized) {
        if (fatal_database_error) break;
        const fs::path& root = root_info.path;
        const std::string& root_string = root_info.display;
        std::error_code exists_ec;
        if (!fs::exists(root, exists_ec) || exists_ec) {
            ++summary.errors;
            summary.warnings.push_back("Scan root does not exist or is inaccessible: " + root_string);
            preserve_presence_for_partial_root(root_string);
            continue;
        }
        if (is_windows_system_root(root)) {
            ++summary.entries_skipped;
            summary.warnings.push_back("Protected Windows/system root was not traversed: " + root_string);
            preserve_presence_for_partial_root(root_string);
            continue;
        }
        if (!begin_transaction() || !impl_->prepare_root_policy(root_string) || !commit_transaction()) {
            ++summary.errors;
            summary.warnings.push_back("Could not prepare root scan state: " + root_string);
            fatal_database_error = true;
            break;
        }

        std::error_code dir_ec;
        const bool root_is_directory = fs::is_directory(root, dir_ec);
        if (dir_ec) {
            ++summary.errors;
            summary.warnings.push_back("Could not inspect scan root: " + root_string);
            preserve_presence_for_partial_root(root_string);
            continue;
        }
        if (!root_is_directory) {
            std::error_code regular_ec;
            if (fs::is_regular_file(root, regular_ec) && !regular_ec) {
                EntryMetadata metadata = build_metadata(root, kEntryTypeFile, options.compute_sha256, std::nullopt);
                if (!persist(metadata, root_string)) { fatal_database_error = true; break; }
                ++summary.files_indexed;
                if (!metadata.error.empty()) {
                    ++summary.errors;
                    summary.warnings.push_back(metadata.full_path + ": " + metadata.error);
                }
                if (!finalize_root_presence(root_string)) { fatal_database_error = true; break; }
            } else {
                ++summary.entries_skipped;
                ++summary.errors;
                summary.warnings.push_back("Unsupported scan root type: " + root_string);
                preserve_presence_for_partial_root(root_string);
            }
            continue;
        }

        bool root_persisted_as_project = false;
        if (options.protect_project_directories) {
            if (auto match = impl_->project_detector.detect(root); match && ProtectedProjectDetector::should_skip(*match)) {
                EntryMetadata metadata = build_metadata(root, kEntryTypeProtectedProject, false, match, kPolicyProtected);
                if (!persist(metadata, root_string)) { fatal_database_error = true; break; }
                ++summary.protected_projects_indexed;
                root_persisted_as_project = true;
                if (!options.index_protected_project_contents) {
                    ++summary.entries_skipped;
                    if (!mark_skipped_subtree(metadata, root_string) || !finalize_root_presence(root_string)) fatal_database_error = true;
                    continue;
                }
            }
        }
        if (!root_persisted_as_project) {
            EntryMetadata metadata = build_metadata(root, kEntryTypeDirectory, false, std::nullopt);
            if (!persist(metadata, root_string)) { fatal_database_error = true; break; }
            ++summary.directories_indexed;
        }

        bool root_complete = true;
        std::vector<fs::path> pending_directories{root};
        while (!pending_directories.empty() && !fatal_database_error) {
            const fs::path current = std::move(pending_directories.back());
            pending_directories.pop_back();
            std::error_code iterator_ec;
            fs::directory_iterator iterator(current, fs::directory_options::none, iterator_ec);
            if (iterator_ec) {
                root_complete = false;
                ++summary.errors;
                summary.warnings.push_back("Could not enumerate directory: " + Utils::path_to_utf8(current));
                continue;
            }
            const fs::directory_iterator end;
            while (iterator != end && !fatal_database_error) {
                const fs::directory_entry entry = *iterator;
                const fs::path path = entry.path();
                const std::string name = Utils::path_to_utf8(path.filename());
                std::error_code increment_ec;
                iterator.increment(increment_ec);
                if (increment_ec) {
                    root_complete = false;
                    ++summary.errors;
                    summary.warnings.push_back("Directory enumeration stopped early: " + Utils::path_to_utf8(current));
                }
                std::error_code entry_dir_ec;
                const bool is_directory = entry.is_directory(entry_dir_ec);
                if (entry_dir_ec) {
                    root_complete = false;
                    ++summary.errors;
                    summary.warnings.push_back("Could not inspect entry: " + Utils::path_to_utf8(path));
                    if (increment_ec) break;
                    continue;
                }
                if (is_reparse_or_symlink(entry)) {
                    EntryMetadata metadata = build_metadata(path, is_directory ? kEntryTypeDirectory : kEntryTypeFile,
                                                           false, std::nullopt, kPolicyReparseSkipped);
                    if (!persist(metadata, root_string) ||
                        (is_directory && !mark_skipped_subtree(metadata, root_string))) {
                        fatal_database_error = true; break;
                    }
                    ++summary.entries_skipped;
                    if (increment_ec) break;
                    continue;
                }
                if (!options.include_hidden && is_hidden(path)) {
                    EntryMetadata metadata = build_metadata(path, is_directory ? kEntryTypeDirectory : kEntryTypeFile,
                                                           false, std::nullopt, kPolicyHidden);
                    if (!persist(metadata, root_string) ||
                        (is_directory && !mark_skipped_subtree(metadata, root_string))) {
                        fatal_database_error = true; break;
                    }
                    ++summary.entries_skipped;
                    if (increment_ec) break;
                    continue;
                }
                if (is_directory && is_windows_system_root(path)) {
                    EntryMetadata metadata = build_metadata(path, kEntryTypeDirectory, false, std::nullopt, kPolicySystem);
                    if (!persist(metadata, root_string) || !mark_skipped_subtree(metadata, root_string)) {
                        fatal_database_error = true; break;
                    }
                    ++summary.entries_skipped;
                    if (increment_ec) break;
                    continue;
                }
                if (is_directory && exclusion_set.contains(comparison_name(name))) {
                    EntryMetadata metadata = build_metadata(path, kEntryTypeDirectory, false, std::nullopt, kPolicyExcluded);
                    if (!persist(metadata, root_string) || !mark_skipped_subtree(metadata, root_string)) {
                        fatal_database_error = true; break;
                    }
                    ++summary.entries_skipped;
                    if (increment_ec) break;
                    continue;
                }
                if (is_directory) {
                    bool protected_project = false;
                    if (options.protect_project_directories) {
                        if (auto match = impl_->project_detector.detect(path); match && ProtectedProjectDetector::should_skip(*match)) {
                            EntryMetadata metadata = build_metadata(path, kEntryTypeProtectedProject, false, match, kPolicyProtected);
                            if (!persist(metadata, root_string)) { fatal_database_error = true; break; }
                            ++summary.protected_projects_indexed;
                            protected_project = true;
                            if (options.index_protected_project_contents) pending_directories.push_back(path);
                            else {
                                ++summary.entries_skipped;
                                if (!mark_skipped_subtree(metadata, root_string)) { fatal_database_error = true; break; }
                            }
                        }
                    }
                    if (protected_project) {
                        if (increment_ec) break;
                        continue;
                    }
                    EntryMetadata metadata = build_metadata(path, kEntryTypeDirectory, false, std::nullopt);
                    if (!persist(metadata, root_string)) { fatal_database_error = true; break; }
                    ++summary.directories_indexed;
                    pending_directories.push_back(path);
                } else {
                    std::error_code regular_ec;
                    const bool regular = entry.is_regular_file(regular_ec);
                    if (regular_ec) {
                        root_complete = false;
                        ++summary.errors;
                        summary.warnings.push_back("Could not inspect file entry: " + Utils::path_to_utf8(path));
                        if (increment_ec) break;
                        continue;
                    }
                    if (!regular) {
                        ++summary.entries_skipped;
                        if (increment_ec) break;
                        continue;
                    }
                    EntryMetadata metadata = build_metadata(path, kEntryTypeFile, options.compute_sha256, std::nullopt);
                    if (!persist(metadata, root_string)) { fatal_database_error = true; break; }
                    ++summary.files_indexed;
                    if (!metadata.error.empty()) {
                        ++summary.errors;
                        summary.warnings.push_back(metadata.full_path + ": " + metadata.error);
                    }
                }
                if (increment_ec) break;
            }
        }

        if (fatal_database_error) break;
        if (!commit_transaction()) {
            ++summary.errors;
            summary.warnings.push_back("Could not commit final index batch for: " + root_string);
            fatal_database_error = true;
            break;
        }
        if (root_complete) {
            if (!finalize_root_presence(root_string)) { fatal_database_error = true; break; }
        } else {
            preserve_presence_for_partial_root(root_string);
        }
    }

    if (transaction_open && !commit_transaction()) {
        ++summary.errors;
        summary.warnings.push_back("Could not commit final index transaction");
        fatal_database_error = true;
    }
    summary.completed = !fatal_database_error;
    const char* status = fatal_database_error ? "failed" : (partial_roots > 0 ? "partial" : "completed");
    impl_->finish_run(summary, status);
    return summary;
}
