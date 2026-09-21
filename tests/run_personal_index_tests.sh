#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/tests/build-personal-index"
mkdir -p "$BUILD_DIR"

TEST_SRC="$BUILD_DIR/personal_index_test.cpp"
UTILS_STUB_SRC="$BUILD_DIR/utils_path_stub.cpp"
OUTPUT="$BUILD_DIR/personal_index_test"

cat > "$TEST_SRC" <<'CPP'
#include "PersonalFileIndex.hpp"

#include <sqlite3.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {

struct TempDir {
    explicit TempDir(fs::path p) : path(std::move(p)) {}
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    fs::path path;
};

[[noreturn]] void fail(const std::string& message) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

std::string utf8_path(const fs::path& path) {
#ifdef _WIN32
    const auto value = path.u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
#else
    return path.string();
#endif
}

void write_file(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        fail("Could not create test file: " + utf8_path(path));
    }
    stream << content;
}

void exec_sql(sqlite3* db, const std::string& sql) {
    char* error = nullptr;
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
        const std::string message = error ? error : "unknown SQLite error";
        sqlite3_free(error);
        fail("SQLite error: " + message);
    }
}

std::int64_t scalar_int64(sqlite3* db, const std::string& sql) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
        fail("Could not prepare SQL: " + sql);
    }
    const int rc = sqlite3_step(statement);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(statement);
        fail("SQL returned no row: " + sql);
    }
    const auto value = sqlite3_column_int64(statement, 0);
    sqlite3_finalize(statement);
    return value;
}

std::string scalar_text(sqlite3* db, const std::string& sql) {
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
        fail("Could not prepare SQL: " + sql);
    }
    const int rc = sqlite3_step(statement);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(statement);
        fail("SQL returned no row: " + sql);
    }
    const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
    std::string value = text ? text : "";
    sqlite3_finalize(statement);
    return value;
}

std::string quote_sql(const std::string& value) {
    std::string result = "'";
    for (const char ch : value) {
        if (ch == '\'') {
            result += "''";
        } else {
            result.push_back(ch);
        }
    }
    result.push_back('\'');
    return result;
}

void mark_hidden(const fs::path& path) {
#ifdef _WIN32
    DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        !SetFileAttributesW(path.c_str(), attributes | FILE_ATTRIBUTE_HIDDEN)) {
        fail("Could not mark hidden test path: " + utf8_path(path));
    }
#else
    (void)path;
#endif
}

#ifdef _WIN32
fs::path windows_directory() {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetEnvironmentVariableW(
        L"WINDIR", buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        fail("Could not read WINDIR for Windows system-root test");
    }
    buffer.resize(length);
    return fs::path(buffer);
}
#endif

