#pragma once

#include "PersonalFileIndex.hpp"

#include <filesystem>
#include <ostream>
#include <string>
#include <vector>

/**
 * @brief Power-user CLI entry point for the personal read-only filesystem index.
 *
 * This command is intentionally separate from HeadlessAnalysisCommand because it
 * does not categorize, rename, move, or apply review plans. It updates only the
 * personal index database while reading explicitly supplied filesystem roots.
 */
class PersonalIndexCommand {
public:
    enum ExitCode {
        Success = 0,
        Failure = 1,
        Usage = 2,
        Busy = 3,
        Partial = 5
    };

    struct Options {
        std::vector<std::filesystem::path> roots;
        bool json_output{false};
        PersonalFileIndexOptions scan_options;
    };

    struct ParseResult {
        bool requested{false};
        bool help_requested{false};
        Options options;
        std::string error;
        std::vector<bool> consumed_arguments;
    };

    /**
     * Parse UTF-8 command arguments for the `index` subcommand.
     *
     * On Windows, filesystem-root strings are explicitly converted from UTF-8
     * to native wide filesystem paths rather than being passed through the
     * process code page.
     */
    static ParseResult parse(int argc, char** argv);

    /**
     * Parse the current process command line without losing Unicode paths.
     *
     * Windows uses the native UTF-16 command line (GetCommandLineW /
     * CommandLineToArgvW) and converts it to UTF-8 before delegating to parse().
     * Other platforms delegate directly to parse(argc, argv).
     */
    static ParseResult parse_process_command_line(int argc, char** argv);

    static std::string usage_text();

    /**
     * Run the read-only index command.
     *
     * @param options Parsed command options.
     * @param config_dir AI File Sorter configuration directory where the personal
     *        index database is stored.
     * @param runtime_dir Shared runtime lock directory.
     * @param out Human or JSON command output.
     * @param err Diagnostics/usage errors.
     */
    static int run(
        const Options& options,
        const std::filesystem::path& config_dir,
        const std::filesystem::path& runtime_dir,
        std::ostream& out,
        std::ostream& err);
};
