# Personal Organizer — Development Log

This is my fork-specific engineering log for the Personal AI File Organizer.

I use it to record significant implementation decisions, commits, CI results, safety boundaries, and upstream-integration notes. Git history remains the canonical record of exact file changes; this file explains why those changes matter.

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

Current work:

```text
Issue #2
Phase 1: build the whole-PC read-only indexing foundation

Draft PR #1
feature/indexer → personal-organizer
```

---

## 2026-09-21 — Fork foundation

### `902312f` — `docs: add personal organizer roadmap`

Established the first roadmap and the core development rule:

```text
index → understand → relate → plan → review → apply → audit/undo
```

I deliberately started with read-only indexing rather than automatic reorganization.

### Branch strategy

I keep `main` close to upstream, use `personal-organizer` as my stable integration branch, and isolate custom work in `feature/*` branches.

---

## 2026-09-21 — Phase 1 persistent index

### `3e49f698` — `feat(indexer): add personal file index interface`

Added `app/include/PersonalFileIndex.hpp` with scan options and summary types.

### `8c00e2bf` — `feat(indexer): implement streaming read-only file index`

Added `app/lib/PersonalFileIndex.cpp`.

Important decisions:

- separate `personal_file_index.db` instead of overloading the categorization cache;
- SQLite WAL mode;
- streaming traversal rather than collecting a whole drive in memory;
- scan-run history;
- persistent path/metadata inventory;
- optional SHA-256 rather than mandatory whole-drive hashing;
- content-analysis columns reserved for later extraction/OCR work;
- reuse upstream `ProtectedProjectDetector`;
- no move/rename/delete/write operations on scanned source files.

### `d2f1d0ac` — `fix(indexer): include size type explicitly`

Made the public header self-contained for its use of `std::size_t`.

---

## 2026-09-21 — Tests and fork CI

### `1f597a7e` — initial personal-index integration test

Added a temporary-filesystem + SQLite integration test covering normal files, a Node.js project, explicit exclusions, hashing, repeated scans, and source-file preservation.

### `78d94aad` — integrate the index test into the suite

Added the test to `tests/run_all_tests.sh`.

### `7795f19a` — run shell tests through Bash

Removed reliance on executable bits for scripts created through GitHub's contents API.

### `3247dfd` — fork-specific GitHub Actions workflow

Added `.github/workflows/personal-organizer-ci.yml` for:

```text
personal-organizer
feature/**
PRs targeting personal-organizer
```

I need this because upstream CI is primarily organized around `main`, while my custom work is intentionally developed outside `main`.

### First CI failure

The first focused run failed during compilation because `Utils.hpp` exposes `curl/system.h`, but the lightweight CI environment did not have the libcurl development headers installed.

I did not treat this as a passing test or hide it as a generic infrastructure problem.

### `4e416695` — `ci(personal-organizer): install curl headers for index tests`

Added the missing libcurl development dependency.

### `093fad51` — `test(indexer): isolate personal index integration dependencies`

The focused test had also been compiling all of production `Utils.cpp`, which pulled networking/GPU/logger code into an index test that did not exercise those systems.

I replaced that dependency with a small test stub for the two path-conversion helpers required by the indexer.

### CI run #26 — PASS

`Personal Organizer CI` passed on commit `093fad51`.

That established a clean baseline before changing stale/presence semantics.

---

## 2026-09-21 — Documentation ownership

I rewrote the fork documentation in my own project voice and made documentation part of the development contract.

Current fork-specific docs:

```text
PERSONAL-ORGANIZER.md
AGENTS.md
docs/PERSONAL-ORGANIZER-ROADMAP.md
docs/PERSONAL-ORGANIZER-ARCHITECTURE.md
docs/PERSONAL-ORGANIZER-CLI.md
docs/PERSONAL-ORGANIZER-DEVELOPMENT-LOG.md
```

`AGENTS.md` tells future coding agents/tools how I want this fork handled: branch discipline, source-file safety, issue/PR linking, testing, documentation updates, shared GUI/CLI core, and no overclaiming of planned functionality.

