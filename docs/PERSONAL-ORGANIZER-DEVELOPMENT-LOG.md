# Personal Organizer — Development Log

This is my fork-specific engineering log for the Personal AI File Organizer.

I use it to record significant implementation decisions, safety corrections, CI results, architecture pivots, and upstream-integration notes. Git history remains the canonical record of exact file changes; this file explains why those changes matter.

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

Current implementation work:

```text
Issue #2
Phase 1: trustworthy persistent filesystem state/index foundation

Draft PR #1
feature/indexer → personal-organizer
```

Architecture epics now include:

```text
#3 CLI-first / offline-first / multi-terabyte architecture
#4 EverythingProvider
#5 dependency/delegation strategy
#6 durable daemon/job/checkpoint/resource runtime
#7 intent/questions/policy/explainability
#8 8TB snapshots/reconciliation/benchmarks/fault injection
```

---

## 2026-09-21 — Fork foundation

### `902312f` — `docs: add personal organizer roadmap`

Established the initial roadmap and the core safety direction:

```text
index → understand → relate → plan → review → apply → audit/undo
```

I deliberately started with read-only indexing rather than automatic reorganization.

### Branch strategy

`main` stays close to upstream. `personal-organizer` is the stable custom integration branch. Experimental/custom work lives in `feature/*` and is reviewed before merge.

---

## 2026-09-21 — Phase 1 persistent index foundation

### `3e49f698` — personal file index interface

Added the public index options/summary types.

### `8c00e2bf` — streaming read-only file index

Introduced a dedicated organizer-owned SQLite database instead of overloading upstream categorization cache state.

Important decisions:

- SQLite/WAL control plane;
- streaming traversal rather than collecting a whole drive in RAM;
- scan-run history;
- persistent path/metadata inventory;
- optional hashing rather than mandatory whole-drive reads;
- protected-project awareness;
- source-filesystem read-only boundary;
- content-analysis fields reserved for later extraction/OCR work.

---

## 2026-09-21 — Focused tests and fork CI

The fork gained a dedicated `Personal Organizer CI` workflow for `personal-organizer`, `feature/**`, and PRs targeting the integration branch.

Early focused tests use temporary filesystems + SQLite and deliberately avoid the developer's real data.

### First CI failure

The first focused build failed because the test environment lacked libcurl development headers indirectly required by `Utils.hpp`.

This was treated as a real failure, not relabelled as success.

The focused index test was also simplified so it did not compile unrelated GPU/network/logger implementation merely to test UTF-8 path conversion.

### CI run #26 — PASS

Established the first clean focused index baseline.

---

## 2026-09-21 — Safe presence semantics

### Problem

The first model could mark old rows absent before proving that a root had been completely observed.

Unsafe interpretation:

```text
root missing/inaccessible
→ nothing seen
→ old data appears deleted
```

### Fix

Presence finalization became root-scoped:

```text
observe root
→ refresh entries actually seen
→ only after a trustworthy complete root observation:
     mark unseen previous entries missing
→ on missing/inaccessible/partial root:
     preserve prior physical state
     record partial observation
```

### CI run #29 — PASS

Regression tests proved both genuine deletion after a successful complete rescan and preservation after an unavailable-root scan.

---

## 2026-09-21 — Project visibility vs mutation protection

The original conservative project handling treated a recognized project root as effectively opaque.

The model was corrected to distinguish:

```text
protected from generic organization mutation
!=
hidden from read-only search/indexing
```

Useful source files inside projects remain indexable while generated internals such as `.git` and `node_modules` remain excluded.

### CI run #38 — PASS

Regression coverage confirmed protected project roots remain marked while useful source content remains visible.

---

## 2026-09-21 — Read-only statistics/query facade

Added a separate `PersonalFileIndexQuery` layer so the CLI/GUI do not need to issue arbitrary SQL against the control database.

It opens SQLite read-only and exposes aggregate statistics/latest-run status.

This intentionally separates:

```text
PersonalFileIndex       writes/updates physical index state
PersonalFileIndexQuery  reads/reporting/query facade
```

---

## 2026-09-21 — Personal index command service

Added `PersonalIndexCommand` as a fork-specific CLI service rather than overloading upstream headless categorization logic.

The service gained:

- argument parsing;
- structured JSON output;
- hashing controls;
- project controls;
- shared runtime lock participation;
- actual temporary-folder scan tests;
- machine-contract tests.

The index path is deliberately independent of LLM/model readiness.

---

## 2026-09-21 — Real executable/Windows launcher integration

The command was routed through the real application executable before GUI startup/LLM selection.

A Windows packaging detail was discovered: the launcher prepends global flags such as `--allow-direct-launch`, so the index parser needed to accept the installed launcher argv shape without consuming flags that belong to the main process.

