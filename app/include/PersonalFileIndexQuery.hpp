#pragma once

#include <cstdint>
#include <optional>
#include <string>

/**
 * @brief Stable aggregate statistics for the personal file index.
 *
 * Physical observation and indexing policy are separate. A policy-skipped entry
 * can be physically present or unknown; it is not treated as missing merely
 * because the current scan chose not to traverse it.
 */
struct PersonalFileIndexStats {
    std::uint64_t total_entries{0};
    std::uint64_t present_entries{0};
    std::uint64_t present_files{0};
    std::uint64_t present_directories{0};
    std::uint64_t present_protected_projects{0};
    std::uint64_t missing_entries{0};
    // Backward-compatible name used by the early CLI JSON. It is exactly the
    // count of observation_state='missing', not policy-skipped/unknown entries.
    std::uint64_t stale_entries{0};
    std::uint64_t unknown_entries{0};
    std::uint64_t policy_skipped_entries{0};
    std::uint64_t hashed_files{0};
    std::uint64_t present_bytes{0};

    std::int64_t latest_run_id{-1};
    std::string latest_run_status;
    std::uint64_t latest_run_errors{0};
};

/**
 * @brief Read-only query facade for `personal_file_index.db`.
 *
 * This class deliberately owns no scan or filesystem-mutation behavior. It exists
 * so GUI/CLI/agent surfaces can consume index state without embedding raw SQLite
 * queries or coupling themselves to PersonalFileIndex traversal internals.
 */
class PersonalFileIndexQuery {
public:
    explicit PersonalFileIndexQuery(std::string database_path);
    ~PersonalFileIndexQuery();

    PersonalFileIndexQuery(const PersonalFileIndexQuery&) = delete;
    PersonalFileIndexQuery& operator=(const PersonalFileIndexQuery&) = delete;

    bool is_open() const noexcept;
    const std::string& database_path() const noexcept;

    /**
     * @return Current aggregate statistics, or std::nullopt when the database is
     *         unavailable or does not contain the expected personal-index schema.
     */
    std::optional<PersonalFileIndexStats> stats() const;

private:
    struct Impl;
    Impl* impl_{nullptr};
    std::string database_path_;
};
