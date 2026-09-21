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
#include <cstdlib>
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
constexpr std::size_t kHashReadChunkBytes = 1024 * 1024;

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

bool execute(sqlite3* db, const char* sql)
{
    char* error = nullptr;
    const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &error);
    if (error) {
        sqlite3_free(error);
    }
    return rc == SQLITE_OK;
}

std::int64_t now_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
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

std::unordered_set<std::string> make_exclusion_set(
    const std::vector<std::string>& exclusions)
{
    std::unordered_set<std::string> result;
    result.reserve(exclusions.size());
    for (const auto& value : exclusions) {
        if (!value.empty()) {
            result.insert(comparison_name(value));
        }
    }
    return result;
}

bool is_hidden(const fs::path& path)
{
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_HIDDEN) != 0;
#else
    const std::string name = path.filename().string();
    return !name.empty() && name.front() == '.';
#endif
}

bool is_reparse_or_symlink(const fs::directory_entry& entry)
{
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(entry.path().c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
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
    if (ec) {
        absolute = input;
    }

    fs::path canonical = fs::weakly_canonical(absolute, ec);
    if (!ec) {
        return canonical;
    }
    return absolute.lexically_normal();
}

std::string join_roots(const std::vector<std::string>& roots)
{
    std::ostringstream stream;
    bool first = true;
    for (const auto& root : roots) {
        if (!first) {
            stream << '\n';
        }
        first = false;
        stream << root;
    }
    return stream.str();
}

std::optional<std::string> sha256_file(const fs::path& path)
{
    const std::string utf8_path = Utils::path_to_utf8(path);
    QFile file(QString::fromUtf8(utf8_path.c_str()));
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(static_cast<qint64>(kHashReadChunkBytes));
        if (chunk.isEmpty() && file.error() != QFileDevice::NoError) {
            return std::nullopt;
        }
        hash.addData(chunk);
    }

    return hash.result().toHex().toStdString();
}

#ifdef _WIN32
std::string comparable_windows_path(const fs::path& path)
{
    std::error_code ec;
    fs::path absolute = fs::absolute(path, ec);
    if (ec) {
        absolute = path;
    }
    return lower_ascii(Utils::path_to_utf8(absolute.lexically_normal()));
}

std::optional<fs::path> path_from_environment(const char* variable)
{
    const char* value = std::getenv(variable);
    if (!value || value[0] == '\0') {
        return std::nullopt;
    }
    return normalized_root(Utils::utf8_to_path(value));
}

bool same_path_windows(const fs::path& left, const fs::path& right)
{
    return comparable_windows_path(left) == comparable_windows_path(right);
}

bool is_windows_system_root(const fs::path& path)
{
    static const std::vector<fs::path> environment_roots = [] {
        std::vector<fs::path> result;
        for (const char* variable : {
                 "WINDIR",
                 "ProgramFiles",
                 "ProgramFiles(x86)",
                 "ProgramData",
                 "LOCALAPPDATA",
                 "APPDATA"}) {
            if (auto value = path_from_environment(variable)) {
                result.push_back(std::move(*value));
            }
        }
        return result;
    }();

    for (const auto& protected_root : environment_roots) {
        if (same_path_windows(path, protected_root)) {
            return true;
        }
    }

    const fs::path root = path.root_path();
    if (root.empty()) {
        return false;
    }

    static const std::unordered_set<std::string> drive_root_protected_names = {
        "$recycle.bin",
        "system volume information",
        "recovery",
        "perflogs"
    };

    const fs::path parent = path.parent_path();
    if (!same_path_windows(parent, root)) {
        return false;
    }

    return drive_root_protected_names.contains(
        lower_ascii(Utils::path_to_utf8(path.filename())));
}
#else
bool is_windows_system_root(const fs::path&)
{
    return false;
}
#endif

