# Personal AI File Organizer — Canonical Checkpoints and Stage Gates

This document is the **canonical execution checklist** for the Personal AI File Organizer fork.

It does not replace the roadmap, architecture, quality bar, CLI contract, dependency strategy, development log, issues, or pull requests. It converts them into explicit **engineering gates** so that at any time we can answer:

> Where are we?
> What is already proven?
> What remains?
> What evidence is required?
> What is the next checkpoint?
> What are we explicitly not allowed to claim or start yet?

The roadmap explains **what we are building and in what order**. This file defines **what must be true before a stage is allowed to advance**.

---

## 1. Authority and relationship to other documents

Canonical supporting documents:

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

Tracking issues currently include:

```text
#2  Phase 1 filesystem state/index foundation
#3  CLI-first/offline-first multi-terabyte architecture epic
#4  EverythingProvider
#5  dependency/delegation strategy
#6  durable daemon/job/checkpoint/resource runtime
#7  intent/questions/policy/explainability
#8  8TB snapshots/reconciliation/benchmarks/fault injection
```

Rules:

1. The roadmap owns high-level sequencing.
2. Architecture documents own design boundaries.
3. Issues own actionable scope and acceptance details.
4. PRs own the actual implementation slice under review.
5. The development log records important engineering history and why decisions changed.
6. **This file owns checkpoint status and exit-gate structure.**
7. A checkpoint may be marked `PASS` only after the required evidence actually exists.
8. Planned behavior must never be marked implemented merely because design work exists.
9. If code, issue state, docs, tests, or this file disagree, resolve the disagreement explicitly; do not silently choose the most optimistic version.

---

## 2. Checkpoint status model

Use only these statuses:

```text
PASS      completed, qualified, documented, and integrated as required
ACTIVE    current work or final qualification is in progress
BLOCKED   cannot advance because a required gate is failing or unavailable
PLANNED   defined but implementation has not begun
DEFERRED  deliberately postponed by an explicit architecture/product decision
```

A checkpoint is **not PASS** merely because:

- code exists;
- unit tests pass;
- a happy-path demo worked once;
- a design document is complete;
- focused mini-binary CI passes while the production boundary is untested;
- a provider returned data once;
- a model produced good-looking output;
- a mutation appeared successful without validation/audit evidence.

PASS means the checkpoint's **exit criteria and evidence pack are complete**.

---

## 3. Universal gate rules

These rules apply to every checkpoint unless the checkpoint explicitly narrows them.

### 3.1 Safety boundary

Read-only stages may write organizer-owned state such as:

```text
SQLite control/index databases
WAL/support files
logs
cache
checkpoint state
worker outputs
search/vector derived indexes
artifacts and reports
```

They may not silently mutate scanned source files.

Model output, OCR text, filenames, metadata, archive contents, worker output, and document content are **untrusted data**. None may override application policy or become hidden instructions.

### 3.2 CLI-first rule

An important capability is not considered complete until it has a safe core/CLI contract or a deliberate documented reason for being internal-only.

Machine output must preserve:

```text
stable kind/schema identity
explicit status
stable exit behavior
parseable stdout
stderr diagnostics
no ANSI/progress pollution in structured mode
Unicode-safe paths
```

### 3.3 Offline-first rule

Baseline filesystem observation, deterministic extraction, OCR/search primitives, planning primitives, and local model paths should remain usable without cloud services where technically practical.

Cloud is optional and policy-controlled.

### 3.4 Progressive-intelligence rule

Never interpret a large inventory as permission to deeply analyze everything.

Preferred escalation:

```text
discover
→ physical metadata
→ deterministic metadata/content extraction
→ selective OCR
→ lexical/full-text indexing
→ relationships/bibliography
→ embeddings when useful
→ LLM reasoning only when justified
```

### 3.5 Scale rule

Design assumptions:

```text
8TB+ total storage
millions to tens of millions of filesystem entries
multiple volumes/devices
mixed HDD/SSD/NVMe
multi-hour/day jobs
large Arabic/Urdu/English document corpora
large project/design/media trees
```

No checkpoint may introduce an architecture that requires the entire filesystem, extracted corpus, OCR corpus, embeddings, or plan set to fit in RAM.

### 3.6 Truth separation rule

Keep these concepts distinct:

```text
physical observation
policy/in-scope state
deterministic derived data
probabilistic/inferred data
user intent/answers
plan state
audit/applied state
```

A provider outage, inaccessible drive, policy exclusion, failed worker, or uncertain model is never deletion evidence by itself.

### 3.7 Evidence rule

Every checkpoint evidence pack should contain, as applicable:

```text
branch
commit SHA(s)
PR / issue references
CI run IDs
commands executed
fixture definition
JSON/JSONL results
before/after hashes when read-only guarantees matter
SQLite verification
benchmark metrics
memory/resource metrics
failure/fault-injection results
manual/native platform observations
known limitations
updated docs
```

### 3.8 Documentation rule

A checkpoint cannot be PASS until relevant status is synchronized across:

- this file;
- roadmap;
- development log;
- CLI contract if user-facing behavior changed;
- issue checklist;
- PR description/evidence;
- architecture/dependency documents when design changed.