void create_legacy_v1_database(const fs::path& config, const fs::path& legacy_source) {
    fs::create_directories(config);
    const fs::path db_path = config / "personal_file_index.db";
    sqlite3* db = nullptr;
    if (sqlite3_open(utf8_path(db_path).c_str(), &db) != SQLITE_OK) {
        fail("Could not create legacy migration fixture");
    }

    const std::string source = utf8_path(legacy_source);
    exec_sql(db,
        "PRAGMA user_version=1;"
        "CREATE TABLE personal_index_scan_runs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, started_at_ms INTEGER NOT NULL, "
        "completed_at_ms INTEGER, status TEXT NOT NULL, roots TEXT NOT NULL, "
        "files_indexed INTEGER NOT NULL DEFAULT 0, directories_indexed INTEGER NOT NULL DEFAULT 0, "
        "protected_projects_indexed INTEGER NOT NULL DEFAULT 0, entries_skipped INTEGER NOT NULL DEFAULT 0, "
        "errors INTEGER NOT NULL DEFAULT 0);"
        "CREATE TABLE personal_index_entries ("
        "full_path TEXT PRIMARY KEY, parent_path TEXT NOT NULL, name TEXT NOT NULL, "
        "extension TEXT NOT NULL DEFAULT '', entry_type INTEGER NOT NULL, size_bytes INTEGER NOT NULL DEFAULT 0, "
        "created_at_ms INTEGER, modified_at_ms INTEGER, sha256 TEXT, "
        "hash_state TEXT NOT NULL DEFAULT 'not_requested', mime_type TEXT NOT NULL DEFAULT '', "
        "text_state TEXT NOT NULL DEFAULT 'not_analyzed', summary TEXT NOT NULL DEFAULT '', "
        "image_description TEXT NOT NULL DEFAULT '', content_language TEXT NOT NULL DEFAULT '', "
        "project_rule_id TEXT NOT NULL DEFAULT '', project_type TEXT NOT NULL DEFAULT '', "
        "project_reason TEXT NOT NULL DEFAULT '', scan_root TEXT NOT NULL, last_seen_run_id INTEGER NOT NULL, "
        "last_indexed_at_ms INTEGER NOT NULL, is_present INTEGER NOT NULL DEFAULT 1, error TEXT NOT NULL DEFAULT '');"
        "INSERT INTO personal_index_scan_runs(id,started_at_ms,completed_at_ms,status,roots) "
        "VALUES(1,1,2,'completed','legacy');"
        "INSERT INTO personal_index_entries("
        "full_path,parent_path,name,extension,entry_type,size_bytes,scan_root,last_seen_run_id,last_indexed_at_ms,is_present) "
        "VALUES(" + quote_sql(source) + "," + quote_sql(utf8_path(legacy_source.parent_path())) + ","
        + quote_sql(utf8_path(legacy_source.filename())) + ",'\.txt',0,5,'legacy',1,2,1);");
    sqlite3_close(db);
}

} // namespace

