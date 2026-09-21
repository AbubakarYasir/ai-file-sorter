# Personal Organizer — Development Log

This is my fork-specific engineering log for the Personal AI File Organizer.

I use this file to record the reasoning behind significant implementation decisions, test/CI state, safety blockers, and upstream-integration notes. Git history remains the canonical record of exact file changes; this log explains what those changes mean for the project.

## Repository model

```text
upstream = hyperfield/ai-file-sorter
origin   = AbubakarYasir/ai-file-sorter

main
  upstream-friendly base

personal-organizer
  stable custom integration branch

feature/*
  isolated work reviewed before integration
```

Current feature branch:

```text
feature/indexer
```

Current tracking issue:

```text
#2 — Phase 1: Whole-PC read-only indexing foundation
```

Current review:

```text
Draft PR #1
feature/indexer → personal-organizer
```

---

## 2026-09-21 — Fork foundation

### `902312f` — `docs: add personal organizer roadmap`

I added the first roadmap under:

```text
docs/PERSONAL-ORGANIZER-ROADMAP.md
```

The core development philosophy was established immediately:

```text
index → understand → plan → review → apply → audit/undo
```

I deliberately made the first milestone read-only instead of starting with automatic sorting.

### Branch strategy

I created `personal-organizer` as the stable custom integration branch and kept `main` suitable for upstream synchronization.

Feature work goes into `feature/*` branches and is reviewed before integration.

---

## 2026-09-21 — Phase 1 indexer foundation

### `3e49f698` — `feat(indexer): add personal file index interface`

Added:

```text
app/include/PersonalFileIndex.hpp
```

The first interface introduced:

- read-only scan options;
- scan summary counts;
- multiple scan roots;
- hidden-entry control;
- reparse/symlink behavior;
- protected-project behavior;
- optional SHA-256 hashing;
- SQLite batching;
- configurable exclusions.

### `8c00e2bf` — `feat(indexer): implement streaming read-only file index`

Added:

```text
app/lib/PersonalFileIndex.cpp
```

Important implementation choices:

- dedicated database: `personal_file_index.db`;
- separate from the existing categorization cache;
- SQLite WAL mode;
- streaming traversal rather than accumulating the entire filesystem in memory;
- persistent scan-run history;
- persistent entry inventory;
- metadata for paths, extension, size, timestamps, hash state, project detection, and presence;
- reserved columns for later text/OCR/image enrichment;
- reuse of upstream `ProtectedProjectDetector`;
- optional hashing, disabled by default;
- no move/rename/delete/write operations on scanned source files.

### `d2f1d0ac` — `fix(indexer): include size type explicitly`

Added `<cstddef>` so the public header explicitly owns its use of `std::size_t` instead of relying on a transitive include.

### Draft PR #1

I opened PR #1 from `feature/indexer` into `personal-organizer` and kept it as a draft.

The PR is not merge-ready merely because the indexer compiles conceptually; Phase 1 also requires safety fixes, a controlled entry point, tests, and accurate documentation.

---

## 2026-09-21 — Test and CI foundation

### `721daf49` — documentation tracking rule

I changed the upstream-style `/docs/**` ignore behavior narrowly so my fork documentation is tracked through:

```text
docs/PERSONAL-ORGANIZER-*.md
```

I intentionally did not unignore the entire docs/R&D surface because that would create unnecessary upstream noise.

### `1f597a7e` — personal-index integration test

Added:

```text
tests/run_personal_index_tests.sh
```

The integration test uses an isolated temporary filesystem and is intended to verify:

- regular files are indexed;
- a Node.js project is recognized;
- explicit exclusions are respected;
- SHA-256 persistence works when requested;
- source files remain intact;
- repeated scans update presence/stale state;
- scan-run history is persisted.

### `78d94aad` — integrate the new test into the suite

Added the personal-index integration test to `tests/run_all_tests.sh`.

### `7795f19a` — invoke shell tests through Bash

The test runner now invokes scripts through `bash` instead of assuming executable bits. This makes tests created through GitHub's contents API work predictably.

### `3247dfd` — fork-specific GitHub Actions workflow

Added:

```text
.github/workflows/personal-organizer-ci.yml
```

The workflow targets:

```text
personal-organizer
feature/**
PRs targeting personal-organizer
```

This exists because upstream's main build workflow is primarily oriented around `main`, while I need feedback on my custom integration branch and feature branches.

### CI result: failing, not hidden

GitHub Actions is now executing the fork workflow.

Observed run:

```text
Workflow: Personal Organizer CI
Run:      #10
Branch:   feature/indexer
Result:   FAILURE
```

Successful steps:

```text
checkout
install test dependencies
```

Failed step:

```text
Run personal file index integration test
```

The documentation verification step was skipped because the preceding test failed.

This is a real engineering failure, not an infrastructure/Actions-enable problem. The next step is to inspect the integration-test logs, fix the underlying test/code issue, and rerun CI. I will not mark Phase 1 tests as passing until GitHub or an equivalent local run actually passes.