The Windows launcher was also made to treat `index` as a synchronous console/headless-style invocation so PowerShell can receive output.

Focused Linux/Windows runs subsequently passed, including Windows system-root and Unicode path tests.

---

## 2026-09-21 — Phase 1 safety review corrections

A deeper review identified several issues that focused happy-path tests did not fully cover. Phase 2 work was frozen while the Phase 1 defects were corrected.

### Windows Unicode command-line boundary

The safe wide-character parser existed, but production wiring still used the narrow argv path in places.

Fix direction:

- read Windows process arguments through the native UTF-16 command line for the fork `index` command;
- preserve Unicode roots end-to-end through the packaged launcher;
- fix unsafe path-conversion buffer handling found during review.

### Reparse traversal

`--follow-reparse-points` was judged unsafe before bounded traversal/cycle/root-boundary semantics exist.

Current Phase 1 rule:

```text
reparse/symlink following = fail closed
```

The option is not allowed to silently enable unbounded traversal.

### Physical state vs policy state

A single presence flag was insufficient because an item can be physically present while excluded by the current scan policy.

The schema now distinguishes concepts such as:

```text
observation_state
  present / missing / unknown

policy_state
  included / hidden / excluded / system / reparse-skipped / protected / ...
```

A policy/settings change must not be interpreted as deletion.

### Overlapping roots

Duplicate or ancestor/descendant roots can create ambiguous root ownership and stale-state behavior.

Phase 1 rejects overlapping roots instead of pretending many-to-many root ownership is solved.

### Schema migration

The control database now has schema-version/migration work and pre-migration backup behavior.

The first migration regression test caught an ordering defect in the migration itself. The migration order was corrected rather than weakening the test.

### CI run #104 — PASS

The expanded Linux and Windows focused suites both passed after these corrections.

Coverage includes:

- missing-root behavior;
- genuine deletion after complete observation;
- hidden/exclusion policy changes;
- overlapping roots;
- reparse fail-closed behavior;
- Windows `%WINDIR%` refusal;
- ordinary user directory named `Windows` remains indexable;
- Arabic/Unicode filesystem paths;
- query facade;
- CLI command service;
- legacy schema migration and backup.

This is strong focused coverage, but it is **not** yet the final production Windows gate.

---

## Remaining Phase 1 production gate

PR #1 remains draft.

Before Phase 1 is merge-ready / suitable for a broad real filesystem scan, require:

1. native MSVC/vcpkg production Windows build in CI;
2. invoke the actual packaged `aifilesorter.exe index ...` path;
3. use an Arabic/Urdu/non-Latin root through that launcher;
4. verify resulting SQLite state;
5. hash a controlled copied source fixture before/after to prove no mutation;
6. run a small copied real Windows fixture owned by me;
7. synchronize PR/issue/docs with the observed production behavior.

Do **not** call Phase 1 checkpoint-resumable. It currently supports persistent incremental rescanning; true in-job pause/resume belongs to the durable job engine.

