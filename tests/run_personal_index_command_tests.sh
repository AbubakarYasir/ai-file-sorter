#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/tests/build-personal-index-command"
mkdir -p "$BUILD_DIR"

TEST_SRC="$BUILD_DIR/personal_index_command_test.cpp"
UTILS_STUB_SRC="$BUILD_DIR/utils_path_stub.cpp"
OUTPUT="$BUILD_DIR/personal_index_command_test"

cat > "$TEST_SRC" <<'CPP'
#include "AnalysisRuntimeLock.hpp"
#include "PersonalIndexCommand.hpp"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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
        fail("Could not create test file");
    }
    stream << content;
}

PersonalIndexCommand::ParseResult parse_args(std::vector<std::string> values) {
    std::vector<char*> argv;
    argv.reserve(values.size());
    for (auto& value : values) {
        argv.push_back(value.data());
    }
    return PersonalIndexCommand::parse(static_cast<int>(argv.size()), argv.data());
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    {
        const auto parsed = parse_args({
            "aifilesorter", "index", "/tmp/root-a", "--root", "/tmp/root-b",
            "--json", "--hash", "all", "--project-root-only"});
        if (!parsed.requested || !parsed.error.empty() || parsed.options.roots.size() != 2 ||
            !parsed.options.json_output || !parsed.options.scan_options.compute_sha256 ||
            parsed.options.scan_options.index_protected_project_contents) {
            fail("Index command parser did not preserve expected options");
        }
    }

    {
        const auto parsed = parse_args({"aifilesorter", "index", "--hash", "smart", "/tmp/root"});
        if (!parsed.requested || parsed.error.empty()) {
            fail("Unsupported hash mode should be rejected until implemented");
        }
    }

    {
        const auto parsed = parse_args({"aifilesorter", "index", "--json"});
        if (!parsed.requested || parsed.error.empty()) {
            fail("Index command without a root should be a usage error");
        }
    }

    const auto unique = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    TempDir temp(fs::temp_directory_path() / ("aifs-personal-command-" + unique));
    const fs::path root = temp.path / "root";
    const fs::path config = temp.path / "config";
    const fs::path runtime = temp.path / "runtime";
    write_file(root / "hello.txt", "hello\n");

    PersonalIndexCommand::Options options;
    options.roots.push_back(root);
    options.json_output = true;

    std::ostringstream out;
    std::ostringstream err;
    const int result = PersonalIndexCommand::run(options, config, runtime, out, err);
    if (result != PersonalIndexCommand::Success) {
        fail("Index command did not succeed: " + err.str());
    }
    if (!fs::exists(root / "hello.txt")) {
        fail("Index command mutated source files");
    }

    QJsonParseError parse_error;
    const QJsonDocument json = QJsonDocument::fromJson(
        QByteArray::fromStdString(out.str()), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !json.isObject()) {
        fail("Index command did not emit valid JSON");
    }
    const QJsonObject object = json.object();
    if (object.value(QStringLiteral("kind")).toString() !=
            QStringLiteral("aifs.personalIndexResult") ||
        object.value(QStringLiteral("status")).toString() != QStringLiteral("completed") ||
        object.value(QStringLiteral("filesIndexed")).toInteger() != 1 ||
        !object.value(QStringLiteral("stats")).isObject()) {
        fail("Index JSON contract did not contain expected values");
    }

    // The personal index command participates in the same runtime lock used by
    // GUI/headless analysis. A second command must fail as busy rather than race.
    AnalysisRuntimeLock lock(runtime);
    AnalysisRuntimeLock::Metadata metadata;
    metadata.owner = AnalysisRuntimeLock::Owner::Headless;
    metadata.job_id = "test-holder";
    metadata.description = "test lock";
    std::string lock_error;
    auto lease = lock.try_acquire(metadata, &lock_error);
    if (!lease) {
        fail("Could not acquire test runtime lock: " + lock_error);
    }

    std::ostringstream busy_out;
    std::ostringstream busy_err;
    const int busy_result = PersonalIndexCommand::run(
        options, config, runtime, busy_out, busy_err);
    if (busy_result != PersonalIndexCommand::Busy) {
        fail("Concurrent index command did not return Busy");
    }

    std::cout << "Personal index command test passed" << std::endl;
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
    echo "ERROR: qmake6 is required to locate Qt6 for personal index command tests." >&2
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
    "$ROOT_DIR/app/lib/PersonalIndexCommand.cpp" \
    "$ROOT_DIR/app/lib/PersonalFileIndex.cpp" \
    "$ROOT_DIR/app/lib/PersonalFileIndexQuery.cpp" \
    "$ROOT_DIR/app/lib/ProtectedProjectDetector.cpp" \
    "$ROOT_DIR/app/lib/AnalysisRuntimeLock.cpp" \
    -o "$OUTPUT" "${LIBS[@]}"

"$OUTPUT"