---

## 4. Repository and branch gates

Repository strategy:

```text
upstream/main
    ↓
origin/main
    ↓
personal-organizer
    ↓
feature/*
```

Rules:

- `main` remains upstream-friendly.
- `personal-organizer` is the stable custom integration branch.
- coherent custom work happens on `feature/*`.
- incomplete work normally enters `personal-organizer` through review.
- unrelated future work should not be stacked indefinitely onto an old feature branch.
- large architectural work should have an issue before implementation expands significantly.
- experimental organizer work should not be committed directly to `main`.

---

# 5. Current status snapshot — 2026-09-22

## CP-00 — Safe development foundation

**Status: PASS**

Established:

- personal fork and remotes;
- upstream-friendly `main`;
- `personal-organizer` integration branch;
- feature-branch workflow;
- fork-specific CI;
- owner-facing architecture/roadmap/CLI/development/agent documentation;
- GitHub Issues as the tracked backlog;
- draft PR workflow for incomplete organizer work.

## CP-01A — Trustworthy native filesystem observation + CLI index

**Status: ACTIVE — final production qualification**

Tracking:

```text
Issue #2
PR #1
feature/indexer → personal-organizer
```

Implemented and already covered by focused/regression work:

- dedicated SQLite `personal_file_index.db`;
- streaming native traversal;
- scan-run history;
- metadata persistence;
- optional SHA-256 hashing;
- protected-project detection;
- useful source indexing inside protected projects;
- generated internals such as `.git` / `node_modules` excluded;
- explicit `observation_state` vs `policy_state`;
- missing/inaccessible root preservation;
- stale genuinely deleted entries only after trustworthy complete observation;
- overlapping-root rejection;
- Unicode Windows argument/path handling;
- path-aware Windows system-root protections;
- reparse traversal fail-closed;
- schema versioning/migration backup;
- read-only query/statistics facade;
- controlled `index` CLI command;
- JSON result contract;
- focused Linux and Windows regression tests;
- packaged launcher fixes for synchronous index execution and structured stdout/stderr behavior.

Local native qualification already achieved on a controlled Windows machine includes:

```text
native production build               PASS
packaged launcher                     PASS
automatic CPU runtime discovery       PASS
Unicode / Arabic path                 PASS
valid JSON stdout                     PASS
isolated organizer database state     PASS
realistic copied fixture              63 files indexed
index errors                          0
before/after fixture hashes           unchanged
```

Remote status at this snapshot:

```text
Personal Organizer CI #130            PASS
Windows Production Gate #4            RUNNING
```

CP-01A must remain ACTIVE until the remote production gate, evidence synchronization, final review, and integration requirements below are complete.

---

# 6. Master checkpoint sequence

```text
CP-00   Safe development foundation                         PASS
CP-01A  Native observation + production CLI index           ACTIVE
CP-01B  FilesystemProvider abstraction                      PLANNED
CP-01C  Everything enumeration provider                     PLANNED
CP-01D  Provider change feed + reconciliation               PLANNED
CP-02   Durable jobs/checkpoints/runtime                    PLANNED
CP-03   Intent compiler + question engine + policy AST      PLANNED
CP-04   Resource-aware scheduler + estimates/budgets        PLANNED
CP-05   Content identity + reusable analysis cache          PLANNED
CP-06   Worker protocol + deterministic extraction          PLANNED
CP-07   Arabic/Urdu/English OCR                             PLANNED
CP-08   Search stack                                        PLANNED
CP-09   Offline/online model router + model benchmarks      PLANNED
CP-10   Stable taxonomy + ORGANIZE.md                       PLANNED
CP-11   Relationships + collections + lineage               PLANNED
CP-12   Exact duplicate engine                              PLANNED
CP-13   Bibliographic/document intelligence                 PLANNED
CP-14   Global organization planner                         PLANNED
CP-15   Validation + virtual-filesystem simulation          PLANNED
CP-16   Apply + audit + undo                                PLANNED
CP-17   CLI production-quality completion                   PLANNED
CP-18   GUI as shared-core client                           PLANNED
CP-19   Agent/MCP constrained interface                     PLANNED
CP-20   Continuous/watch operation                          PLANNED
```

The numbering deliberately separates Phase 1 provider work into multiple gates because discovery, provider acceleration, and trustworthy reconciliation are different risk boundaries.

---

# 7. Checkpoint definitions

## CP-00 — Safe development foundation

**Status: PASS**

### Objective

Establish a development process that keeps upstream compatibility, custom work isolation, testability, documentation, and review discipline.

### Exit criteria

- [x] fork/remotes configured;
- [x] `main` upstream-friendly;
- [x] `personal-organizer` integration branch exists;
- [x] `feature/*` workflow established;
- [x] fork CI exists;
- [x] roadmap/architecture/CLI/dev-log/agent rules exist;
- [x] issues track architecture/backlog;
- [x] incomplete implementation uses draft PRs where appropriate.

### Unlocks

CP-01A.

---

## CP-01A — Trustworthy native filesystem observation + CLI index

**Status: ACTIVE**

