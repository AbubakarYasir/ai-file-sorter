#pragma once

#include <cstdint>
#include <optional>
#include <string>

/**
 * @brief Stable aggregate statistics for the personal file index.
 *
 * Counts describe persisted index state. `present_*` fields only count entries
 * whose latest trustworthy root scan still considers them present. Stale entries
 * remain in the database for history/audit purposes.
 */
struct PersonalFileIndexStats {
    std::uint64_t total_entries{0};
    std::uint64_t present_entries{0};
    std::uint64_t present_files{0};
    std::uint64_t present_directories{0};
    std::uint64_t present_protected_projects{0};
    std::uint64_t stale_entries{0};
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