int main() {
    const auto unique = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    TempDir temp(fs::temp_directory_path() / ("aifs-personal-index-" + unique));
    const fs::path root = temp.path / "scan-root";
    const fs::path config = temp.path / "config";
    const fs::path arabic_dir = fs::path(u8"العربية");
    const fs::path arabic_file = fs::path(u8"كتاب.txt");
    const fs::path hidden_dir = root / ".hidden-dir";
    const fs::path hidden_file = root / ".hidden-file.txt";

    write_file(root / "document.txt", "personal index integration test\n");
    write_file(root / "ordinary" / "notes.md", "notes\n");
    write_file(root / "ExcludedStuff" / "ignore.txt", "ignore me\n");
    write_file(root / "Windows" / "user-not-system.txt", "ordinary user data\n");
    write_file(root / arabic_dir / arabic_file, "arabic path regression\n");
    write_file(hidden_dir / "inside.txt", "hidden child\n");
    write_file(hidden_file, "hidden direct\n");
    mark_hidden(hidden_dir);
    mark_hidden(hidden_file);

    write_file(root / "TaskApp" / "package.json", "{\"name\":\"task-app\"}\n");
    write_file(root / "TaskApp" / "src" / "main.js", "console.log('ok');\n");
    write_file(root / "TaskApp" / "node_modules" / "pkg" / "index.js", "generated\n");

    PersonalFileIndex index(utf8_path(config));
    if (!index.is_open()) {
        fail("Personal index database did not open");
    }

    // Invalid/unsafe root configurations are rejected before a scan run exists.
    PersonalFileIndexOptions unsafe_options;
    unsafe_options.follow_reparse_points = true;
    const auto unsafe = index.scan({utf8_path(root)}, unsafe_options);
    if (unsafe.completed || unsafe.run_id != -1 || unsafe.errors == 0) {
        fail("Core API accepted unsafe reparse-point traversal");
    }

    const auto duplicate_roots = index.scan(
        {utf8_path(root), utf8_path(root) + std::string(1, fs::path::preferred_separator)});
    if (duplicate_roots.completed || duplicate_roots.run_id != -1 || duplicate_roots.errors == 0) {
        fail("Duplicate roots with trailing-separator differences were not rejected");
    }

    const auto overlapping_roots = index.scan(
        {utf8_path(root), utf8_path(root / "ordinary")});
    if (overlapping_roots.completed || overlapping_roots.run_id != -1 ||
        overlapping_roots.errors == 0) {
        fail("Overlapping roots were not rejected");
    }

    PersonalFileIndexOptions options;
    options.compute_sha256 = true;
    options.include_hidden = true;
    options.excluded_directory_names = PersonalFileIndex::default_excluded_directory_names();
    options.commit_batch_size = 2;

    const auto first = index.scan({utf8_path(root)}, options);
    if (!first.completed) {
        fail("Initial scan did not complete");
    }
    if (first.files_indexed != 9) {
        fail("Expected 9 included files in initial scan, got " +
             std::to_string(first.files_indexed));
    }
    if (first.protected_projects_indexed != 1) {
        fail("Expected exactly 1 protected project");
    }
    if (first.directories_indexed != 7) {
        fail("Expected 7 included directories in initial scan, got " +
             std::to_string(first.directories_indexed));
    }

    if (!fs::exists(root / "document.txt") ||
        !fs::exists(root / "TaskApp" / "src" / "main.js")) {
        fail("Read-only scan changed source files");
    }

    sqlite3* db = nullptr;
    if (sqlite3_open(index.database_path().c_str(), &db) != SQLITE_OK) {
        fail("Could not open personal index SQLite database for verification");
    }

    const std::string root_utf8 = utf8_path(root);
    const std::string doc_path = utf8_path(root / "document.txt");
    const std::string project_path = utf8_path(root / "TaskApp");
    const std::string project_source_path = utf8_path(root / "TaskApp" / "src" / "main.js");
    const std::string generated_project_path =
        utf8_path(root / "TaskApp" / "node_modules" / "pkg" / "index.js");
    const std::string excluded_path = utf8_path(root / "ExcludedStuff" / "ignore.txt");
    const std::string ordinary_windows_named_path =
        utf8_path(root / "Windows" / "user-not-system.txt");
    const std::string arabic_path = utf8_path(root / arabic_dir / arabic_file);
    const std::string hidden_child_path = utf8_path(hidden_dir / "inside.txt");
    const std::string hidden_direct_path = utf8_path(hidden_file);

    if (scalar_int64(db, "PRAGMA user_version;") != 2) {
        sqlite3_close(db);
        fail("Fresh index database did not set schema user_version=2");
    }

    const auto present_count = scalar_int64(
        db,
        "SELECT COUNT(*) FROM personal_index_entries WHERE observation_state='present' AND scan_root=" +
            quote_sql(root_utf8) + ";");
    if (present_count != 18) {
        sqlite3_close(db);
        fail("Expected 18 physically present index rows after first scan, got " +
             std::to_string(present_count));
    }

    const auto windows_named_file_count = scalar_int64(
        db,
        "SELECT COUNT(*) FROM personal_index_entries WHERE full_path=" +
            quote_sql(ordinary_windows_named_path) + " AND observation_state='present';");
    if (windows_named_file_count != 1) {
        sqlite3_close(db);
        fail("A user-owned folder named Windows was incorrectly treated as a system root");
    }

    const auto arabic_file_count = scalar_int64(
        db,
        "SELECT COUNT(*) FROM personal_index_entries WHERE full_path=" +
            quote_sql(arabic_path) + " AND observation_state='present';");
    if (arabic_file_count != 1) {
        sqlite3_close(db);
        fail("Arabic/UTF-8 filesystem path was not persisted correctly");
    }

    const auto protected_type = scalar_int64(
        db,
        "SELECT entry_type FROM personal_index_entries WHERE full_path=" +
            quote_sql(project_path) + ";");
    if (protected_type != 2) {
        sqlite3_close(db);
        fail("Protected project row did not use protected-project entry type");
    }

    const std::string project_rule = scalar_text(
        db,
        "SELECT project_rule_id FROM personal_index_entries WHERE full_path=" +
            quote_sql(project_path) + ";");
    if (project_rule != "node") {
        sqlite3_close(db);
        fail("Expected Node.js project detection, got: " + project_rule);
    }

    const auto project_source_count = scalar_int64(
        db,
        "SELECT COUNT(*) FROM personal_index_entries WHERE full_path=" +
            quote_sql(project_source_path) + " AND observation_state='present';");
    if (project_source_count != 1) {
        sqlite3_close(db);
        fail("Useful source file inside protected project was not indexed");
    }

    const auto generated_project_count = scalar_int64(
        db,
        "SELECT COUNT(*) FROM personal_index_entries WHERE full_path=" +
            quote_sql(generated_project_path) + ";");
    if (generated_project_count != 0) {
        sqlite3_close(db);
        fail("node_modules contents should remain excluded from project indexing");
    }

    const std::string hash = scalar_text(
        db,
        "SELECT sha256 FROM personal_index_entries WHERE full_path=" + quote_sql(doc_path) + ";");
    if (hash.size() != 64) {
        sqlite3_close(db);
        fail("Expected a 64-character SHA-256 hash");
    }

#ifdef _WIN32
    PersonalFileIndex windows_policy_index(utf8_path(temp.path / "windows-policy-config"));
    if (!windows_policy_index.is_open()) {
        sqlite3_close(db);
        fail("Windows policy test database did not open");
    }
    const fs::path real_windows_root = windows_directory();
    const auto system_scan = windows_policy_index.scan({utf8_path(real_windows_root)});
    if (!system_scan.completed || system_scan.entries_skipped != 1 ||
        system_scan.files_indexed != 0 || system_scan.directories_indexed != 0) {
        sqlite3_close(db);
        fail("Actual WINDIR was not safely refused as a protected system root");
    }
#endif

    // Change scan policy. Existing paths that are intentionally skipped must not
    // be reclassified as physically missing.
    fs::remove(root / "ordinary" / "notes.md");
    PersonalFileIndexOptions second_options = options;
    second_options.compute_sha256 = false;
    second_options.include_hidden = false;
    second_options.excluded_directory_names.push_back("ExcludedStuff");
    const auto second = index.scan({utf8_path(root)}, second_options);
    if (!second.completed) {
        sqlite3_close(db);
        fail("Second scan did not complete");
    }

    const std::string deleted_path = utf8_path(root / "ordinary" / "notes.md");
    if (scalar_text(
            db,
            "SELECT observation_state FROM personal_index_entries WHERE full_path=" +
                quote_sql(deleted_path) + ";") != "missing") {
        sqlite3_close(db);
        fail("Genuinely deleted included file was not marked physically missing");
    }

    if (scalar_text(
            db,
            "SELECT observation_state FROM personal_index_entries WHERE full_path=" +
                quote_sql(excluded_path) + ";") != "unknown" ||
        scalar_text(
            db,
            "SELECT policy_state FROM personal_index_entries WHERE full_path=" +
                quote_sql(excluded_path) + ";") != "excluded") {
        sqlite3_close(db);
        fail("New exclusion policy incorrectly converted existing child to missing");
    }

    if (scalar_text(
            db,
            "SELECT observation_state FROM personal_index_entries WHERE full_path=" +
                quote_sql(hidden_child_path) + ";") != "unknown" ||
        scalar_text(
            db,
            "SELECT policy_state FROM personal_index_entries WHERE full_path=" +
                quote_sql(hidden_child_path) + ";") != "hidden") {
        sqlite3_close(db);
        fail("Hidden-directory child was not preserved as policy-skipped/unknown");
    }

    if (scalar_text(
            db,
            "SELECT observation_state FROM personal_index_entries WHERE full_path=" +
                quote_sql(hidden_direct_path) + ";") != "present" ||
        scalar_text(
            db,
            "SELECT policy_state FROM personal_index_entries WHERE full_path=" +
                quote_sql(hidden_direct_path) + ";") != "hidden") {
        sqlite3_close(db);
        fail("Direct hidden file should be observed present while excluded by policy");
    }

    // If the entire root becomes unavailable, no prior physical observation is
    // rewritten as missing simply because the scan could not inspect it.
    fs::remove_all(root);
    const auto third = index.scan({utf8_path(root)}, second_options);
    if (!third.completed || third.errors == 0) {
        sqlite3_close(db);
        fail("Missing-root scan should finish as a non-fatal partial run with an error");
    }

    if (scalar_text(
            db,
            "SELECT observation_state FROM personal_index_entries WHERE full_path=" +
                quote_sql(doc_path) + ";") != "present") {
        sqlite3_close(db);
        fail("Unavailable root incorrectly changed prior physical observation");
    }

    const std::string latest_status = scalar_text(
        db,
        "SELECT status FROM personal_index_scan_runs ORDER BY id DESC LIMIT 1;");
    if (latest_status != "partial") {
        sqlite3_close(db);
        fail("Unavailable root should persist a partial scan status, got: " + latest_status);
    }

    const auto run_count = scalar_int64(
        db, "SELECT COUNT(*) FROM personal_index_scan_runs;");
    if (run_count != 3) {
        sqlite3_close(db);
        fail("Rejected root configurations must not create scan runs");
    }
    sqlite3_close(db);

    // Migrate a real legacy v1 fixture transactionally and leave a recovery copy.
    const fs::path legacy_config = temp.path / "legacy-config";
    const fs::path legacy_source = temp.path / "legacy-source.txt";
    write_file(legacy_source, "legacy\n");
    create_legacy_v1_database(legacy_config, legacy_source);

    PersonalFileIndex migrated(utf8_path(legacy_config));
    if (!migrated.is_open()) {
        fail("Legacy v1 database did not migrate to the current schema");
    }

    sqlite3* migrated_db = nullptr;
    if (sqlite3_open(migrated.database_path().c_str(), &migrated_db) != SQLITE_OK) {
        fail("Could not open migrated database for verification");
    }
    if (scalar_int64(migrated_db, "PRAGMA user_version;") != 2) {
        sqlite3_close(migrated_db);
        fail("Schema migration did not advance user_version to 2");
    }
    if (scalar_text(
            migrated_db,
            "SELECT observation_state FROM personal_index_entries LIMIT 1;") != "present" ||
        scalar_text(
            migrated_db,
            "SELECT policy_state FROM personal_index_entries LIMIT 1;") != "included" ||
        scalar_text(
            migrated_db,
            "SELECT path_key FROM personal_index_entries LIMIT 1;").empty()) {
        sqlite3_close(migrated_db);
        fail("Legacy row was not backfilled into the v2 state model");
    }
    sqlite3_close(migrated_db);

    const fs::path backup = legacy_config / "personal_file_index.db.pre-migration-v1.bak";
    if (!fs::exists(backup) || fs::file_size(backup) == 0) {
        fail("Schema migration did not create a non-empty recovery backup");
    }

    std::cout << "Personal file index integration test passed" << std::endl;
    return 0;
}
CPP