---

## 2026-09-21 — Documentation system

I decided not to heavily rewrite upstream `README.md`, because doing so would make every upstream sync unnecessarily conflict-prone.

Instead, fork-specific documentation is namespaced and linked from a dedicated root entry point.

### Personal project entry point

```text
PERSONAL-ORGANIZER.md
```

This explains the project in my voice: why I am building it, what problem it solves for my mixed filesystem, the GUI/CLI relationship, safety model, branch strategy, current status, and documentation map.

### Architecture

```text
docs/PERSONAL-ORGANIZER-ARCHITECTURE.md
```

This documents the shared-core model:

```text
core services
├── GUI
├── CLI
└── future Agent/MCP
```

The GUI is my everyday interface. The CLI is the power-user superset. Neither gets a separate implementation of organizer logic.

### CLI contract

```text
docs/PERSONAL-ORGANIZER-CLI.md
```

The CLI documentation now explicitly labels commands as:

```text
implemented upstream
implemented in fork
in progress
planned
```

This prevents design examples such as `aifilesorter index ...` from being mistaken for commands that already exist.

### Roadmap rewrite

The roadmap now uses my project perspective instead of referring to me as “the user.” It also separates the phases more clearly and records the intended architecture for indexing, extraction, OCR, policy, relationships, duplicates, bibliography, planning, GUI, CLI, and agent integration.

### Documentation ownership rule

From this point forward I treat documentation as part of implementation.

A meaningful feature change should update the relevant combination of:

```text
Issue
PR description
PERSONAL-ORGANIZER.md
roadmap
architecture
CLI contract
development log
AGENTS.md
```

Planned behavior must remain explicitly labelled as planned.

---

## GitHub Issues enabled

Issues were initially disabled on the fork. After enabling them, I created:

```text
#2 — Phase 1: Whole-PC read-only indexing foundation
```

Issues are now the actionable backlog. The roadmap remains the long-term direction; issues represent work that can actually be implemented/reviewed.

Future commits and PRs should reference their relevant issue where practical.

---

# Current safety review

The current indexer is still an engineering draft. I should **not** run it unattended against all of `C:\` yet.

## Blocker 1 — stale-state semantics on inaccessible roots

Current implementation marks previous rows for a scan root as not present before completing the new traversal.

Risk:

- if a root is disconnected, unreadable, or fails very early, existing indexed entries may be incorrectly marked absent.

Required direction:

- identify entries through `last_seen_run_id`;
- only finalize stale/presence state when the root scan is trustworthy;
- preserve previous presence when the root itself could not be scanned reliably;
- distinguish complete and partial root scans.

## Blocker 2 — protected-project semantics

The first conservative implementation treats a recognized strong project root as one protected entry and does not traverse its contents.

That protects projects from generic sorting, but it is too restrictive for a long-term searchable index.

The intended distinction is:

```text
protected from generic mutation
!=
excluded from read-only indexing
```

Long term I want useful project contents indexed while generated/internal directories such as `.git`, `node_modules`, build outputs, caches, and virtual environments are excluded appropriately. Descendants should retain project metadata so later planners know they belong to a protected repository/project.

## Blocker 3 — Windows system exclusions are name-based

The first pass mainly excludes directory names.

A real whole-drive scanner needs path-aware rules. A user-owned folder called `Windows` is not equivalent to the operating-system directory `C:\Windows`.

Whole-drive use remains blocked until this is corrected and tested.

## Blocker 4 — public CLI entry point not wired

`PersonalFileIndex` exists as a core service, but the application does not yet expose the personal indexer as a stable command.

The next CLI deliverable should be a small, read-only command that accepts explicit roots and returns structured status.

## Blocker 5 — current integration test failure

CI run #10 failed in `Run personal file index integration test`.

Required action:

- inspect exact compiler/runtime assertion output;
- fix the test or code based on evidence;
- rerun CI;
- record the resolution here.

---

# Current implementation sequence

1. diagnose and fix the failing Phase 1 integration test;
2. fix stale/presence semantics for failed/partial scans;
3. make Windows system exclusions path-aware;
4. separate project indexing depth from mutation protection;
5. expose a controlled read-only `index` CLI command with JSON output;
6. add statistics/query helpers;
7. add parser/CLI regression tests;
8. test on a small real folder;
9. only then consider larger real roots;
10. connect existing document extraction to the persistent index;
11. benchmark and implement Arabic/Urdu OCR.

---

# Merge rule for PR #1

I will not merge `feature/indexer` into `personal-organizer` until:

- the integration test actually passes;
- stale-state safety is corrected;
- real-root exclusion semantics are safe enough for controlled use;
- protected-project behavior is explicitly defined;
- the CLI entry point is controlled and read-only;
- documentation matches implementation;
- indexing cannot move, rename, delete, or edit scanned source files;
- known Phase 1 blockers are either resolved or explicitly split into follow-up issues with a safe current boundary.
