# Personal Organizer Development Log

This is the fork-specific engineering log for the Personal AI File Organizer work.

It records important commits, architectural decisions, verification state, known limitations, and upstream-integration notes. It is not a replacement for Git history; it explains why the changes exist.

## Repository model

```text
upstream = hyperfield/ai-file-sorter
origin   = AbubakarYasir/ai-file-sorter

main
  upstream-compatible base

personal-organizer
  stable custom integration branch

feature/*
  isolated work reviewed before integration
```

Current feature branch:

```text
feature/indexer
```

Current review:

```text
Draft PR #1: Phase 1: add read-only personal file index foundation
feature/indexer → personal-organizer
```

---

## 2026-09-21 — Fork foundation

### `902312f` — `docs: add personal organizer roadmap`

Added the initial long-term roadmap under:

```text
docs/PERSONAL-ORGANIZER-ROADMAP.md
```

The roadmap establishes the analyze-first sequence:

```text
index → understand → plan → review → apply → audit/undo
```

and keeps automatic filesystem mutation out of the first milestone.

### Branch strategy

Created `personal-organizer` as the stable custom integration branch while leaving `main` suitable for upstream synchronization.

Feature work is expected to happen on `feature/*` branches and merge into `personal-organizer` only after review.

---

## 2026-09-21 — Phase 1 indexer foundation

### `3e49f698` — `feat(indexer): add personal file index interface`

Added:

```text
app/include/PersonalFileIndex.hpp
```

The interface defines:

- read-only scan options;
- scan summary/result counts;
- multiple scan roots;
- hidden-file control;
- reparse/symlink control;
- project protection;
- optional SHA-256 hashing;
- SQLite batching;
- configurable directory exclusions.

### `8c00e2bf` — `feat(indexer): implement streaming read-only file index`

Added:

```text
app/lib/PersonalFileIndex.cpp
```

Key choices:

- separate database: `personal_file_index.db`;
- SQLite WAL mode;
- streaming traversal rather than collecting the entire filesystem in memory;
- persistent scan-run table;
- persistent entry inventory;
- metadata fields for paths, extensions, size, timestamps, hash state, project detection, and presence;
- reserved content fields for later text/OCR/image enrichment;
- reuse of upstream `ProtectedProjectDetector`;
- optional SHA-256 hashing, off by default;
- no source-file move/rename/delete/write operations.

### `d2f1d0ac` — `fix(indexer): include size type explicitly`

Added `<cstddef>` to make the public interface's `std::size_t` dependency explicit rather than relying on transitive includes.

### Draft PR #1

Opened a draft pull request from:

```text
feature/indexer
```

to:

```text
personal-organizer
```

The PR remains draft until Phase 1 correctness, tests, and a controlled application entry point are complete.

---

## 2026-09-21 — Test and CI foundation

### `721daf49` — `chore: track personal organizer documentation`

Changed the upstream-style `/docs/**` ignore rule narrowly so files matching:

```text
docs/PERSONAL-ORGANIZER-*.md
```

are tracked without making all ignored upstream/R&D documentation part of the fork.

### `1f597a7e` — `test: add personal file index integration coverage`

Added:

```text
tests/run_personal_index_tests.sh
```

The integration test creates an isolated temporary filesystem and verifies:

- regular files are indexed;
- a Node.js project is detected as a protected project;
- explicit exclusions are respected;
- SHA-256 can be persisted;
- source files remain intact;
- a second scan records stale/deleted state;
- scan-run history is persisted.

### `78d94aad` — `test: include personal index integration suite`

Added the personal-index integration script to `tests/run_all_tests.sh`.

### `7795f19a` — `test: run integration scripts through bash`

Changed the test runner to invoke shell integration tests through `bash` instead of relying on Unix executable bits. This is necessary because GitHub's contents API creates new text files as normal non-executable files.

### `3247dfd` — `ci: add personal organizer integration checks`

Added:

```text
.github/workflows/personal-organizer-ci.yml
```

The workflow is scoped to:

```text
personal-organizer
feature/**
PRs targeting personal-organizer
```

It installs only the dependencies needed for the current personal-index integration check and runs the new test.

#### CI status

