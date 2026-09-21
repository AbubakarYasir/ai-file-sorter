#include "PersonalIndexCommand.hpp"

#include "AnalysisRuntimeLock.hpp"
#include "PersonalFileIndexQuery.hpp"
#include "Utils.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

std::optional<std::string> inline_value(
    const std::string& argument,
    const std::string& option)
{
    const std::string prefix = option + "=";
    if (argument.rfind(prefix, 0) != 0) {
        return std::nullopt;
    }
    return argument.substr(prefix.size());
}

std::string make_job_id()
{
    const auto ticks = std::chrono::system_clock::now().time_since_epoch();
    return "personal-index-" + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(ticks).count());
}

void append_warning_array(QJsonObject* payload, const std::vector<std::string>& warnings)
{
    QJsonArray values;
    for (const auto& warning : warnings) {
        values.append(QString::fromStdString(warning));
    }
    payload->insert(QStringLiteral("warnings"), values);
}

void append_root_array(QJsonObject* payload, const std::vector<std::filesystem::path>& roots)
{
    QJsonArray values;
    for (const auto& root : roots) {
        values.append(QString::fromStdString(Utils::path_to_utf8(root)));
    }
    payload->insert(QStringLiteral("roots"), values);
}

QJsonObject stats_to_json(const PersonalFileIndexStats& stats)
{
    QJsonObject result;
    result.insert(QStringLiteral("totalEntries"), static_cast<qint64>(stats.total_entries));
    result.insert(QStringLiteral("presentEntries"), static_cast<qint64>(stats.present_entries));
    result.insert(QStringLiteral("presentFiles"), static_cast<qint64>(stats.present_files));
    result.insert(
        QStringLiteral("presentDirectories"),
        static_cast<qint64>(stats.present_directories));
    result.insert(
        QStringLiteral("presentProtectedProjects"),
        static_cast<qint64>(stats.present_protected_projects));
    result.insert(QStringLiteral("staleEntries"), static_cast<qint64>(stats.stale_entries));
    result.insert(QStringLiteral("hashedFiles"), static_cast<qint64>(stats.hashed_files));
    result.insert(QStringLiteral("presentBytes"), static_cast<qint64>(stats.present_bytes));
    result.insert(QStringLiteral("latestRunId"), static_cast<qint64>(stats.latest_run_id));
    result.insert(
        QStringLiteral("latestRunStatus"),
        QString::fromStdString(stats.latest_run_status));
    result.insert(
        QStringLiteral("latestRunErrors"),
        static_cast<qint64>(stats.latest_run_errors));
    return result;
}

std::string status_for_scan(
    const PersonalFileIndexScanSummary& summary,
    const std::optional<PersonalFileIndexStats>& stats)
{
    if (!summary.completed) {
        return "failed";
    }
    if (stats && !stats->latest_run_status.empty()) {
        return stats->latest_run_status;
    }
    return summary.errors > 0 ? "partial" : "completed";
}

} // namespace

PersonalIndexCommand::ParseResult PersonalIndexCommand::parse(int argc, char** argv)
{
    ParseResult result;
    result.consumed_arguments.assign(static_cast<std::size_t>(std::max(argc, 0)), false);

    if (argc < 2 || !argv || !argv[1] || std::string(argv[1]) != "index") {
        return result;
    }

    result.requested = true;
    result.consumed_arguments[1] = true;

    for (int i = 2; i < argc; ++i) {
        if (!argv[i]) {
            continue;
        }

        const std::string argument = argv[i];
        result.consumed_arguments[static_cast<std::size_t>(i)] = true;

        if (argument == "--help" || argument == "-h") {
            result.help_requested = true;
            continue;
        }
        if (argument == "--json") {
            result.options.json_output = true;
            continue;
        }
        if (argument == "--include-hidden") {
            result.options.scan_options.include_hidden = true;
            continue;
        }
        if (argument == "--follow-reparse-points") {
            result.options.scan_options.follow_reparse_points = true;
            continue;
        }
        if (argument == "--project-root-only") {
            result.options.scan_options.index_protected_project_contents = false;
            continue;
        }
        if (argument == "--no-project-protection") {
            result.options.scan_options.protect_project_directories = false;
            result.options.scan_options.index_protected_project_contents = true;
            continue;
        }

        if (argument == "--root") {
            if (i + 1 >= argc || !argv[i + 1]) {
                result.error = "Missing value for --root.";
                break;
            }
            ++i;
            result.consumed_arguments[static_cast<std::size_t>(i)] = true;
            result.options.roots.emplace_back(argv[i]);
            continue;
        }
        if (const auto value = inline_value(argument, "--root")) {
            if (value->empty()) {
                result.error = "Missing value for --root.";
                break;
            }
            result.options.roots.emplace_back(*value);
            continue;
        }

        if (argument == "--hash") {
            if (i + 1 >= argc || !argv[i + 1]) {
                result.error = "Missing value for --hash (supported: off, all).";
                break;
            }
            ++i;
            result.consumed_arguments[static_cast<std::size_t>(i)] = true;
            const std::string value = argv[i];
            if (value == "off") {
                result.options.scan_options.compute_sha256 = false;
            } else if (value == "all") {
                result.options.scan_options.compute_sha256 = true;
            } else {
                result.error = "Unsupported --hash value '" + value + "' (supported: off, all).";
                break;
            }
            continue;
        }
        if (const auto value = inline_value(argument, "--hash")) {
            if (*value == "off") {
                result.options.scan_options.compute_sha256 = false;
            } else if (*value == "all") {
                result.options.scan_options.compute_sha256 = true;
            } else {
                result.error = "Unsupported --hash value '" + *value + "' (supported: off, all).";
                break;
            }
            continue;
        }

        if (!argument.empty() && argument.front() == '-') {
            result.error = "Unknown index option: " + argument;
            break;
        }

        result.options.roots.emplace_back(argument);
    }

    if (!result.help_requested && result.error.empty() && result.options.roots.empty()) {
        result.error = "The index command requires at least one filesystem root.";
    }

    return result;
}