struct EntryMetadata {
    std::string full_path;
    std::string parent_path;
    std::string name;
    std::string extension;
    int entry_type{kEntryTypeFile};
    std::int64_t size_bytes{0};
    std::int64_t created_at_ms{-1};
    std::int64_t modified_at_ms{-1};
    std::string sha256;
    std::string hash_state{"not_requested"};
    std::string project_rule_id;
    std::string project_type;
    std::string project_reason;
    std::string error;
};

EntryMetadata build_metadata(
    const fs::path& path,
    int entry_type,
    bool compute_sha256,
    const std::optional<ProtectedProjectMatch>& project_match)
{
    EntryMetadata metadata;
    metadata.full_path = Utils::path_to_utf8(path);
    metadata.parent_path = Utils::path_to_utf8(path.parent_path());
    metadata.name = Utils::path_to_utf8(path.filename());
    metadata.extension = lower_ascii(Utils::path_to_utf8(path.extension()));
    metadata.entry_type = entry_type;

    const QFileInfo info(QString::fromUtf8(metadata.full_path.c_str()));
    if (entry_type == kEntryTypeFile) {
        metadata.size_bytes = info.exists() ? info.size() : 0;
    }
    if (info.birthTime().isValid()) {
        metadata.created_at_ms = info.birthTime().toMSecsSinceEpoch();
    }
    if (info.lastModified().isValid()) {
        metadata.modified_at_ms = info.lastModified().toMSecsSinceEpoch();
    }

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
    ProtectedProjectDetector project_detector;

    ~Impl()
    {
        if (db) {
            sqlite3_close(db);
        }
    }

    bool initialize_schema()
    {
        static constexpr const char* kSchema = R"SQL(
PRAGMA journal_mode=WAL;
PRAGMA synchronous=NORMAL;
PRAGMA foreign_keys=ON;

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
    is_present INTEGER NOT NULL DEFAULT 1,
    error TEXT NOT NULL DEFAULT ''
);

CREATE INDEX IF NOT EXISTS personal_index_parent_idx
    ON personal_index_entries(parent_path);
CREATE INDEX IF NOT EXISTS personal_index_extension_idx
    ON personal_index_entries(extension);
CREATE INDEX IF NOT EXISTS personal_index_size_idx
    ON personal_index_entries(size_bytes);
CREATE INDEX IF NOT EXISTS personal_index_sha256_idx
    ON personal_index_entries(sha256);
CREATE INDEX IF NOT EXISTS personal_index_run_idx
    ON personal_index_entries(last_seen_run_id);
CREATE INDEX IF NOT EXISTS personal_index_scan_root_idx
    ON personal_index_entries(scan_root, is_present);
)SQL";
        return execute(db, kSchema);
    }

    std::int64_t begin_run(const std::vector<std::string>& roots)
    {
        auto statement = prepare(
            db,
            "INSERT INTO personal_index_scan_runs(started_at_ms, status, roots) "
            "VALUES(?, 'running', ?);");
        if (!statement) {
            return -1;
        }
        sqlite3_bind_int64(statement.get(), 1, now_ms());
        bind_text(statement.get(), 2, join_roots(roots));
        if (sqlite3_step(statement.get()) != SQLITE_DONE) {
            return -1;
        }
        return sqlite3_last_insert_rowid(db);
    }

    void finish_run(const PersonalFileIndexScanSummary& summary, const char* status)
    {
        auto statement = prepare(
            db,
            "UPDATE personal_index_scan_runs "
            "SET completed_at_ms=?, status=?, files_indexed=?, directories_indexed=?, "
            "protected_projects_indexed=?, entries_skipped=?, errors=? WHERE id=?;");
        if (!statement) {
            return;
        }
        sqlite3_bind_int64(statement.get(), 1, now_ms());
        sqlite3_bind_text(statement.get(), 2, status, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(
            statement.get(), 3, static_cast<sqlite3_int64>(summary.files_indexed));
        sqlite3_bind_int64(
            statement.get(), 4, static_cast<sqlite3_int64>(summary.directories_indexed));
        sqlite3_bind_int64(
            statement.get(), 5,
            static_cast<sqlite3_int64>(summary.protected_projects_indexed));
        sqlite3_bind_int64(
            statement.get(), 6, static_cast<sqlite3_int64>(summary.entries_skipped));
        sqlite3_bind_int64(
            statement.get(), 7, static_cast<sqlite3_int64>(summary.errors));
        sqlite3_bind_int64(statement.get(), 8, summary.run_id);
        sqlite3_step(statement.get());
    }

    bool mark_unseen_not_present(const std::string& root, std::int64_t run_id)
    {
        auto statement = prepare(
            db,
            "UPDATE personal_index_entries SET is_present=0 "
            "WHERE scan_root=? AND last_seen_run_id<>?;");
        if (!statement) {
            return false;
        }
        bind_text(statement.get(), 1, root);
        sqlite3_bind_int64(statement.get(), 2, run_id);
        return sqlite3_step(statement.get()) == SQLITE_DONE;
    }

    StatementPtr prepare_upsert()
    {
        return prepare(
            db,
            R"SQL(
INSERT INTO personal_index_entries(
    full_path, parent_path, name, extension, entry_type, size_bytes,
    created_at_ms, modified_at_ms, sha256, hash_state,
    project_rule_id, project_type, project_reason,
    scan_root, last_seen_run_id, last_indexed_at_ms, is_present, error)
VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 1, ?)
ON CONFLICT(full_path) DO UPDATE SET
    parent_path=excluded.parent_path,
    name=excluded.name,
    extension=excluded.extension,
    entry_type=excluded.entry_type,
    size_bytes=excluded.size_bytes,
    created_at_ms=excluded.created_at_ms,
    modified_at_ms=excluded.modified_at_ms,
    sha256=CASE
        WHEN excluded.hash_state='not_requested' THEN personal_index_entries.sha256
        ELSE excluded.sha256
    END,
    hash_state=CASE
        WHEN excluded.hash_state='not_requested' AND personal_index_entries.hash_state='complete'
            THEN personal_index_entries.hash_state
        ELSE excluded.hash_state
    END,
    project_rule_id=excluded.project_rule_id,
    project_type=excluded.project_type,
    project_reason=excluded.project_reason,
    scan_root=excluded.scan_root,
    last_seen_run_id=excluded.last_seen_run_id,
    last_indexed_at_ms=excluded.last_indexed_at_ms,
    is_present=1,
    error=excluded.error;
)SQL");
    }

    bool upsert(
        sqlite3_stmt* statement,
        const EntryMetadata& metadata,
        const std::string& scan_root,
        std::int64_t run_id)
    {
        sqlite3_reset(statement);
        sqlite3_clear_bindings(statement);

        bind_text(statement, 1, metadata.full_path);
        bind_text(statement, 2, metadata.parent_path);
        bind_text(statement, 3, metadata.name);
        bind_text(statement, 4, metadata.extension);
        sqlite3_bind_int(statement, 5, metadata.entry_type);
        sqlite3_bind_int64(statement, 6, metadata.size_bytes);
        if (metadata.created_at_ms >= 0) {
            sqlite3_bind_int64(statement, 7, metadata.created_at_ms);
        } else {
            sqlite3_bind_null(statement, 7);
        }
        if (metadata.modified_at_ms >= 0) {
            sqlite3_bind_int64(statement, 8, metadata.modified_at_ms);
        } else {
            sqlite3_bind_null(statement, 8);
        }
        if (!metadata.sha256.empty()) {
            bind_text(statement, 9, metadata.sha256);
        } else {
            sqlite3_bind_null(statement, 9);
        }
        bind_text(statement, 10, metadata.hash_state);
        bind_text(statement, 11, metadata.project_rule_id);
        bind_text(statement, 12, metadata.project_type);
        bind_text(statement, 13, metadata.project_reason);
        bind_text(statement, 14, scan_root);
        sqlite3_bind_int64(statement, 15, run_id);
        sqlite3_bind_int64(statement, 16, now_ms());
        bind_text(statement, 17, metadata.error);

        return sqlite3_step(statement) == SQLITE_DONE;
    }
};