I am intentionally keeping the large upstream `README.md` mostly upstream-owned to reduce merge conflicts. `PERSONAL-ORGANIZER.md` is the canonical entry point for my fork-specific product direction; if I later add a README notice it should remain a very small pointer rather than a rewrite of upstream documentation.

GitHub Issues are enabled. Issue #2 is the active Phase 1 backlog; PR #1 is the draft implementation review.

---

## 2026-09-21 — Safe presence/stale semantics

### Problem found

The initial indexer marked every previously indexed row under a root as `is_present=0` before starting the new scan. That was unsafe:

```text
root temporarily missing / inaccessible
→ scanner sees nothing
→ old index rows could look deleted
```

Absence can only be inferred from a trustworthy scan.

### `36e93f5a` — `fix(indexer): preserve presence on partial scans`

Changed the model to root-by-root finalization:

```text
scan root
→ refresh every entry actually seen
→ if root scan completed reliably:
     mark unseen old entries not present
→ if root missing/inaccessible/partial:
     preserve old presence for unseen entries
     persist scan status = partial
```

Other changes in the same safety rewrite:

- permission/enumeration errors make that root partial rather than evidence of deletion;
- database failures remain fatal;
- successful roots still correctly stale genuinely deleted entries;
- Windows system locations began moving from generic name matching to path-aware environment/root detection;
- generic exclusions now focus on generated/project internals such as `.git`, `node_modules`, `.next`, virtual environments, and caches.

Windows-specific path logic still needs Windows execution/validation before I call that blocker finished.

### `8ad828a4` — `test(indexer): preserve presence when roots disappear`

Expanded the integration test to prove both sides of the contract:

1. delete one file and successfully rescan → that row becomes stale;
2. remove the whole scan root and rescan → previously present files remain present because the observation was incomplete;
3. the missing-root run is persisted with `status = partial`;
4. source mutation remains absent.

### CI run #29 — PASS

`Personal Organizer CI` passed with the new stale/presence regression test.

This resolves the original false-deletion safety flaw for missing/partial roots on the tested path.

---

# Current Phase 1 safety state

## Resolved and tested

- persistent read-only index foundation;
- streaming traversal;
- focused integration CI;
- successful-rescan stale detection;
- missing-root presence preservation;
- persistent `partial` run status;
- optional hashing;
- project-root recognition;
- source files are not mutated by the index test.

## Implemented but still needs platform validation

### Path-aware Windows/system exclusions

The scanner now identifies major protected Windows locations using actual environment/root paths rather than treating every directory named `Windows`, `Program Files`, etc. as a system directory.

This must still be exercised on Windows before whole-drive scanning is considered safe.

## Still open

### Project indexing vs mutation protection

The current conservative mode stores a strong project root as one protected entry and does not index its useful internal source files.

My long-term rule is:

```text
protected from generic mutation
!=
excluded from read-only indexing/search
```

I still need to separate those concepts explicitly.

### Public personal-index CLI

`PersonalFileIndex` exists as a core service but is not yet exposed as a stable `aifilesorter index ...` command.

### Query/statistics helpers

The index needs a supported service/API for counts, status, search groundwork, and diagnostics rather than requiring direct ad-hoc SQLite queries.

### Small real-folder validation

After the controlled CLI exists and Windows-specific safety is validated, I will test on a small real folder before any broad filesystem scan.

---

# Current implementation order

1. validate Windows-specific path exclusions;
2. separate project read-only indexing depth from mutation protection;
3. expose a controlled read-only `index` CLI command with JSON output;
4. add statistics/query helpers;
5. add CLI parser/output regression tests;
6. run a small real-folder test;
7. only then consider larger roots;
8. connect existing document extraction to the persistent index;
9. benchmark Arabic/Urdu OCR engines on representative scans;
10. continue into policy/taxonomy/relationship planning.

---

# Merge rule for PR #1

I will keep PR #1 draft until the Phase 1 slice has a defensible boundary.

Before merge into `personal-organizer`, I require:

- focused CI green;
- stale/presence safety green;
- Windows exclusion behavior validated or conservatively blocked;
- project indexing semantics explicit;
- controlled read-only CLI entry point;
- documentation matching implementation;
- no source mutation in index flows;
- remaining non-critical work split into clearly scoped follow-up issues where appropriate.
