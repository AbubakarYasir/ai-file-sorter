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
docs/PERSONAL-ORGANIZER-SYSTEM-QUALITY-BAR.md
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

### Progressive intelligence

Do not treat a whole-drive inventory as permission to hash/OCR/embed/LLM-analyze the whole drive.

Prefer:

```text
discover
→ physical metadata
→ deterministic metadata/content extraction
→ selective OCR
→ lexical/full-text indexing
→ relationships/bibliography
→ embeddings where useful
→ LLM reasoning only where justified
```

Expensive work should be demand-driven, cacheable, budget-aware, and resumable.

### Scale is a requirement

Design for:

```text
8TB+ storage
millions/tens of millions of entries
hours-to-days jobs
mixed HDD/SSD/NVMe
multiple/removable volumes
```

Do not introduce algorithms that require all paths, extracted text, OCR, embeddings, or plans in memory at once.

Use streaming/paging, bounded queues/backpressure, durable tasks, incremental caches, and per-device resource controls.

### Long work must be durable

A future long job must survive process crash/reboot and support true checkpoint-based pause/resume. Do not misuse the word `resumable` for a process that merely starts another scan and reuses some existing rows.

Durable job/checkpoint state belongs in organizer-owned storage, not only in worker memory.

Closing a terminal must not be allowed to destroy hours of completed durable work once the job engine exists.

### Interpret requirements before acting

Natural-language requests are untrusted/ambiguous input until compiled into a structured `JobSpec`/requirements model.

Distinguish:

- explicit user requirements;
- CLI flags;
- policy constraints;
- profiles/stored defaults;
- inferred assumptions;
- unresolved questions.

High-impact ambiguity becomes a targeted question instead of a guess. In non-interactive operation, unresolved high-impact requirements should become `needs_user` or equivalent rather than silently choosing an answer.

Related questions should be batched where practical; unrelated work may continue while only affected tasks wait.

### Understand before mutating

Intended pipeline:

```text
locate
→ observe/index
→ extract/OCR
→ understand/search
→ relate
→ plan
→ validate/simulate
→ questions/review
→ apply
→ audit/undo
```

Discovery, extraction, OCR, classification, search indexing, relationship detection, and planning must not silently mutate source files.

Model output is never authorization to mutate files.

## Truth/state boundaries

Do not collapse these concepts:

```text
physical observation
policy/in-scope state
deterministic derived data
probabilistic/inferred data
user intent/answers
plan state
audit/applied state
```

A provider outage, inaccessible root, policy exclusion, failed worker, or uncertain model must not silently rewrite physical truth.

Observation epochs/snapshots and provider reconciliation should eventually replace simplistic “seen/not seen” assumptions at scale.

## Delegate mature primitives

Do not automatically rebuild solved low-level infrastructure.

Before writing a new scanner, metadata parser, OCR stack, duplicate engine, full-text engine, vector store, CLI parser, archive parser, source-code parser, or other generic primitive, check:

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
Tantivy / FTS5         full-text search candidates
USearch                 vector-search candidate
BLAKE3                  fast content fingerprints
CLI11                   expanding CLI parser candidate
Tree-sitter             source-code structure candidate
Git/libgit2 adapter     repository metadata candidate
ICU                     multilingual normalization/tokenization candidate
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

## File/volume identity rules

Paths are not sufficient identity for all serious workflows.

Where supported, consider volume identity, platform file ID, hardlink identity, normalized path key, size/timestamps, and staged content fingerprints.

Windows edge cases requiring deliberate semantics/tests include:

- Unicode and long paths;
- case-only path collisions;
- hardlinks;
- reparse/junction/mount points;
- sparse/compressed/encrypted files;
- removable/offline volumes;
- cloud placeholders.

Ordinary inventory should avoid accidentally hydrating cloud-placeholder files unless policy explicitly allows the I/O cost.

## Personal organization rules

Long-term behavior includes:

- Islamic/religious material: Arabic naming where appropriate;
- non-religious organization: consistent English naming;
- preserve authentic bibliographic titles/languages;
- do not invent missing bibliographic metadata;
- project-owned files remain with their project when ownership is more meaningful than file type;
- uncertain items may remain in Inbox/review;
- exact duplicates, derivatives, editions, translations, and related files remain distinct claims;
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

Large derived OCR/text/chunk/thumbnail data may use a content cache rather than bloating control tables.

Do not add RocksDB or another database simply because it sounds more scalable; require measurements and a migration/operational justification.

## Resource scheduling and budgets

Large-drive work must distinguish resource classes:

```text
metadata I/O
sequential reads
random hashing reads
CPU extraction
GPU OCR
CPU/GPU LLM
network/cloud
search-index writes
```

Do not saturate HDDs with NVMe-style random concurrency. Prefer per-device limits and user-visible controls.

Before expensive jobs, expose estimates where reasonably knowable and support explicit future budgets for read volume, OCR pages, runtime, cache growth, and cloud cost/tokens.

When a hard budget is exhausted, checkpoint cleanly rather than silently exceeding it.

## Plans and explainability

Important inferred facts/relations/plans should retain evidence/provenance sufficient for `why`/explain workflows.

Plans should be versioned artifacts tied to source state, JobSpec, policy/config identity, answers, workers/models, evidence, and unresolved assumptions.

Before apply, use deterministic validation plus virtual-filesystem simulation for collisions, invalid/path-length/case issues, protected structures, changed sources, available space, circular moves, and cross-volume semantics.

Apply must not silently regenerate an already approved plan using fresh model output.

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
#6 durable daemon/job/checkpoint/resource runtime
#7 intent/questions/policy/explainability
#8 8TB snapshots/reconciliation/benchmarks/fault injection
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
feat(policy): ...
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
- 1M/10M synthetic-entry benchmarks;
- bounded-memory verification;
- long soak tests;
- worker timeout/crash/malformed-output tests;
- drive disconnect/reconnect tests;
- provider journal-gap/reset reconciliation tests;
- hardlink/cloud-placeholder/long-path fixtures;
- real native Windows production build/launcher smoke tests;
- before/after source hashes for read-only phases.

Focused mini-binary CI is not a substitute for testing the real packaged executable where the integration boundary matters.

Performance regressions should eventually have documented thresholds rather than subjective “feels fast” acceptance.

## CLI machine contracts

Machine output requires stable `kind`/schema identifiers, explicit status, stable exit codes, parseable stdout, diagnostics on stderr, and tests.

Interactive progress/ANSI output must not corrupt JSON/JSONL or piped output.

CLI ergonomics are product work, not cosmetic extras.

Human explicit commands, guided interactive flows, and natural-language intent must resolve into the same typed core behavior rather than three separate implementations.

## Current priorities

Check Issues #2–#8 and Draft PR #1 for live status. Current broad order is:

1. finish Phase 1 native Windows build/packaged-launcher/no-mutation smoke gate;
2. controlled copied real-folder validation;
3. merge PR #1 only when safe;
4. introduce filesystem provider abstraction and Everything acceleration;
5. build durable job runtime / true pause-resume / resource scheduler;
6. add observation snapshots/reconciliation and million-entry/fault testing;
7. build typed intent/question/policy engine;
8. worker/plugin protocol + deterministic extraction;
9. content identity/cache/fingerprints;
10. Arabic/Urdu OCR + full-text search + semantic search;
11. offline/online model routing and task-specific model benchmarks;
12. relationships/lineage/bibliography/duplicates;
13. planner + simulation + review;
14. validated apply/audit/undo;
15. continuous operation, GUI client, and agent integrations.

## Final rule

The organizer should earn authority over the filesystem through explicit requirements, durable state, provenance, questions, evidence, tests, simulation, reviewability, and reversibility. Convenience must not come from hiding uncertainty, wasting terabytes of I/O, or weakening safety boundaries.