No workflow run was observed immediately after adding the workflow. This is an infrastructure state, not yet a passing/failing test result. The fork may require GitHub Actions to be enabled/approved before custom workflows execute.

Do not record the Phase 1 integration suite as passing until an actual run or local build/test confirms it.

---

## 2026-09-21 — Documentation architecture

### `d857de3c` — `docs: add personal organizer project index`

Added root landing page:

```text
PERSONAL-ORGANIZER.md
```

It provides the fork overview without heavily rewriting upstream `README.md`, reducing future merge conflicts.

### `f5c42993` — `docs: define personal organizer architecture`

Added:

```text
docs/PERSONAL-ORGANIZER-ARCHITECTURE.md
```

Important architectural rule:

> GUI, CLI, and future Agent/MCP interfaces call the same underlying organizer services.

The GUI is the primary everyday interface; the CLI is the power-user superset.

### `95487330` — `docs: define personal organizer CLI contract`

Added:

```text
docs/PERSONAL-ORGANIZER-CLI.md
```

The document distinguishes implemented engine capability from planned public commands. Planned CLI groups include indexing, search, inspect, plan/apply, OCR, duplicate analysis, taxonomy/policy tools, export, database maintenance, and diagnostics.

---

# Current safety review

The current indexer is an engineering draft. It must **not** yet be used for an unattended whole-`C:\` scan.

## Blocker 1 — stale state on inaccessible roots

Current implementation marks previous rows for a scan root as not present before completing the new traversal.

Risk:

- if a root is disconnected, temporarily inaccessible, or fails early, previous entries could incorrectly appear absent.

Required correction:

- record seen rows using `last_seen_run_id`;
- finalize `is_present=0` only after the relevant root scan is trustworthy;
- preserve previous presence state when the root cannot be scanned reliably.

## Blocker 2 — protected project semantics

Current draft treats a strong project root as one protected entry and skips its contents.

This is safe for reorganization but too conservative for a long-term searchable PC index.

Desired separation:

```text
project is protected from mutation
!=
project is invisible to read-only indexing
```

A later Phase 1 revision should be able to index useful source/docs inside a project while excluding `.git`, `node_modules`, build outputs, caches, virtual environments, and similar generated content. Descendants should retain project-root/type metadata so the planner knows not to pull files out of the repository.

## Blocker 3 — system exclusion semantics

The initial exclusion mechanism is based primarily on directory names.

For a real Windows whole-drive scan, protected system locations should become path-aware. A user folder merely named `Windows` should not be treated the same as `C:\Windows`.

## Blocker 4 — public CLI not wired yet

`PersonalFileIndex` exists as a shared service, but the public application entry point is not yet wired into `main.cpp`/the argument parser.

The documentation deliberately marks `aifilesorter index ...` as planned until that wiring and CLI regression coverage are complete.

---

# Infrastructure notes

## GitHub Issues

Attempting to create the Phase 1 engineering backlog through GitHub Issues returned:

```text
Issues has been disabled in this repository.
```

Until Issues are enabled, active engineering checklists belong in:

- Draft PR #1;
- this development log;
- the roadmap.

## GitHub Actions

The upstream `Build` workflow only targets `main`. A fork-specific `Personal Organizer CI` workflow was therefore added for `personal-organizer` and `feature/**` development.

No CI success should be claimed until an actual workflow run is visible.

---

# Next implementation sequence

1. Fix stale/presence semantics for failed or partial scans.
2. Separate protected-project mutation safety from read-only indexing depth.
3. Introduce path-aware exclusions for Windows system roots.
4. Get the integration test executing in CI or run the same test locally.
5. Wire the index service into a controlled CLI entry point with stable JSON output.
6. Add query/statistics helpers for the GUI and CLI.
7. Only then expose the indexer in the Qt GUI.
8. After metadata indexing is reliable, begin content extraction and Arabic/Urdu OCR work.

---

# Merge rule for PR #1

Do not merge `feature/indexer` into `personal-organizer` until:

- the integration test has actually passed;
- stale-state safety is fixed;
- system exclusion semantics are safe enough for selected real roots;
- the CLI entry point is controlled and read-only;
- documentation matches the implemented behavior;
- no known operation can move, rename, delete, or modify scanned source files during indexing.