### Objective

Create a production-boundary read-only filesystem inventory that can persist trustworthy physical state without mutating scanned sources.

### Required implementation

- [x] dedicated organizer-owned SQLite index;
- [x] streaming traversal;
- [x] scan-run history;
- [x] metadata persistence;
- [x] optional hashing rather than mandatory whole-drive hashing;
- [x] physical observation separate from policy state;
- [x] trustworthy complete-vs-partial root semantics;
- [x] project mutation protection separate from read-only visibility;
- [x] generated project internals excluded;
- [x] Windows system-root refusal uses actual path/environment identity;
- [x] ordinary user-owned folders merely named `Windows` remain indexable;
- [x] Unicode paths/arguments preserved;
- [x] reparse following fail-closed;
- [x] overlapping roots rejected;
- [x] schema migration/versioning/backup path;
- [x] read-only query/statistics facade;
- [x] `index` command service independent of LLM readiness;
- [x] machine-readable JSON result;
- [x] runtime lock participation;
- [x] packaged Windows launcher synchronous behavior.

### Automated gates

- [x] focused Linux integration suite;
- [x] focused Windows integration suite;
- [x] missing-root preservation regression;
- [x] genuine deletion after complete rescan regression;
- [x] hidden/exclusion policy-change regression;
- [x] protected-project visibility regression;
- [x] overlapping-root regression;
- [x] reparse fail-closed regression;
- [x] Windows `%WINDIR%` refusal regression;
- [x] ordinary user directory named `Windows` regression;
- [x] Arabic/Unicode path regression;
- [x] migration + backup regression;
- [x] query facade tests;
- [x] CLI parse/output/exit-code tests;
- [x] ordinary Personal Organizer CI green on final head;
- [ ] Windows production packaged smoke gate green on final head.

### Native/manual qualification

- [x] native production MSVC/vcpkg build on Windows;
- [x] actual packaged `aifilesorter.exe index ...` invocation;
- [x] non-Latin/Arabic/Urdu root;
- [x] automatic CPU runtime path works without temporary DLL relocation;
- [x] structured JSON stdout parses cleanly;
- [x] diagnostics do not corrupt machine stdout;
- [x] SQLite result verified;
- [x] realistic copied fixture indexed;
- [x] project-root/protection behavior observed;
- [x] before/after source hashes prove no mutation;
- [x] errors = 0 on qualified copied fixture.

### Required final evidence before PASS

- [ ] final Windows Production Gate run ID recorded;
- [ ] all steps green on final PR head;
- [ ] synthetic smoke artifact inspected;
- [ ] database containment/isolated-state assertion proven;
- [ ] fresh isolated `runId == 1` assertion proven;
- [ ] CI artifact/results summarized in PR #1;
- [ ] local native qualification summarized in PR #1;
- [ ] Issue #2 checklist updated to observed reality;
- [ ] development log updated;
- [ ] roadmap status updated;
- [ ] `PERSONAL-ORGANIZER.md` status updated;
- [ ] CLI wording reconciled with final packaged behavior;
- [ ] final PR diff reviewed;
- [ ] PR #1 marked ready only after evidence is complete;
- [ ] merge into `personal-organizer`;
- [ ] post-merge sanity CI green.

### Explicit non-goals / not unlocked yet

Until PASS:

- no unattended whole-`C:\` recommendation;
- no claim of true checkpoint resume;
- no claim that recursive traversal is the final scale architecture;
- no Phase-2 content-intelligence pile-on inside `feature/indexer`.

### Unlocks

CP-01B.

---

## CP-01B — FilesystemProvider abstraction

**Status: PLANNED**

Tracking: Issue #4 and architecture epic #3.

### Objective

Stop hard-coding filesystem discovery forever to one recursive walker. Establish a typed provider contract while preserving CP-01A behavior.

### Required design

Conceptual interface:

```text
FilesystemProvider
├── kind / version / capabilities
├── health()
├── coverage(root/volume)
├── enumerate(root, page/cursor, requested_fields)
├── observe(path/provider_id)
├── changes(since_cursor)
└── reconciliation_hint()
```

### Required implementation

- [ ] introduce provider interface without breaking canonical index semantics;
- [ ] move existing native traversal behind `NativeProvider`;
- [ ] preserve observation/policy separation;
- [ ] persist provider provenance for observation epochs;
- [ ] expose provider health/capabilities in typed form;
- [ ] preserve bounded/streamed enumeration;
- [ ] preserve fallback behavior when optional providers are absent;
- [ ] no provider gets mutation authority.

### Gates

- [ ] CP-01A behavior parity tests pass through `NativeProvider`;
- [ ] Unicode paths pass;
- [ ] partial root semantics pass;
- [ ] project protection semantics pass;
- [ ] bounded-memory behavior demonstrated on a large synthetic result stream;
- [ ] provider outage cannot stale prior physical truth;
- [ ] docs and CLI diagnostics reflect provider abstraction.

### Exit criteria

Native behavior is fully available through the provider contract with no safety regression.

### Unlocks

CP-01C.

---

## CP-01C — Everything enumeration provider

**Status: PLANNED**

Tracking: Issue #4.

### Objective

Use voidtools Everything as an optional high-performance Windows discovery provider without making it mandatory or authoritative over canonical state.

### Required implementation

- [ ] runtime detection of compatible Everything instance(s);
- [ ] explicitly selectable instance name;
- [ ] version/capability detection;
- [ ] official SDK/IPC path, not human-output scraping;
- [ ] UTF-16/Unicode preservation end-to-end;
- [ ] bounded paging with `max`/`offset` or SDK3 equivalent;
- [ ] request only needed fields;
- [ ] exact provider coverage reporting;
- [ ] provider/version/instance provenance persisted;
- [ ] missing Everything → NativeProvider fallback;
- [ ] uncovered root/volume → fallback/reconciliation, not empty-success;
- [ ] no silent Everything configuration mutation.

### Tests

- [ ] fake-provider adapter unit tests;
- [ ] compatible 1.4-style enumeration where supported;
- [ ] 1.5/SDK3 enumeration fixture;
- [ ] Arabic/Urdu/Unicode path round trip;
- [ ] large synthetic paging test;
- [ ] Everything unavailable test;
- [ ] database-loading/degraded health test;
- [ ] provider coverage-incomplete test;
- [ ] NativeProvider parity counts/sampling;
- [ ] memory and latency benchmark.

### Exit criteria

Everything can accelerate enumeration safely while the CLI remains fully usable without it.

### Unlocks

CP-01D.

---

## CP-01D — Provider change feed, snapshots, and reconciliation foundation

**Status: PLANNED**

Tracking: Issues #4 and #8.

### Objective

Replace simplistic repeated full traversal assumptions with trustworthy provider cursors, observation epochs, journal/change processing, and reconciliation.

### Required implementation

Persist enough cursor/epoch identity to detect continuity:

```text
provider
instance
version
coverage identity
journal ID
change ID / next change ID
root/volume identity
last reconciled epoch
completeness/errors
```

- [ ] create/delete/rename/move/modify events;
- [ ] idempotent change processing;
- [ ] journal reset detection;
- [ ] cursor gap detection;
- [ ] provider rebuild/reset detection;
- [ ] coverage change detection;
- [ ] reconciliation-required state;
- [ ] trustworthy observation epochs/snapshots;
- [ ] provider disagreement becomes reconciliation work, not deletion;
- [ ] periodic parity/reconciliation policy;
- [ ] Windows volume/file identity groundwork where required.

### Fault gates

- [ ] provider outage;
- [ ] Everything database reload;
- [ ] journal reset;
- [ ] missed/gapped cursor;
- [ ] drive offline/reconnect;
- [ ] root coverage removed;
- [ ] path reused by a different file identity;
- [ ] hardlink cases.

### Exit criteria

Fast incremental provider updates can be consumed without manufacturing false deletions, and the system can explicitly identify when reconciliation is required.

### Unlocks

CP-02 and later scale validation.

---

## CP-02 — Durable job engine, checkpoints, and runtime

**Status: PLANNED**

Tracking: Issue #6.

### Objective

Make long-running work survive terminal closure, crashes, reboots, provider outages, and multi-hour/day execution.

### Required durable schema

```text
jobs
phases
tasks
attempts
checkpoints
leases
events
errors
questions
answers
provider cursors
worker/model versions
```

### Required behavior

- [ ] `job list`;
- [ ] `job status`;
- [ ] `job tail`;
- [ ] `job pause`;
- [ ] `job resume` from persisted checkpoints;
- [ ] `job cancel`;
- [ ] task idempotency;
- [ ] retry semantics;
- [ ] worker heartbeat/lease expiry;
- [ ] crash/reboot recovery;
- [ ] completed expensive work not replayed unnecessarily;
- [ ] `needs_user` is a non-failure state;
- [ ] structured event stream usable by CLI/GUI/agents.

### Qualification gates

- [ ] kill process during discovery and recover;
- [ ] kill process during extraction and recover;
- [ ] expire worker lease and recover;
- [ ] pause leaves clean persisted state;
- [ ] resume continues rather than restarting from zero;
- [ ] cancellation leaves consistent durable state;
- [ ] terminal closure does not lose completed work;
- [ ] SQLite busy/retry/recovery behavior tested;
- [ ] multi-hour soak test.

### Exit criteria

The project may finally claim **true checkpoint-based pause/resume**.

### Unlocks

Large-scale extraction/OCR/search jobs.

---

## CP-03 — Intent compiler, typed selectors, question engine, and policy AST

**Status: PLANNED**

Tracking: Issue #7.

### Objective

Make natural-language control powerful without allowing a model to silently redefine user intent or scope.

### Required implementation

- [ ] immutable typed `JobSpec`;
- [ ] source/evidence/confidence for important requirements;
- [ ] explicit precedence rules;
- [ ] typed selector/query algebra;
- [ ] AND/OR/NOT-style predictable scope composition;
- [ ] natural language compiles into the same selectors;
- [ ] preview/sample/count of resolved scope;
- [ ] persisted questions and answers;
- [ ] question impact scoring;
- [ ] related-question batching;
- [ ] independent tasks continue while affected work waits in `needs_user`;
- [ ] `ORGANIZE.md` parsed into structured policy AST;
- [ ] policy validation/lint/test/diff/explain;
- [ ] profile/recipe resolution is inspectable;
- [ ] `--why` / explain chain;
- [ ] immutable config/policy/JobSpec hashes for plans.

### Safety gates

- [ ] unresolved high-impact ambiguity cannot silently continue in non-interactive mode;
- [ ] natural-language scope cannot silently broaden beyond resolved typed selection;
- [ ] changed policy/answers invalidate affected plan assumptions;
- [ ] corrections are never silently promoted to permanent policy.

### Exit criteria

Human CLI flags, natural-language requests, profiles, and policy resolve into the same inspectable typed control model.

---

## CP-04 — Resource-aware scheduler, estimates, and budgets

**Status: PLANNED**

### Objective

Prevent large jobs from making the machine unusable and make expensive work predictable before execution.

### Resource classes

```text
filesystem metadata I/O
sequential content reads
random hashing reads
CPU extraction
GPU OCR
CPU/GPU local LLM
network/cloud API
search-index writes
```

### Required behavior

- [ ] bounded queues/backpressure;
- [ ] per-device scheduling;
- [ ] conservative HDD defaults;
- [ ] SSD/NVMe-appropriate concurrency;
- [ ] user controls for relevant concurrency/rate limits;
- [ ] exact vs estimated progress clearly distinguished;
- [ ] preflight estimates for entries/read bytes/hash/OCR/cache/cloud where knowable;
- [ ] hard budget handling;
- [ ] budget exhaustion checkpoints cleanly rather than silently exceeding limits.

### Exit criteria

Representative large jobs remain operationally usable under controlled resource budgets.

---

## CP-05 — Content identity and reusable analysis cache

**Status: PLANNED**

### Objective

Avoid rereading/reprocessing unchanged terabytes and preserve analysis across harmless rename/move operations.

### Identity ladder

```text
volume/platform file identity + size + mtime
→ sampled fingerprint where justified
→ BLAKE3 full hash where justified
→ SHA-256 for interoperability/audit when required
```

### Required behavior

- [ ] stable content identity model;
- [ ] hardlink-aware identity;
- [ ] rename/move can reuse prior expensive analysis;
- [ ] changed file invalidates stale derived results;
- [ ] cache keys include worker/model/version/settings;
- [ ] cache is rebuildable where appropriate;
- [ ] canonical state is not moved into a derived cache.

### Exit criteria

Expensive extraction/OCR/embedding work can be reused safely by content identity.

---

## CP-06 — Worker/plugin protocol + deterministic extraction

**Status: PLANNED**

### Objective

Integrate mature tools through narrow, versioned, observable capability boundaries instead of bloating the core executable.

### Initial candidates

```text
upstream DocumentTextAnalyzer / PDFium
ExifTool
MediaInfo
libarchive
libmagic/file typing
Tree-sitter / Git adapters where justified
```

### Worker contract gates

- [ ] versioned protocol;
- [ ] capability manifest;
- [ ] timeout;
- [ ] cancellation;
- [ ] stderr/log capture;
- [ ] retry/failure semantics;
- [ ] health reporting;
- [ ] provenance recording;
- [ ] malformed-output handling;
- [ ] worker crash does not corrupt canonical state;
- [ ] external output treated as untrusted data.

### Exit criteria

Deterministic metadata/content extraction works through durable, inspectable workers without creating hidden authority outside the organizer.

---

## CP-07 — Arabic/Urdu/English OCR

**Status: PLANNED**

### Objective

Understand scanned documents offline with selective, benchmarked OCR rather than whole-corpus brute force.

### Candidate engines

```text
PaddleOCR   leading multilingual candidate
OCRmyPDF    PDF orchestration
Tesseract   compatibility/fallback
Surya       OCR/layout benchmark candidate
```

### Required pipeline

```text
measure usable text layer
→ decide whether OCR is needed
→ strategic pages first
→ OCR
→ Unicode normalization
→ page/layout confidence
→ escalate to more pages only when justified
```

### Benchmark gates

- [ ] Arabic printed scans;
- [ ] Urdu printed scans;
- [ ] English scans;
- [ ] mixed-language pages;
- [ ] title/publication pages;
- [ ] noisy/low-resolution cases;
- [ ] quality/latency/RAM/VRAM measurements;
- [ ] worker restart/cache reuse;
- [ ] OCR output provenance and confidence.

### Exit criteria

The selected OCR stack wins on representative real workloads and is integrated as a selective cacheable worker path.

---

## CP-08 — Search stack

**Status: PLANNED**

### Objective

Make the personal filesystem searchable through deterministic and semantic evidence while preserving explainability.

### Search layers

```text
filename/path      provider search / Everything on Windows
structured state  SQLite
full text          benchmark FTS5 vs Tantivy
semantic vectors   USearch candidate
```

### Required behavior

- [ ] literal path/name search;
- [ ] structured filters;
- [ ] extracted text search;
- [ ] OCR text search;
- [ ] Arabic/Urdu normalization strategy;
- [ ] diacritic/mixed-language behavior tested;
- [ ] why-match evidence surfaced;
- [ ] deterministic and semantic scores remain distinguishable;
- [ ] derived index rebuild does not destroy canonical state;
- [ ] bounded-memory indexing/search.

### Exit criteria

Representative personal queries can find files and explain whether the match came from path, metadata, exact text, OCR, bibliography, relationships, or semantic similarity.

---

## CP-09 — Offline/online model router + model benchmarks

**Status: PLANNED**

### Objective

Use local models as the baseline and optional cloud models only when capability, privacy, quality, and budget policy justify escalation.

### Provider direction

```text
bundled llama.cpp
Ollama
LM Studio
custom OpenAI-compatible endpoint
OpenAI
Gemini
other explicitly configured providers
```

### Required gates

- [ ] path/task privacy policy;
- [ ] local-only enforcement;
- [ ] ask-before-cloud behavior;
- [ ] metadata-only-cloud behavior where supported;
- [ ] provider health/capability diagnostics;
- [ ] provider/model/version provenance;
- [ ] task-oriented benchmark fixtures;
- [ ] Arabic bibliographic/document workloads;
- [ ] mixed-language classification;
- [ ] latency/RAM/VRAM/cost metrics;
- [ ] optional provider outage cannot brick baseline CLI.

### Exit criteria

Routing decisions are policy-driven and benchmark-informed rather than based on marketing names or hidden defaults.

---

## CP-10 — Stable taxonomy + `ORGANIZE.md`

**Status: PLANNED**

### Objective

Define predictable personal organization rules rather than inventing folders on every run.

Starting top-level direction:

```text
00 Inbox
01 Islamic Studies
02 Markaz
03 Development
04 Design
05 Learning
06 Personal
07 Library
99 Archive
```

### Required policy capabilities

- [ ] allowed roots/destinations;
- [ ] Arabic naming for Islamic/religious organization where appropriate;
- [ ] consistent English naming for non-religious organization;
- [ ] bibliographic titles preserve authentic language/spelling;
- [ ] protected paths;
- [ ] project ownership rules;
- [ ] archive behavior;
- [ ] privacy rules;
- [ ] confidence thresholds;
- [ ] never-move / always-review rules;
- [ ] duplicate policy;
- [ ] policy fixtures/tests.

### Exit criteria

Organization policy is explicit, versioned, testable, explainable, and not scattered through ad-hoc C++ conditions.

---

## CP-11 — Relationships, collections, and lineage

**Status: PLANNED**

### Objective

Understand meaningful groups before planning individual file movements.

### Relationship types

```text
is_edition_of
is_translation_of
is_volume_of
is_annotation_of
is_ocr_derivative_of
is_export_of
belongs_to_project
uses_asset
has_supplement
is_exact_duplicate_of
is_possible_near_duplicate_of
```

### Required gates

- [ ] evidence-backed relationship graph;
- [ ] source/provenance/confidence per edge;
- [ ] book + notes + annotations;
- [ ] source scan + OCR derivative;
- [ ] original + translation;
- [ ] multi-volume work;
- [ ] source design + exports;
- [ ] repo/project docs;
- [ ] course bundles;
- [ ] lineage remains distinct from duplicate identity.

### Exit criteria

Planner/search can operate on relationships/collections rather than treating every file as independent.

---

## CP-12 — Exact duplicate engine

**Status: PLANNED**

### Objective

Prove exact duplicates efficiently before any destructive duplicate workflow is considered.

### Staged strategy

```text
size
→ cached/sampled fingerprint
→ full hash only for candidates
→ exact duplicate group
```

### Gates

- [ ] reuse existing cached hashes;
- [ ] hardlinks distinguished from separate duplicate bytes;
- [ ] derivatives/editions/translations not mislabeled exact duplicates;
- [ ] fclones benchmark/reference where useful;
- [ ] SSD/HDD strategy benchmarked;
- [ ] report-only first;
- [ ] no destructive external duplicate modes invoked automatically.

### Exit criteria

Exact duplicate groups are deterministic, explainable, and efficient at large scale.

---

## CP-13 — Bibliographic/document intelligence

**Status: PLANNED**

### Objective

Represent books/papers/documents with evidence-backed bibliographic fields suitable for serious Arabic/Islamic and general library organization.

### Fields may include

```text
title
author/editor/muhaqqiq
publisher
edition
volume
year
language
subject
identifiers
```

### Evidence priority

```text
trusted embedded metadata
→ title/publication page
→ embedded text
→ OCR
→ existing path context
→ model inference only when necessary
```

### Gates

- [ ] field-level evidence;
- [ ] confidence;
- [ ] worker/model provenance;
- [ ] authentic title/language preserved;
- [ ] missing facts remain missing rather than fabricated;
- [ ] representative Arabic book fixtures;
- [ ] multi-volume/edition/translation cases.

### Exit criteria

Bibliographic metadata can support search, relationships, and planning without invented facts.

---

## CP-14 — Global organization planner

**Status: PLANNED**

### Objective

Turn knowledge + policy + explicit requirements into a serializable proposal without mutating files.

### Plan entry model

```text
source identity/path
proposed action
destination/name
requirements used
policy rules used
evidence
worker/model provenance
confidence
relationships
conflicts
warnings
unresolved questions
```

### Required gates

- [ ] planner never executes actions;
- [ ] plan schema/version;
- [ ] JobSpec hash;
- [ ] policy/config identity;
- [ ] evidence identity;
- [ ] model/worker provenance;
- [ ] unresolved assumptions explicit;
- [ ] plan diff when policy/model/answers change;
- [ ] stale source state invalidates affected actions.

### Exit criteria

Plans are reproducible artifacts, not ephemeral model output.

---

## CP-15 — Deterministic validation + virtual-filesystem simulation

**Status: PLANNED**

### Objective

Prove a plan is structurally safe before real filesystem mutation.

### Simulation must detect

```text
path collisions
case-only collisions
Windows-invalid names
path-length problems
cross-volume operations
circular moves
insufficient destination space
protected project/collection breakage
changed source identity/state
permission problems
```

### Gates

- [ ] deterministic validation independent of fresh model output;
- [ ] virtual/overlay filesystem simulation;
- [ ] source freshness check;
- [ ] protected/project constraints;
- [ ] required questions answered;
- [ ] duplicate certainty requirements;
- [ ] plan approval does not silently regenerate on apply.

### Exit criteria

A reviewed plan can be proven internally consistent against the current source state before mutation.

---

## CP-16 — Apply, audit, and undo

**Status: PLANNED**

### Objective

Perform approved filesystem changes conservatively with explicit guarantees and durable audit state.

### Required behavior

- [ ] apply only validated approved plan artifact;
- [ ] revalidate source identity immediately before operation;
- [ ] recoverable/chunked operation journal;
- [ ] same-volume move semantics documented;
- [ ] cross-volume copy → verify → durable checkpoint → remove source only when authorized;
- [ ] audit original/final state;
- [ ] truthful undo guarantees;
- [ ] failed operation leaves inspectable recoverable state;
- [ ] upstream review/apply/undo reused where contracts remain correct.

### Exit criteria

Mutation authority is earned through explicit requirements, plan validation, simulation, review, audit, and recoverability.

---

## CP-17 — CLI production-quality completion

**Status: PLANNED**

### Objective

Turn the growing command set into a coherent long-lived product interface.

### Target command families

```text
aifs doctor
aifs provider ...
aifs index ...
aifs snapshot ...
aifs diff ...
aifs reconcile ...
aifs job ...
aifs estimate ...
aifs inspect ...
aifs extract ...
aifs ocr ...
aifs search ...
aifs related ...
aifs duplicates ...
aifs policy ...
aifs plan ...
aifs review ...
aifs apply ...
aifs undo ...
aifs models ...
aifs components ...
aifs shell
```

### Quality gates

- [ ] human-readable default output;
- [ ] JSON/JSONL machine modes;
- [ ] versioned schemas;
- [ ] stable exit codes;
- [ ] no ANSI/progress noise when piped;
- [ ] UTF-8 paths;
- [ ] shell completion;
- [ ] high-quality help/errors;
- [ ] `--offline` / privacy controls;
- [ ] `--dry-run` boundaries;
- [ ] `--explain` / `--why` where inference exists;
- [ ] CLI11 or equivalent serious parser if justified;
- [ ] `doctor` reports optional capability health.

### Exit criteria

The CLI is a stable human and automation surface rather than a collection of ad-hoc flags.

---

## CP-18 — GUI as shared-core client

**Status: PLANNED**

### Objective

Make the GUI a rich visual client over the same core services rather than a second implementation.

### Potential views

```text
Overview
Jobs
Index
Search
Questions
Collections
Duplicates
Review
Policy
History / Undo
Diagnostics
```

### Gates

- [ ] no unique organization authority exists only in widgets;
- [ ] GUI uses same jobs/providers/search/policy/planner services;
- [ ] GUI respects locks/privacy/questions/audit;
- [ ] core behavior remains reproducible from CLI.

### Exit criteria

GUI adds presentation and review power without splitting product semantics.

---

## CP-19 — Agent/MCP constrained interface

**Status: PLANNED**

### Objective

Allow automation/agents to use organizer capabilities without granting raw filesystem authority.

### Allowed direction

Agents may eventually:

```text
query providers/index
start approved analysis jobs
inspect job status
answer persisted questions
search
build/inspect plans
request validated apply
```

### Gates

- [ ] same policy/privacy model as CLI;
- [ ] same jobs/questions/plans;
- [ ] same validation/audit path;
- [ ] no raw mutation bypass;
- [ ] machine schemas stable enough for automation.

### Exit criteria

Agent workflows are clients of the product rather than privileged bypasses.

---

## CP-20 — Continuous/watch operation

**Status: PLANNED**

### Objective

Support ongoing personal filesystem intelligence after the initial organization is stable.

### Sources may include

```text
Downloads
Desktop
scanner imports
phone imports
course downloads
project exports
media/design exports
watched inbox roots
```

### Gates

- [ ] incremental provider/change-feed path stable;
- [ ] durable background jobs;
- [ ] policy-driven proposals;
- [ ] user questions when needed;
- [ ] no silent mutation outside approved policy/apply boundaries;
- [ ] bounded resource usage;
- [ ] audit/history remains continuous.

### Exit criteria

The organizer can maintain a stable personal filesystem over time rather than functioning only as a one-off cleanup tool.

---

# 8. Cross-cutting scale qualification gates

Issue #8 defines the standard for claiming large-scale readiness. These tests are introduced progressively as relevant subsystems mature.

A subsystem must not claim 8TB/multi-million-file readiness until the relevant gates are demonstrated.

Required long-term qualification set:

```text
[ ] 1M synthetic-entry benchmark
[ ] 10M synthetic-entry benchmark
[ ] bounded-memory verification
[ ] bounded queues/backpressure verification
[ ] multi-hour soak
[ ] kill/restart recovery
[ ] drive disconnect/reconnect
[ ] provider reset/journal-gap handling
[ ] SQLite busy/recovery/migration
[ ] worker timeout/crash/malformed output
[ ] Unicode paths
[ ] long paths
[ ] case-only collisions
[ ] hardlinks/file IDs
[ ] reparse/junction/mount-point cases
[ ] sparse/compressed/encrypted files
[ ] cloud placeholder behavior
[ ] ordinary inventory does not accidentally hydrate placeholders
[ ] before/after hashes for read-only fixture guarantees
[ ] documented performance regression thresholds
```

Performance claims must include the environment and measurement method. “Feels fast” is not a benchmark.

---

# 9. Required evidence pack template

Every checkpoint PR or final qualification note should include a compact evidence block in this form:

```markdown
## Checkpoint evidence

