#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct PersonalFileIndexOptions {
    /** Include hidden files and directories in the index. */
    bool include_hidden{false};

    /**
     * Reserved for later traversal work. Phase 1 rejects true rather than
     * following reparse points/symlinks without cycle detection, directory
     * identity tracking, and root-boundary enforcement.
     */
    bool follow_reparse_points{false};

    /**
     * Detect recognized software/project roots and mark them as protected project
     * entries. This protection is metadata for later organization/planning and
     * does not by itself require hiding project contents from the read-only index.
     */
    bool protect_project_directories{true};

    /**
     * Continue read-only traversal inside recognized protected projects.
     * Enabled by default so repositories remain searchable while generated
     * internals such as .git/node_modules stay excluded by normal scan rules.
     * Disable for the conservative "protected root only" behavior.
     */
    bool index_protected_project_contents{true};

    /**
     * Compute SHA-256 for regular files while scanning.
     * Disabled by default because hashing a whole drive can cause substantial I/O.
     */
    bool compute_sha256{false};

    /** Commit SQLite writes after this many indexed entries. */
    std::size_t commit_batch_size{2000};

    /**
     * Directory names that are never traversed. Matching is case-insensitive on
     * Windows and case-sensitive on platforms whose filesystems are normally so.
     */
    std::vector<std::string> excluded_directory_names;
};

struct PersonalFileIndexScanSummary {
    std::int64_t run_id{-1};
    std::uint64_t files_indexed{0};
    std::uint64_t directories_indexed{0};
    std::uint64_t protected_projects_indexed{0};
    std::uint64_t entries_skipped{0};
    std::uint64_t errors{0};
    bool completed{false};
    std::vector<std::string> warnings;
};

/**
 * @brief Persistent, read-only inventory of files outside the application's
 *        categorization cache.
 *
 * "Read-only" refers to the scanned filesystem: scan() never moves, renames,
 * deletes, or writes to source files. It writes only to its own SQLite index in
 * the AI File Sorter configuration directory.
 */
class PersonalFileIndex {
public:
    explicit PersonalFileIndex(std::string config_dir);
    ~PersonalFileIndex();

    PersonalFileIndex(const PersonalFileIndex&) = delete;
    PersonalFileIndex& operator=(const PersonalFileIndex&) = delete;

    bool is_open() const noexcept;
    const std::string& database_path() const noexcept;

    /**
     * @brief Index one or more roots using streaming traversal.
     * @param roots Files/directories to inventory. Duplicate/overlapping roots are
     *        rejected in Phase 1 so one entry cannot silently change root ownership.
     * @param options Scan policy. Empty exclusion list uses safe defaults.
     * @return Scan counts, warnings, and persistent run identifier.
     */
    PersonalFileIndexScanSummary scan(
        const std::vector<std::string>& roots,
        const PersonalFileIndexOptions& options = {});

    /** Safe default directory exclusions for whole-PC scanning. */
    static std::vector<std::string> default_excluded_directory_names();

private:
    struct Impl;
    Impl* impl_{nullptr};
    std::string database_path_;
};