Do **not** recommend an unattended whole `C:\` scan before the production gate above.

---

## 2026-09-21 — Product direction pivot: CLI first / offline first

The project direction was deliberately strengthened from “GUI-first organizer with a power CLI” to:

> a serious CLI-first, offline-first filesystem intelligence and organization system for multi-million-file / multi-terabyte storage.

Important architecture documentation commits include:

- `bf77f136` — make personal organizer CLI-first and multi-terabyte;
- `9a9d0ac6` — redesign architecture around CLI providers and durable jobs;
- `df8fc5d2` — revise roadmap for CLI-first multi-terabyte system;
- `5a444f0b` — align agent rules with CLI-first architecture.

The GUI remains valuable, but is now explicitly a secondary client of shared services.

---

## 2026-09-21 — Delegate mature primitives

Created/expanded the dependency strategy so the project does not waste time rebuilding solved infrastructure.

Strong directions include:

```text
Everything SDK/IPC    Windows filesystem discovery/change feed
ExifTool              broad metadata
MediaInfo             media metadata
libarchive            archive inventory
OCRmyPDF              PDF OCR orchestration
PaddleOCR             Arabic/Urdu/English OCR benchmark candidate
Tantivy/FTS5          full-text search candidates
USearch               vector search candidate
BLAKE3                fast fingerprints
CLI11                 CLI parser candidate
```

Additional candidates now tracked include deterministic file typing (`libmagic`), Tree-sitter/Git adapters for software-project understanding, ICU for multilingual normalization/tokenization where justified, zstd for derived-cache compression, OS credential/keychain storage for API secrets, and ONNX Runtime as a benchmark candidate for compact local models.

External tools remain workers/providers. The organizer owns intent, canonical state, durable jobs, policy, privacy, provenance, plans, apply safety, and audit.

Tracking: Issue #5.

---

## 2026-09-21 — “Amazing systems tool” quality bar

### `3984f7ca` — `docs: define system quality bar for serious CLI organizer`

Added `docs/PERSONAL-ORGANIZER-SYSTEM-QUALITY-BAR.md`.

This defines what production quality means beyond feature checkboxes.

Major additions:

- fast discovery is separated from deep semantic enrichment;
- progressive intelligence ladder instead of mandatory analyze-everything passes;
- physical truth, policy truth, deterministic derived data, inferred knowledge, user intent, plan state, and audit state remain separate;
- observation epochs/snapshots/reconciliation;
- stable file/volume identity and hardlink awareness;
- avoid accidental cloud-placeholder hydration;
- durable daemon/job runtime direction;
- bounded queues/backpressure;
- per-device resource scheduling;
- preflight estimates and hard resource/cost budgets;
- typed JobSpec and question engine;
- policy lint/test/diff/explain;
- evidence-backed relationship graph and content lineage;
- field-level provenance;
- reproducible plans;
- virtual-filesystem simulation before apply;
- honest transactional/recovery semantics for filesystem mutation;
- scope-aware privacy;
- optional-component health reporting;
- profiles/recipes with inspectable resolved settings;
- structured observability;
- 1M/10M synthetic benchmarks and fault-injection quality gates.

### New architecture issues

- #6 — durable daemon, job engine, checkpoints, scheduler;
- #7 — intent compiler, questions, policy simulation, explainability;
- #8 — 8TB snapshots, reconciliation, benchmarks, fault injection.

### `31a53f1f` — expand fork product overview

Updated `PERSONAL-ORGANIZER.md` so the public fork description reflects progressive intelligence, durable runtime direction, budgets, snapshots, filesystem edge cases, provenance, plan simulation, and the new issues.

### `6b585216` — align CLI contract with canonical architecture

Corrected the CLI document so it no longer describes the GUI as the primary interface and no longer advertises reparse following as enabled.

Added planned command families for durable jobs/daemon, snapshots/diffs/reconciliation, estimates, model/component control, request explanation, and richer policy/search/relationship workflows.

### Issue #3 expansion

The main architecture epic now explicitly tracks the new quality bar, progressive-intelligence rule, durable runtime, user-control standard, apply simulation, provider reconciliation, and scale/fault acceptance gates.

---

## Current architectural sequence

The current high-level build order is:

```text
Phase 1
trustworthy physical state + production CLI validation

↓

filesystem provider abstraction + Everything acceleration

↓

durable jobs/checkpoints/daemon + resource scheduler

↓

intent compiler + persisted question engine + policy AST

↓

content identity/cache + worker/plugin protocol

↓

deterministic extraction + multilingual OCR

↓

full-text search + semantic search

↓

model router / offline-first intelligence

↓

relationships + lineage + bibliography + duplicates

↓

planner + simulation + review

↓

validated apply + audit/undo

↓

continuous/watch operation + GUI/agent clients
```

The key optimization is that these stages are progressive: a physical inventory does not require deep understanding of every file before the system becomes useful.

---

## Documentation ownership rule

Fork-specific documentation is part of implementation, not optional cleanup.

Current canonical docs:

```text
PERSONAL-ORGANIZER.md
AGENTS.md
docs/PERSONAL-ORGANIZER-ROADMAP.md
docs/PERSONAL-ORGANIZER-ARCHITECTURE.md
docs/PERSONAL-ORGANIZER-SYSTEM-QUALITY-BAR.md
docs/PERSONAL-ORGANIZER-DEPENDENCY-STRATEGY.md
docs/PERSONAL-ORGANIZER-CLI.md
docs/PERSONAL-ORGANIZER-DEVELOPMENT-LOG.md
```

Rules:

- planned behavior is labelled planned;
- tests are never claimed green unless they actually ran green;
- architecture changes update docs/issues in the same development cycle;
- Git history remains the exact commit record;
- this log records why important changes matter.

---

## Merge rule for PR #1

PR #1 remains draft until the Phase 1 slice has a defensible production boundary.

Before merge into `personal-organizer`, require:

- focused Linux/Windows CI green;
- physical-vs-policy state semantics green;
- migration/backup regression green;
- Windows Unicode/system-root behavior green;
- project indexing semantics explicit;
- controlled read-only CLI entry point;
- real native Windows build + packaged-launcher smoke test;
- copied-fixture before/after no-mutation proof;
- documentation matching implementation;
- remaining non-critical work split into scoped follow-up issues.