Checkpoint: CP-XX
Status requested: PASS
Branch: feature/...
Head SHA: ...
PR: #...
Issues: #...

### Automated
- CI workflow/run: ...
- Result: PASS
- Relevant tests: ...

### Native/manual
- OS/build: ...
- Command(s): ...
- Fixture: ...
- Result: ...

### Safety
- Source mutation check: ...
- Before/after hashes: ...
- Isolation/containment: ...

### Scale/performance
- Entries: ...
- Elapsed: ...
- Peak memory: ...
- Read volume: ...
- Notes: ...

### Known limitations
- ...

### Documentation synchronized
- [ ] checkpoints
- [ ] roadmap
- [ ] development log
- [ ] CLI contract if applicable
- [ ] issue
- [ ] PR
```

No checkpoint should rely on chat history as its only evidence.

---

# 10. Pull request merge gate template

Before merging a checkpoint implementation into `personal-organizer`:

```text
[ ] checkpoint exit criteria complete
[ ] final branch head tested
[ ] required CI green on final head
[ ] required native/manual qualification complete
[ ] source-safety assertions complete where applicable
[ ] benchmark/fault gates complete where applicable
[ ] no unexplained failures ignored
[ ] PR diff reviewed
[ ] unrelated temporary/debug files removed
[ ] issue checklist synchronized
[ ] roadmap synchronized
[ ] development log synchronized
[ ] CLI docs synchronized if behavior changed
[ ] known limitations explicitly recorded
[ ] follow-up work split into scoped issues
[ ] branch can be merged without claiming unimplemented behavior
```

After merge:

```text
[ ] integration branch CI green
[ ] checkpoint marked PASS in this file
[ ] next feature branch starts from the intended integration head
```

---

# 11. Current immediate action list

For the current project state, do **not** skip ahead.

```text
CP-01A FINALIZATION