PersonalFileIndex::PersonalFileIndex(std::string config_dir)
{
    std::error_code ec;
    fs::create_directories(Utils::utf8_to_path(config_dir), ec);
    if (ec) {
        return;
    }

    database_path_ = config_dir + "/personal_file_index.db";
    impl_ = new Impl();
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

PersonalFileIndex::~PersonalFileIndex()
{
    delete impl_;
}

bool PersonalFileIndex::is_open() const noexcept
{
    return impl_ && impl_->db;
}

const std::string& PersonalFileIndex::database_path() const noexcept
{
    return database_path_;
}

std::vector<std::string> PersonalFileIndex::default_excluded_directory_names()
{
    return {
        ".git",
        "node_modules",
        ".next",
        ".venv",
        "venv",
        "__pycache__",
        ".cache",
        ".idea"
    };
}

PersonalFileIndexScanSummary PersonalFileIndex::scan(
    const std::vector<std::string>& roots,
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

    std::vector<std::string> normalized_roots;
    normalized_roots.reserve(roots.size());
    for (const auto& root : roots) {
        if (root.empty()) {
            continue;
        }
        normalized_roots.push_back(
            Utils::path_to_utf8(normalized_root(Utils::utf8_to_path(root))));
    }
    if (normalized_roots.empty()) {
        summary.errors = 1;
        summary.warnings.push_back("No valid scan roots were provided");
        return summary;
    }

    summary.run_id = impl_->begin_run(normalized_roots);
    if (summary.run_id < 0) {
        summary.errors = 1;
        summary.warnings.push_back("Could not create file-index scan run");
        return summary;
    }

    std::vector<std::string> exclusions = options.excluded_directory_names;
    if (exclusions.empty()) {
        exclusions = default_excluded_directory_names();
    }
    const auto exclusion_set = make_exclusion_set(exclusions);
    const std::size_t batch_size =
        std::max<std::size_t>(1, options.commit_batch_size);

    auto upsert = impl_->prepare_upsert();
    if (!upsert) {
        ++summary.errors;
        summary.warnings.push_back("Could not prepare index upsert statement");
        impl_->finish_run(summary, "failed");
        return summary;
    }

    bool transaction_open = false;
    std::size_t entries_in_batch = 0;
    bool fatal_database_error = false;
    std::size_t partial_roots = 0;

    auto begin_transaction = [&]() -> bool {
        if (transaction_open) {
            return true;
        }
        if (!execute(impl_->db, "BEGIN IMMEDIATE;")) {
            return false;
        }
        transaction_open = true;
        entries_in_batch = 0;
        return true;
    };

    auto commit_transaction = [&]() -> bool {
        if (!transaction_open) {
            return true;
        }
        if (!execute(impl_->db, "COMMIT;")) {
            execute(impl_->db, "ROLLBACK;");
            transaction_open = false;
            return false;
        }
        transaction_open = false;
        entries_in_batch = 0;
        return true;
    };

    auto persist = [&](const EntryMetadata& metadata,
                       const std::string& scan_root) -> bool {
        if (!begin_transaction()) {
            ++summary.errors;
            summary.warnings.push_back("Could not begin index write transaction");
            return false;
        }
        if (!impl_->upsert(upsert.get(), metadata, scan_root, summary.run_id)) {
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

    auto finalize_root_presence = [&](const std::string& root) -> bool {
        if (!commit_transaction()) {
            ++summary.errors;
            summary.warnings.push_back(
                "Could not commit indexed entries before finalizing root: " + root);
            return false;
        }
        if (!begin_transaction()) {
            ++summary.errors;
            summary.warnings.push_back(
                "Could not begin presence-finalization transaction for: " + root);
            return false;
        }
        if (!impl_->mark_unseen_not_present(root, summary.run_id)) {
            ++summary.errors;
            summary.warnings.push_back(
                "Could not finalize missing entries for: " + root);
            return false;
        }
        if (!commit_transaction()) {
            ++summary.errors;
            summary.warnings.push_back(
                "Could not commit missing-entry state for: " + root);
            return false;
        }
        return true;
    };

    auto preserve_presence_for_partial_root = [&](const std::string& root) {
        ++partial_roots;
        summary.warnings.push_back(
            "Scan was incomplete for '" + root +
            "'; previous presence state for unseen entries was preserved.");
    };

    for (const auto& root_string : normalized_roots) {
        if (fatal_database_error) {
            break;
        }

        const fs::path root = Utils::utf8_to_path(root_string);
        std::error_code exists_ec;
        if (!fs::exists(root, exists_ec) || exists_ec) {
            ++summary.errors;
            summary.warnings.push_back(
                "Scan root does not exist or is inaccessible: " + root_string);
            preserve_presence_for_partial_root(root_string);
            continue;
        }

        if (is_windows_system_root(root)) {
            ++summary.entries_skipped;
            summary.warnings.push_back(
                "Protected Windows/system root was not traversed: " + root_string);
            preserve_presence_for_partial_root(root_string);
            continue;
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
                EntryMetadata metadata = build_metadata(
                    root,
                    kEntryTypeFile,
                    options.compute_sha256,
                    std::nullopt);
                if (!persist(metadata, root_string)) {
                    fatal_database_error = true;
                    break;
                }
                ++summary.files_indexed;
                if (!metadata.error.empty()) {
                    ++summary.errors;
                    summary.warnings.push_back(
                        metadata.full_path + ": " + metadata.error);
                }
                if (!finalize_root_presence(root_string)) {
                    fatal_database_error = true;
                    break;
                }
            } else {
                ++summary.entries_skipped;
                ++summary.errors;
                summary.warnings.push_back(
                    "Unsupported scan root type: " + root_string);
                preserve_presence_for_partial_root(root_string);
            }
            continue;
        }

        bool root_persisted_as_project = false;
        if (options.protect_project_directories) {
            if (auto match = impl_->project_detector.detect(root);
                match && ProtectedProjectDetector::should_skip(*match)) {
                EntryMetadata metadata = build_metadata(
                    root,
                    kEntryTypeProtectedProject,
                    false,
                    match);
                if (!persist(metadata, root_string)) {
                    fatal_database_error = true;
                    break;
                }
                ++summary.protected_projects_indexed;
                root_persisted_as_project = true;

                if (!options.index_protected_project_contents) {
                    if (!finalize_root_presence(root_string)) {
                        fatal_database_error = true;
                    }
                    continue;
                }
            }
        }

        if (!root_persisted_as_project) {
            EntryMetadata root_metadata = build_metadata(
                root,
                kEntryTypeDirectory,
                false,
                std::nullopt);
            if (!persist(root_metadata, root_string)) {
                fatal_database_error = true;
                break;
            }
            ++summary.directories_indexed;
        }

        bool root_complete = true;
        std::vector<fs::path> pending_directories;
        pending_directories.push_back(root);

        while (!pending_directories.empty() && !fatal_database_error) {
            const fs::path current = std::move(pending_directories.back());
            pending_directories.pop_back();

            std::error_code iterator_ec;
            fs::directory_iterator iterator(current, fs::directory_options::none, iterator_ec);
            if (iterator_ec) {
                root_complete = false;
                ++summary.errors;
                summary.warnings.push_back(
                    "Could not enumerate directory: " +
                    Utils::path_to_utf8(current));
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
                    summary.warnings.push_back(
                        "Directory enumeration stopped early: " +
                        Utils::path_to_utf8(current));
                }

                if (!options.include_hidden && is_hidden(path)) {
                    ++summary.entries_skipped;
                    if (increment_ec) {
                        break;
                    }
                    continue;
                }

                if (!options.follow_reparse_points && is_reparse_or_symlink(entry)) {
                    ++summary.entries_skipped;
                    if (increment_ec) {
                        break;
                    }
                    continue;
                }

                std::error_code entry_dir_ec;
                const bool is_directory = entry.is_directory(entry_dir_ec);
                if (entry_dir_ec) {
                    root_complete = false;
                    ++summary.errors;
                    summary.warnings.push_back(
                        "Could not inspect entry: " + Utils::path_to_utf8(path));
                    if (increment_ec) {
                        break;
                    }
                    continue;
                }

                if (is_directory) {
                    if (is_windows_system_root(path)) {
                        ++summary.entries_skipped;
                        if (increment_ec) {
                            break;
                        }
                        continue;
                    }

                    if (exclusion_set.contains(comparison_name(name))) {
                        ++summary.entries_skipped;
                        if (increment_ec) {
                            break;
                        }
                        continue;
                    }

                    bool protected_project = false;
                    if (options.protect_project_directories) {
                        if (auto match = impl_->project_detector.detect(path);
                            match && ProtectedProjectDetector::should_skip(*match)) {
                            EntryMetadata metadata = build_metadata(
                                path,
                                kEntryTypeProtectedProject,
                                false,
                                match);
                            if (!persist(metadata, root_string)) {
                                fatal_database_error = true;
                                break;
                            }
                            ++summary.protected_projects_indexed;
                            protected_project = true;

                            if (options.index_protected_project_contents) {
                                pending_directories.push_back(path);
                            }
                        }
                    }

                    if (protected_project) {
                        if (increment_ec) {
                            break;
                        }
                        continue;
                    }

                    EntryMetadata metadata = build_metadata(
                        path,
                        kEntryTypeDirectory,
                        false,
                        std::nullopt);
                    if (!persist(metadata, root_string)) {
                        fatal_database_error = true;
                        break;
                    }
                    ++summary.directories_indexed;
                    pending_directories.push_back(path);
                } else {
                    std::error_code regular_ec;
                    const bool regular = entry.is_regular_file(regular_ec);
                    if (regular_ec) {
                        root_complete = false;
                        ++summary.errors;
                        summary.warnings.push_back(
                            "Could not inspect file entry: " +
                            Utils::path_to_utf8(path));
                        if (increment_ec) {
                            break;
                        }
                        continue;
                    }
                    if (!regular) {
                        ++summary.entries_skipped;
                        if (increment_ec) {
                            break;
                        }
                        continue;
                    }

                    EntryMetadata metadata = build_metadata(
                        path,
                        kEntryTypeFile,
                        options.compute_sha256,
                        std::nullopt);
                    if (!persist(metadata, root_string)) {
                        fatal_database_error = true;
                        break;
                    }
                    ++summary.files_indexed;
                    if (!metadata.error.empty()) {
                        ++summary.errors;
                        summary.warnings.push_back(
                            metadata.full_path + ": " + metadata.error);
                    }
                }

                if (increment_ec) {
                    break;
                }
            }
        }

        if (fatal_database_error) {
            break;
        }

        if (!commit_transaction()) {
            ++summary.errors;
            summary.warnings.push_back(
                "Could not commit final index batch for: " + root_string);
            fatal_database_error = true;
            break;
        }

        if (root_complete) {
            if (!finalize_root_presence(root_string)) {
                fatal_database_error = true;
                break;
            }
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
    const char* status = fatal_database_error
        ? "failed"
        : (partial_roots > 0 ? "partial" : "completed");
    impl_->finish_run(summary, status);
    return summary;
}
