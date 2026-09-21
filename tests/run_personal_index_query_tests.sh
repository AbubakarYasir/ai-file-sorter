#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/tests/build-personal-index-query"
mkdir -p "$BUILD_DIR"

TEST_SRC="$BUILD_DIR/personal_index_query_test.cpp"
OUTPUT="$BUILD_DIR/personal_index_query_test"

cat > "$TEST_SRC" <<'CPP'
#include "PersonalFileIndexQuery.hpp"

#include <sqlite3.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

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

void exec(sqlite3* db, const char* sql) {
    char* error = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) {
        std::string message = error ? error : "unknown SQLite error";
        sqlite3_free(error);
        fail(message);
    }
}

} // namespace

int main() {
    const auto unique = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    TempDir temp(fs::temp_directory_path() / ("aifs-personal-query-" + unique));
    fs::create_directories(temp.path);

    const fs::path missing_db = temp.path / "missing.db";
    {
        PersonalFileIndexQuery missing_query(missing_db.string());
        if (missing_query.is_open()) {
            fail("Read-only query unexpectedly opened a nonexistent database");
        }
        if (fs::exists(missing_db)) {
            fail("Read-only query created a nonexistent database");
        }
    }

    const fs::path db_path = temp.path / "personal_file_index.db";
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path.string().c_str(), &db) != SQLITE_OK) {
        fail("Could not create query test database");
    }

    exec(db,
        "CREATE TABLE personal_index_entries ("
        "full_path TEXT PRIMARY KEY, entry_type INTEGER NOT NULL, size_bytes INTEGER NOT NULL, "
        "sha256 TEXT, hash_state TEXT NOT NULL, observation_state TEXT NOT NULL, "
        "policy_state TEXT NOT NULL);"
        "CREATE TABLE personal_index_scan_runs ("
        "id INTEGER PRIMARY KEY, status TEXT NOT NULL, errors INTEGER NOT NULL);"
        "INSERT INTO personal_index_entries VALUES"
        "('/a.txt',0,100,'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa','complete','present','included'),"
        "('/b.pdf',0,250,NULL,'not_requested','present','included'),"
        "('/folder',1,0,NULL,'not_requested','present','included'),"
        "('/repo',2,0,NULL,'not_requested','present','protected'),"
        "('/gone.txt',0,999,NULL,'not_requested','missing','included'),"
        "('/hidden/child.txt',0,42,NULL,'not_requested','unknown','hidden');"
        "INSERT INTO personal_index_scan_runs VALUES(4,'completed',0);"
        "INSERT INTO personal_index_scan_runs VALUES(5,'partial',2);");
    sqlite3_close(db);

    PersonalFileIndexQuery query(db_path.string());
    if (!query.is_open()) {
        fail("Query facade did not open fixture database");
    }

    const auto stats = query.stats();
    if (!stats) {
        fail("Query facade did not return statistics");
    }

    if (stats->total_entries != 6 ||
        stats->present_entries != 4 ||
        stats->present_files != 2 ||
        stats->present_directories != 1 ||
        stats->present_protected_projects != 1 ||
        stats->missing_entries != 1 ||
        stats->stale_entries != 1 ||
        stats->unknown_entries != 1 ||
        stats->policy_skipped_entries != 2 ||
        stats->hashed_files != 1 ||
        stats->present_bytes != 350) {
        fail("Aggregate index statistics were incorrect");
    }

    if (stats->latest_run_id != 5 ||
        stats->latest_run_status != "partial" ||
        stats->latest_run_errors != 2) {
        fail("Latest scan-run statistics were incorrect");
    }

    std::cout << "Personal file index query test passed" << std::endl;
    return 0;
}
CPP

g++ -std=c++20 -fPIC \
    -I"$ROOT_DIR/app/include" \
    "$TEST_SRC" \
    "$ROOT_DIR/app/lib/PersonalFileIndexQuery.cpp" \
    -lsqlite3 \
    -o "$OUTPUT"

"$OUTPUT"