[x] source implementation
[x] focused Linux/Windows regression suites
[x] local native Windows production build
[x] packaged launcher qualification
[x] Unicode/Arabic qualification
[x] realistic copied fixture qualification
[x] before/after no-mutation hashes
[x] ordinary GitHub Personal Organizer CI on current head

[ ] Windows Production Gate #4 completes green
[ ] inspect smoke evidence artifact
[ ] record final CI run/evidence
[ ] synchronize Issue #2
[ ] synchronize PR #1
[ ] update development log
[ ] update roadmap
[ ] update product overview status
[ ] reconcile CLI status wording
[ ] final PR review
[ ] merge feature/indexer → personal-organizer
[ ] post-merge sanity CI
[ ] mark CP-01A PASS

THEN:

[ ] create/start CP-01B implementation branch from personal-organizer
```

Do not start OCR, taxonomy automation, semantic embeddings, or mutation planning as substitutes for finishing the foundational gates.

---

# 12. Final project rule

The Personal AI File Organizer should earn increasing authority over the filesystem only as its evidence becomes stronger.

The progression is intentional:

```text
observe safely
→ prove observation semantics
→ accelerate discovery without losing truth
→ make long work durable
→ compile intent explicitly
→ control resources
→ build reusable content knowledge
→ search and relate evidence
→ plan without mutating
→ validate and simulate
→ review
→ apply conservatively
→ audit and undo honestly
```

Convenience must not come from hiding uncertainty, wasting terabytes of I/O, treating provider gaps as deletion, allowing model guesses to become authority, or claiming production readiness without evidence.