std::string PersonalIndexCommand::usage_text()
{
    return
        "Usage:\n"
        "  aifilesorter index <root> [<root> ...] [options]\n"
        "  aifilesorter index --root <path> [--root <path> ...] [options]\n\n"
        "Read-only personal filesystem indexing options:\n"
        "  --json                     Emit machine-readable JSON.\n"
        "  --hash <off|all>           SHA-256 policy (default: off).\n"
        "  --include-hidden           Include hidden filesystem entries.\n"
        "  --follow-reparse-points    Follow symlinks/reparse points (advanced).\n"
        "  --project-root-only        Mark protected projects without indexing contents.\n"
        "  --no-project-protection    Disable project protection metadata/detection.\n"
        "  --help, -h                 Show this help.\n\n"
        "The index command does not move, rename, delete, or edit scanned source files.\n";
}

int PersonalIndexCommand::run(
    const Options& options,
    const std::filesystem::path& config_dir,
    const std::filesystem::path& runtime_dir,
    std::ostream& out,
    std::ostream& err)
{
    if (options.roots.empty()) {
        err << "No index roots were provided.\n" << usage_text();
        return Usage;
    }

    AnalysisRuntimeLock runtime_lock(runtime_dir);
    AnalysisRuntimeLock::Metadata metadata;
    metadata.owner = AnalysisRuntimeLock::Owner::Headless;
    metadata.job_id = make_job_id();
    metadata.description = "Personal filesystem index";

    std::string lock_error;
    auto lease = runtime_lock.try_acquire(std::move(metadata), &lock_error);
    if (!lease) {
        err << (lock_error.empty() ? "Another AI File Sorter job is already running." : lock_error)
            << '\n';
        return Busy;
    }

    const std::string config_dir_utf8 = Utils::path_to_utf8(config_dir);
    PersonalFileIndex index(config_dir_utf8);
    if (!index.is_open()) {
        err << "Could not open the personal file index database.\n";
        return Failure;
    }

    std::vector<std::string> roots;
    roots.reserve(options.roots.size());
    for (const auto& root : options.roots) {
        roots.push_back(Utils::path_to_utf8(root));
    }

    const PersonalFileIndexScanSummary summary = index.scan(roots, options.scan_options);

    PersonalFileIndexQuery query(index.database_path());
    const auto stats = query.stats();
    const std::string status = status_for_scan(summary, stats);

    if (options.json_output) {
        QJsonObject payload;
        payload.insert(QStringLiteral("kind"), QStringLiteral("aifs.personalIndexResult"));
        payload.insert(QStringLiteral("status"), QString::fromStdString(status));
        payload.insert(QStringLiteral("runId"), static_cast<qint64>(summary.run_id));
        payload.insert(
            QStringLiteral("filesIndexed"),
            static_cast<qint64>(summary.files_indexed));
        payload.insert(
            QStringLiteral("directoriesIndexed"),
            static_cast<qint64>(summary.directories_indexed));
        payload.insert(
            QStringLiteral("protectedProjects"),
            static_cast<qint64>(summary.protected_projects_indexed));
        payload.insert(
            QStringLiteral("skipped"),
            static_cast<qint64>(summary.entries_skipped));
        payload.insert(QStringLiteral("errors"), static_cast<qint64>(summary.errors));
        payload.insert(
            QStringLiteral("database"),
            QString::fromStdString(index.database_path()));
        append_root_array(&payload, options.roots);
        append_warning_array(&payload, summary.warnings);
        if (stats) {
            payload.insert(QStringLiteral("stats"), stats_to_json(*stats));
        }

        out << QJsonDocument(payload).toJson(QJsonDocument::Compact).toStdString() << '\n';
    } else {
        out << "Personal file index: " << status << '\n'
            << "Database: " << index.database_path() << '\n'
            << "Files indexed this run: " << summary.files_indexed << '\n'
            << "Directories indexed this run: " << summary.directories_indexed << '\n'
            << "Protected projects: " << summary.protected_projects_indexed << '\n'
            << "Skipped entries: " << summary.entries_skipped << '\n'
            << "Errors: " << summary.errors << '\n';
        if (stats) {
            out << "Present files: " << stats->present_files << '\n'
                << "Present directories: " << stats->present_directories << '\n'
                << "Present protected projects: " << stats->present_protected_projects << '\n'
                << "Stale entries: " << stats->stale_entries << '\n';
        }
        for (const auto& warning : summary.warnings) {
            out << "Warning: " << warning << '\n';
        }
    }

    if (!summary.completed || status == "failed") {
        return Failure;
    }
    if (status == "partial") {
        return Partial;
    }
    return Success;
}