cat > "$UTILS_STUB_SRC" <<'CPP'
#include "Utils.hpp"

#include <filesystem>
#include <string>

std::string Utils::path_to_utf8(const std::filesystem::path& path) {
#if defined(_WIN32)
    const auto value = path.u8string();
    return std::string(reinterpret_cast<const char*>(value.data()), value.size());
#else
    return path.string();
#endif
}

std::filesystem::path Utils::utf8_to_path(const std::string& utf8_path) {
#if defined(_WIN32)
    return std::filesystem::u8path(utf8_path);
#else
    return std::filesystem::path(utf8_path);
#endif
}
CPP

QT_HEADERS="$(qmake6 -query QT_INSTALL_HEADERS 2>/dev/null || true)"
QT_LIB_DIR="$(qmake6 -query QT_INSTALL_LIBS 2>/dev/null || true)"
if [[ -z "$QT_HEADERS" || -z "$QT_LIB_DIR" ]]; then
    echo "ERROR: qmake6 is required to locate Qt6 for personal index tests." >&2
    exit 1
fi

INCLUDES=(
    -I"$ROOT_DIR/app/include"
    -I"$QT_HEADERS"
    -I"$QT_HEADERS/QtCore"
)

LIBS=(
    -L"$QT_LIB_DIR"
    -lQt6Core
    -lsqlite3
    -pthread
)

g++ -std=c++20 -fPIC "${INCLUDES[@]}" \
    "$TEST_SRC" "$UTILS_STUB_SRC" \
    "$ROOT_DIR/app/lib/PersonalFileIndex.cpp" \
    "$ROOT_DIR/app/lib/ProtectedProjectDetector.cpp" \
    -o "$OUTPUT" "${LIBS[@]}"

"$OUTPUT"
