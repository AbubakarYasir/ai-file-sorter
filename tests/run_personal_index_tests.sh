#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/tests/build-personal-index"
mkdir -p "$BUILD_DIR"
mkdir -p "$BUILD_DIR/spdlog/sinks"
mkdir -p "$BUILD_DIR/spdlog/fmt"

TEST_SRC="$BUILD_DIR/personal_index_test.cpp"
STUB_SRC="$BUILD_DIR/logger_stub.cpp"
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

void write_file(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        fail("Could not create test file: " + path.string());
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

} // namespace

int main() {
    const auto unique = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    TempDir temp(fs::temp_directory_path() / ("aifs-personal-index-" + unique));
    const fs::path root = temp.path / "scan-root";
    const fs::path config = temp.path / "config";

    write_file(root / "document.txt", "personal index integration test\n");
    write_file(root / "ordinary" / "notes.md", "notes\n");
    write_file(root / "ExcludedStuff" / "ignore.txt", "ignore me\n");

    // A recognized Node.js project must be indexed as one protected unit.
    write_file(root / "TaskApp" / "package.json", "{\"name\":\"task-app\"}\n");
    write_file(root / "TaskApp" / "src" / "main.js", "console.log('ok');\n");

    PersonalFileIndex index(config.string());
    if (!index.is_open()) {
        fail("Personal index database did not open");
    }

    PersonalFileIndexOptions options;
    options.compute_sha256 = true;
    options.excluded_directory_names = PersonalFileIndex::default_excluded_directory_names();
    options.excluded_directory_names.push_back("ExcludedStuff");
    options.commit_batch_size = 2;

    const auto first = index.scan({root.string()}, options);
    if (!first.completed) {
        fail("Initial scan did not complete");
    }
    if (first.files_indexed != 2) {
        fail("Expected 2 normal files, got " + std::to_string(first.files_indexed));
    }
    if (first.protected_projects_indexed != 1) {
        fail("Expected exactly 1 protected project");
    }
    if (first.directories_indexed < 2) {
        fail("Expected root and ordinary directory to be indexed");
    }

    if (!fs::exists(root / "document.txt") || !fs::exists(root / "TaskApp" / "src" / "main.js")) {
        fail("Read-only scan changed source files");
    }

    sqlite3* db = nullptr;
    if (sqlite3_open(index.database_path().c_str(), &db) != SQLITE_OK) {
        fail("Could not open personal index SQLite database for verification");
    }

    const std::string root_utf8 = root.string();
    const std::string doc_path = (root / "document.txt").string();
    const std::string project_path = (root / "TaskApp").string();
    const std::string excluded_path = (root / "ExcludedStuff" / "ignore.txt").string();

    const auto present_count = scalar_int64(
        db,
        "SELECT COUNT(*) FROM personal_index_entries WHERE is_present=1 AND scan_root=" +
            quote_sql(root_utf8) + ";");
    if (present_count != 5) {
        sqlite3_close(db);
        fail("Expected 5 present index rows after first scan, got " + std::to_string(present_count));
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

    // A subsequent scan should mark a deleted file as no longer present without
    // deleting its historical index row.
    fs::remove(root / "ordinary" / "notes.md");
    PersonalFileIndexOptions second_options = options;
    second_options.compute_sha256 = false;
    const auto second = index.scan({root.string()}, second_options);
    if (!second.completed) {
        sqlite3_close(db);
        fail("Second scan did not complete");
    }

    const std::string deleted_path = (root / "ordinary" / "notes.md").string();
    const auto deleted_present = scalar_int64(
        db,
        "SELECT is_present FROM personal_index_entries WHERE full_path=" +
            quote_sql(deleted_path) + ";");
    if (deleted_present != 0) {
        sqlite3_close(db);
        fail("Deleted file was not marked stale on the next scan");
    }

    const auto run_count = scalar_int64(db, "SELECT COUNT(*) FROM personal_index_scan_runs;");
    if (run_count != 2) {
        sqlite3_close(db);
        fail("Expected exactly two persisted scan runs");
    }

    sqlite3_close(db);
    std::cout << "Personal file index integration test passed" << std::endl;
    return 0;
}
CPP

cat > "$STUB_SRC" <<'CPP'
#include "Logger.hpp"
#include <filesystem>

std::string Logger::get_log_directory() {
    return std::filesystem::temp_directory_path().string();
}

void Logger::setup_loggers() {}

std::shared_ptr<spdlog::logger> Logger::get_logger(const std::string&) {
    return nullptr;
}

std::string Logger::get_log_file_path(const std::string& log_dir, const std::string& log_name) {
    return log_dir + "/" + log_name;
}
CPP

cat > "$BUILD_DIR/spdlog/spdlog.h" <<'CPP'
#pragma once
#include <chrono>
#include <memory>
#include <string>

namespace spdlog {
namespace level {
enum level_enum {
    trace,
    debug,
    info,
    warn,
    err,
    critical,
    off
};
}

class logger {
public:
    template <typename... Args>
    void log(level::level_enum, const std::string&, Args&&...) {}

    template <typename... Args>
    void info(const std::string&, Args&&...) {}

    template <typename... Args>
    void warn(const std::string&, Args&&...) {}

    template <typename... Args>
    void error(const std::string&, Args&&...) {}

    void flush_on(level::level_enum) {}
    void set_level(level::level_enum) {}
};

inline std::shared_ptr<logger> get(const std::string&) { return nullptr; }
inline void register_logger(std::shared_ptr<logger>) {}
inline void flush_every(std::chrono::seconds) {}
inline void set_level(level::level_enum) {}
inline void info(const std::string&) {}
} // namespace spdlog
CPP

cat > "$BUILD_DIR/spdlog/sinks/stdout_color_sinks.h" <<'CPP'
#pragma once
namespace spdlog { namespace sinks { class stdout_color_sink_mt {}; } }
CPP

cat > "$BUILD_DIR/spdlog/sinks/basic_file_sink.h" <<'CPP'
#pragma once
namespace spdlog { namespace sinks { class basic_file_sink_mt {}; } }
CPP

cat > "$BUILD_DIR/spdlog/sinks/rotating_file_sink.h" <<'CPP'
#pragma once
namespace spdlog { namespace sinks { class rotating_file_sink_mt {}; } }
CPP

cat > "$BUILD_DIR/spdlog/fmt/fmt.h" <<'CPP'
#pragma once
#include <string>
namespace fmt {
inline const std::string& runtime(const std::string& value) { return value; }
template <typename... Args>
std::string format(const std::string& fmt_str, Args&&...) { return fmt_str; }
} // namespace fmt
CPP

QT_HEADERS="$(qmake6 -query QT_INSTALL_HEADERS 2>/dev/null || true)"
QT_LIB_DIR="$(qmake6 -query QT_INSTALL_LIBS 2>/dev/null || true)"
if [[ -z "$QT_HEADERS" || -z "$QT_LIB_DIR" ]]; then
    echo "ERROR: qmake6 is required to locate Qt6 for personal index tests." >&2
    exit 1
fi

INCLUDES=(
    -I"$BUILD_DIR"
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
    "$TEST_SRC" "$STUB_SRC" \
    "$ROOT_DIR/app/lib/PersonalFileIndex.cpp" \
    "$ROOT_DIR/app/lib/ProtectedProjectDetector.cpp" \
    "$ROOT_DIR/app/lib/Utils.cpp" \
    -o "$OUTPUT" "${LIBS[@]}"

"$OUTPUT"
