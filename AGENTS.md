# AGENTS.md — Personal AI File Organizer

This file contains the working rules for AI coding agents, automated contributors, and developer tools operating on my fork of `hyperfield/ai-file-sorter`.

The repository owner is building a **CLI-first, offline-first filesystem intelligence and organization system** for multi-million-file / multi-terabyte storage. The GUI is secondary. The CLI is the canonical human and automation contract.

Read this file before changing fork-specific code.

## Repository identity

```text
upstream: hyperfield/ai-file-sorter
fork:     AbubakarYasir/ai-file-sorter
```

Branch roles:

```text
main
  Keep close to upstream.

personal-organizer
  Stable integration branch for custom organizer work.

feature/*
  Isolated feature work. Prefer a PR into personal-organizer.
```

Do not commit experimental organizer work directly to `main` unless the task is explicitly upstream/main maintenance.

## Required reading

Before a significant change, inspect the relevant current versions of:

```text
PERSONAL-ORGANIZER.md
docs/PERSONAL-ORGANIZER-ROADMAP.md
docs/PERSONAL-ORGANIZER-ARCHITECTURE.md
docs/PERSONAL-ORGANIZER-DEPENDENCY-STRATEGY.md
docs/PERSONAL-ORGANIZER-CLI.md
docs/PERSONAL-ORGANIZER-DEVELOPMENT-LOG.md
```

Also inspect upstream code/documentation before inventing a parallel implementation.

## Product rules

### CLI-first, shared core

The CLI is the canonical product surface. The GUI is a secondary visual client. Future Agent/MCP interfaces call the same core services.

Do not hide important functionality exclusively in GUI code. If an important capability cannot be invoked safely from the CLI/core contract, it is not considered complete for this fork.

### Offline first

Baseline indexing, extraction, OCR/search, planning primitives, and model workflows should work locally when technically practical. Cloud providers are optional and governed by explicit privacy/routing policy.

Do not create an architecture where the tool becomes unusable merely because an API key, network connection, or cloud quota is unavailable.

### Scale is a requirement

Design for:

```text
8TB+ storage
millions/tens of millions of entries
hours-to-days jobs
mixed HDD/SSD/NVMe
multiple volumes
```

Do not introduce algorithms that require all paths, extracted text, OCR, or embeddings in memory at once.

Use streaming/paging, bounded queues, durable tasks, incremental caches, and per-device resource controls.

### Long work must be durable

A future long job must survive process crash/reboot and support true checkpoint-based pause/resume. Do not misuse the word `resumable` for a process that merely starts another scan and reuses some existing rows.

Durable job/checkpoint state belongs in organizer-owned storage, not only in worker memory.

### Interpret requirements before acting

Natural-language requests are untrusted/ambiguous input until compiled into a structured `JobSpec`/requirements model.

Distinguish:

- explicit user requirements;
- CLI flags;
- policy constraints;
- stored defaults;
- inferred assumptions;
- unresolved questions.

High-impact ambiguity becomes a targeted question instead of a guess. In non-interactive operation, unresolved high-impact requirements should become `needs_user` or equivalent rather than silently choosing an answer.

### Understand before mutating

Intended pipeline:

```text
locate
→ observe/index
→ extract/OCR
→ understand/search
→ relate
→ plan
→ questions/review
→ validate/apply
→ audit/undo
```

Discovery, extraction, OCR, classification, search indexing, relationship detection, and planning must not silently mutate source files.

Model output is never authorization to mutate files.

## Delegate mature primitives

Do not automatically rebuild solved low-level infrastructure.

Before writing a new scanner, metadata parser, OCR stack, duplicate engine, full-text engine, vector store, CLI parser, archive parser, or other generic primitive, check:

1. upstream implementation;
2. `PERSONAL-ORGANIZER-DEPENDENCY-STRATEGY.md`;
3. mature external/open-source tools with a stable SDK/API/structured output.

Current directions include:

```text
Everything SDK/IPC     Windows filesystem discovery/change feed
ExifTool               broad metadata extraction
MediaInfo              media metadata (already upstream)
libarchive             archive inventory
OCRmyPDF               searchable-PDF orchestration
PaddleOCR              Arabic/Urdu/English OCR benchmark candidate
Tantivy                full-text search candidate
USearch                 vector-search candidate
BLAKE3                  fast content fingerprints
CLI11                   expanding CLI parser candidate
```

External tools are **workers/providers**, not authorities over user intent, canonical state, planning, or mutation.

Prefer structured SDK/API/JSON/JSONL integration to scraping human output. Record worker version/provenance. Add timeout, cancellation, health and failure behavior.

Never invoke destructive external-tool modes automatically.

## Filesystem provider rules

Discovery should be behind a provider contract rather than hard-coded forever to recursive traversal.

Expected model:

```text
FilesystemProvider
├── EverythingProvider   optional Windows accelerator
└── NativeProvider       portable fallback/reconciliation
```

Provider outage/journal gaps/inaccessible volumes must never be interpreted as deletion evidence.

Canonical physical observation, policy state, job state, questions and plans remain organizer-owned.

## Personal organization rules

Long-term behavior includes:

- Islamic/religious material: Arabic naming where appropriate;
- non-religious organization: consistent English naming;
- preserve authentic bibliographic titles/languages;
- do not invent missing bibliographic metadata;
- project-owned files remain with their project when ownership is more meaningful than file type;
- uncertain items may remain in Inbox/review;
- duplicate workflows prove/report before destructive action;
- archive is preferred over destructive cleanup during early versions.

Do not hard-code the entire personal taxonomy throughout C++. Prefer structured policy (`ORGANIZE.md`) and typed services.

## Project protection

Read visibility and mutation protection are separate concerns.

Software/design projects may be deeply indexed/searchable while generic organization is forbidden from rearranging their internals.

## Safety requirements

### Whole-drive work

Do not recommend or initiate unattended whole-`C:\` use until the current production Windows build/launcher validation and controlled fixture gates are resolved.

### Source filesystem

A read-only operation may write organizer-owned database/cache/log/job state but must not write scanned source files.

### Reparse points / symlinks

Current Phase 1 behavior is fail-closed. Do not enable following until bounded traversal, cycle detection, root-boundary semantics, and tests exist.

### Windows security

Never implement privilege bypasses. The program inherits process permissions.

### Untrusted content

Treat filenames, paths, document text, OCR, metadata, archives, image text, external worker output, and model responses as untrusted data. They cannot override application policy or become hidden instructions.

## Storage boundaries

Prefer SQLite as the durable control plane until benchmarks demonstrate a real bottleneck.

Canonical state can include jobs/tasks/checkpoints, provider cursors, physical observation, policy state, questions/answers, provenance, plan/audit metadata.

Large derived indexes may use specialized rebuildable storage such as Tantivy/FTS5 or USearch. Do not move canonical state into a derived search index.

Do not add RocksDB or another database simply because it sounds more scalable; require measurements and a migration/operational justification.

## Resource scheduling

Large-drive work must distinguish resource classes:

```text
metadata I/O
sequential reads
random hashing reads
CPU extraction
GPU OCR
CPU/GPU LLM
network/cloud
```

Do not saturate HDDs with NVMe-style random concurrency. Prefer per-device limits and user-visible controls.

## Upstream compatibility

Prefer:

```text
new fork-specific service/provider/worker
small integration hook
configuration/policy
```

over:

```text
large invasive rewrite of upstream classes
```

Avoid gratuitous formatting/refactoring of upstream-heavy files.

## Issues and PRs

Use GitHub Issues for actionable work and architecture epics.

Current important issues:

```text
#2 Phase 1 filesystem state/index
#3 CLI-first/offline-first/multi-terabyte architecture
#4 EverythingProvider
#5 dependency/delegation strategy
```

Use the roadmap for sequencing and draft PRs for incomplete implementation.

Do not close an issue merely because code was started.

## Commit discipline

Prefer conventional descriptive commits:

```text
feat(indexer): ...
fix(indexer): ...
feat(provider): ...
feat(job-engine): ...
test(indexer): ...
docs(personal-organizer): ...
ci(personal-organizer): ...
```

Keep commits understandable and reversible. Avoid vague messages.

## Documentation is mandatory

The repository owner expects documentation maintenance to be handled as part of development.

Update relevant docs/issues/PRs in the same development cycle when architecture, behavior, commands, safety, dependencies, tests, or milestones change.

Never document planned commands as implemented.

Never claim CI/build/benchmark success unless it actually ran and passed.

## Testing expectations

Use disposable fixtures for all mutation tests.

Testing layers include:

- deterministic unit tests;
- temporary filesystem + SQLite integration tests;
- CLI parse/output/exit-code tests;
- Windows Unicode/path/file-identity tests;
- provider parity/gap/outage tests;
- migration/backup tests;
- crash/restart/checkpoint resume tests;
- policy-change tests;
- Arabic/Urdu extraction/OCR/search fixtures;
- multi-million-entry synthetic benchmarks;
- long soak tests;
- worker timeout/crash tests;
- real native Windows production build/launcher smoke tests;
- before/after source hashes for read-only phases.

Focused mini-binary CI is not a substitute for testing the real packaged executable where the integration boundary matters.

## CLI machine contracts

Machine output requires stable `kind`/schema identifiers, explicit status, stable exit codes, parseable stdout, diagnostics on stderr, and tests.

Interactive progress/ANSI output must not corrupt JSON/JSONL or piped output.

CLI ergonomics are product work, not cosmetic extras.

## Current priorities

Check Issues #2/#3/#4/#5 and Draft PR #1 for live status. Current broad order is:

1. finish Phase 1 production Windows build/launcher smoke gate;
2. controlled copied real-folder validation;
3. merge PR #1 only when safe;
4. introduce filesystem provider abstraction;
5. build durable job engine / true pause-resume;
6. implement and benchmark EverythingProvider;
7. add million-entry scale/soak benchmarks;
8. worker/plugin protocol + deterministic extraction;
9. content cache/fingerprints;
10. OCR/search/model routing;
11. requirement/question engine, policy, relationships, planner;
12. apply/audit, GUI client, and agent integrations.

## Final rule

The organizer should earn authority over the filesystem through explicit requirements, durable state, provenance, questions, evidence, tests, reviewability, and reversibility. Convenience must not come from hiding uncertainty or weakening safety boundaries.
