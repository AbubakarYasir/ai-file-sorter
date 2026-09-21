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

} // namespace

int main() {
    const auto unique = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    TempDir temp(fs::temp_directory_path() / ("aifs-personal-index-" + unique));
    const fs::path root = temp.path / "scan-root";
    const fs::path config = temp.path / "config";
    const fs::path arabic_dir = fs::path(u8"العربية");
    const fs::path arabic_file = fs::path(u8"كتاب.txt");

    write_file(root / "document.txt", "personal index integration test\n");
    write_file(root / "ordinary" / "notes.md", "notes\n");
    write_file(root / "ExcludedStuff" / "ignore.txt", "ignore me\n");

    // A user-owned directory merely named "Windows" is not the operating-system
    // root and must not be excluded just because of its basename.
    write_file(root / "Windows" / "user-not-system.txt", "ordinary user data\n");

    // Multilingual/Islamic-library use is a primary fork requirement. Keep a
    // non-ASCII path in the core indexing regression suite so UTF-8 path storage
    // is exercised on both Linux and Windows.
    write_file(root / arabic_dir / arabic_file, "arabic path regression\n");

    // A recognized Node.js project is marked as protected metadata while its
    // useful source files remain visible to the read-only index by default.
    write_file(root / "TaskApp" / "package.json", "{\"name\":\"task-app\"}\n");
    write_file(root / "TaskApp" / "src" / "main.js", "console.log('ok');\n");
    write_file(root / "TaskApp" / "node_modules" / "pkg" / "index.js", "generated\n");

    PersonalFileIndex index(utf8_path(config));
    if (!index.is_open()) {
        fail("Personal index database did not open");
    }

    PersonalFileIndexOptions options;
    options.compute_sha256 = true;
    options.excluded_directory_names = PersonalFileIndex::default_excluded_directory_names();
    options.excluded_directory_names.push_back("ExcludedStuff");
    options.commit_batch_size = 2;

    const auto first = index.scan({utf8_path(root)}, options);
    if (!first.completed) {
        fail("Initial scan did not complete");
    }
    if (first.files_indexed != 6) {
        fail("Expected 6 useful files including project, Windows-named, and Arabic paths, got " +
             std::to_string(first.files_indexed));
    }
    if (first.protected_projects_indexed != 1) {
        fail("Expected exactly 1 protected project");
    }
    if (first.directories_indexed < 5) {
        fail("Expected normal, multilingual, and project source directories to be indexed");
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

    const auto present_count = scalar_int64(
        db,
        "SELECT COUNT(*) FROM personal_index_entries WHERE is_present=1 AND scan_root=" +
            quote_sql(root_utf8) + ";");
    if (present_count != 12) {
        sqlite3_close(db);
        fail("Expected 12 present index rows after first scan, got " +
             std::to_string(present_count));
    }

    const auto windows_named_file_count = scalar_int64(
        db,
        "SELECT COUNT(*) FROM personal_index_entries WHERE full_path=" +
            quote_sql(ordinary_windows_named_path) + " AND is_present=1;");
    if (windows_named_file_count != 1) {
        sqlite3_close(db);
        fail("A user-owned folder named Windows was incorrectly treated as a system root");
    }

    const auto arabic_file_count = scalar_int64(
        db,
        "SELECT COUNT(*) FROM personal_index_entries WHERE full_path=" +
            quote_sql(arabic_path) + " AND is_present=1;");
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
            quote_sql(project_source_path) + " AND is_present=1;");
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
        fail("node_modules should remain excluded from project indexing");
    }

    const std::string hash = scalar_text(
        db,
        "SELECT sha256 FROM personal_index_entries WHERE full_path=" +
            quote_sql(doc_path) + ";");
    if (hash.size() != 64) {
        sqlite3_close(db);
        fail("Expected a 64-character SHA-256 hash");
    }

    const auto excluded_count = scalar_int64(
        db,
        "SELECT COUNT(*) FROM personal_index_entries WHERE full_path=" +
            quote_sql(excluded_path) + ";");
    if (excluded_count != 0) {
        sqlite3_close(db);
        fail("Explicitly excluded directory was traversed");
    }

#ifdef _WIN32
    // Exercise the production Windows system-root policy directly. This call
    // must be refused before directory traversal, so CI never enumerates WINDIR.
    PersonalFileIndex windows_policy_index(utf8_path(temp.path / "windows-policy-config"));
    if (!windows_policy_index.is_open()) {
        sqlite3_close(db);
        fail("Windows policy test database did not open");
    }
    PersonalFileIndexOptions windows_policy_options;
    const fs::path real_windows_root = windows_directory();
    const auto system_scan = windows_policy_index.scan(
        {utf8_path(real_windows_root)}, windows_policy_options);
    if (!system_scan.completed || system_scan.entries_skipped != 1 ||
        system_scan.files_indexed != 0 || system_scan.directories_indexed != 0) {
        sqlite3_close(db);
        fail("Actual WINDIR was not safely refused as a protected system root");
    }
#endif

    // A subsequent complete scan should mark a genuinely deleted file as no
    // longer present without deleting its historical index row.
    fs::remove(root / "ordinary" / "notes.md");
    PersonalFileIndexOptions second_options = options;
    second_options.compute_sha256 = false;
    const auto second = index.scan({utf8_path(root)}, second_options);
    if (!second.completed) {
        sqlite3_close(db);
        fail("Second scan did not complete");
    }

    const std::string deleted_path = utf8_path(root / "ordinary" / "notes.md");
    const auto deleted_present = scalar_int64(
        db,
        "SELECT is_present FROM personal_index_entries WHERE full_path=" +
            quote_sql(deleted_path) + ";");
    if (deleted_present != 0) {
        sqlite3_close(db);
        fail("Deleted file was not marked stale on the next complete scan");
    }

    // If the entire root becomes unavailable, the index must not reinterpret
    // that failed observation as evidence that every file was deleted.
    fs::remove_all(root);
    const auto third = index.scan({utf8_path(root)}, second_options);
    if (!third.completed) {
        sqlite3_close(db);
        fail("Missing-root scan should finish as a non-fatal partial run");
    }
    if (third.errors == 0) {
        sqlite3_close(db);
        fail("Missing-root scan should report an access/root error");
    }

    const auto document_still_present = scalar_int64(
        db,
        "SELECT is_present FROM personal_index_entries WHERE full_path=" +
            quote_sql(doc_path) + ";");
    if (document_still_present != 1) {
        sqlite3_close(db);
        fail("Unavailable root incorrectly marked previously present files as missing");
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
        fail("Expected exactly three persisted scan runs");
    }

    sqlite3_close(db);
    std::cout << "Personal file index integration test passed" << std::endl;
    return 0;
}
CPP

# This focused integration test only needs the UTF-8 path helpers from Utils.
# Compiling all of Utils.cpp would pull unrelated networking/GPU/logger code into
# the test and make this test depend on subsystems it is not exercising.
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
