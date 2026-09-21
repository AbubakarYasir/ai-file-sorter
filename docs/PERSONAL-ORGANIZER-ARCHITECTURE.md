# Personal AI File Organizer — Architecture

This document defines the fork-specific architecture layered on top of Hyperfield AI File Sorter. It is intentionally separate from upstream `docs/architecture.md`.

The target is not a slightly better file sorter. It is a **CLI-first, offline-first filesystem intelligence system** designed for multi-million-file, multi-terabyte personal storage, long-running jobs, multilingual content, and safe organization planning.

## Product surface

The CLI is the canonical interface.

```text
                              Shared core
 providers / durable jobs / extraction / OCR / search / models / policy / planning
                    /                 |                    \
                   v                  v                     v
                 CLI                 GUI                Agent/MCP
              PRIMARY             secondary             automation
```

A capability is not fully implemented until it has a safe CLI contract and can be consumed by other surfaces through the same core service.

The GUI is a useful secondary client for visual browsing/review. Agent integrations are automation clients. Neither may bypass policy, job state, validation, locks, audit, or mutation rules.

## Core safety rule

**Understand first, mutate later.**

```text
locate
  ↓
observe/index
  ↓
extract/OCR
  ↓
search/understand
  ↓
relationships + policy
  ↓
organization plan
  ↓
questions/review
  ↓
validated apply
  ↓
audit / undo
```

Discovery, analysis, model inference, and planning are read-only with respect to source files. A lower layer cannot move a file merely because it classified it.

---

## Scale invariants

The system is designed for 8TB+ storage and millions of entries.

Therefore:

- no phase may require all filesystem paths in RAM;
- no phase may assume all extracted text fits in RAM;
- provider results are paged/streamed;
- derived work is queued in bounded batches;
- the same unchanged content is not repeatedly reread;
- I/O parallelism is controlled per physical/logical device;
- long operations expose progress, status, pause, resume and cancel;
- crashes/reboots preserve completed work;
- workers are idempotent or have explicit retry semantics;
- inaccessible/offline roots preserve previous knowledge as `unknown`, not `deleted`;
- search/vector indexes are derived and rebuildable;
- the durable control database remains small enough to inspect, backup, migrate, and repair.

---

## Layer 1 — Requirement and intent compiler

The first intelligent layer interprets what I actually want before expensive analysis or planning begins.

Inputs can include:

```text
CLI flags
free-form natural-language request
ORGANIZE.md policy
stored profile/defaults
answers from this job/session
```

The output is a structured `JobSpec`, not an unconstrained model prompt.

Conceptual model:

```text
Requirement
├── value
├── source
│   ├── explicit_cli
│   ├── explicit_user_text
│   ├── policy
│   ├── stored_preference
│   └── inferred
├── confidence
└── evidence
```

The compiler must distinguish explicit instructions from inferred assumptions.

### Questions

If an unresolved ambiguity materially changes scope, cost, privacy, or filesystem outcome, it becomes a persisted question.

Examples:

```text
Should these Markaz exports stay beside their source PSD files?
Should cloud escalation be allowed for this folder?
Should alternate editions be grouped together or kept separately?
OCR title pages only first, or full documents?
```

Answers can be:

- job-only;
- session-scoped;
- optionally promoted into permanent policy.

In non-interactive mode, unresolved high-impact questions become `needs_user`; the program must not invent an answer.

---

## Layer 2 — Filesystem provider abstraction

Filesystem discovery is a provider problem, not a single hard-coded recursive walker.

```text
FilesystemProvider
├── capabilities()
├── enumerate(root, cursor, page_size)
├── observe(path/id)
├── changes(since_cursor)
├── health()
└── reconcile(...)
```

Initial providers:

```text
EverythingProvider   Windows accelerated provider
NativeProvider       portable fallback / verification provider
```

### EverythingProvider

On Windows, voidtools Everything is a strong optional fast path.

Use the official SDK/IPC rather than scraping GUI output. Everything can cheaply supply large filename/path inventories and, where supported, journal/change data.

Everything remains a **locator/change-feed provider only**. It does not own semantic analysis, user intent, policy, questions, or plans.

Provider state includes:

```text
provider kind/version
volume/root identity
cursor/journal position
last successful reconciliation
capabilities
health/degraded state
```

A provider gap/rebuild/outage triggers reconciliation; it never becomes deletion evidence by itself.

Tracking: Issue #4.

### NativeProvider

The native provider remains essential for:

- portability;
- validation against Everything;
- unsupported filesystems;
- controlled tests;
- fallback when Everything is unavailable.

Reparse/symlink traversal remains fail-closed until bounded/cycle-safe semantics are deliberately implemented.

---

## Layer 3 — Durable observation index

Canonical filesystem/job state lives in a durable control database, currently SQLite.

The index should represent physical observation separately from scan policy.

```text
observation_state:
  present / missing / unknown

policy_state:
  included / hidden / excluded / system / reparse-skipped / protected / ...
```

A skipped file is not the same thing as a deleted file.

Important identity fields may include:

```text
logical path
normalized path key
volume/root identity
platform file identity where available
size
timestamps
entry type
provider provenance
last trustworthy observation
content/fingerprint state
project metadata
analysis state
errors
```

The current `PersonalFileIndex` is the Phase 1 foundation for this layer.

---

## Layer 4 — Durable job engine

Long operations must not exist only as an in-memory call stack.

Persist at minimum:

```text
jobs
job_phases
tasks
attempts
checkpoints
questions
answers
provider_cursors
worker_versions
resource leases
errors/events
```

Example job pipeline:

```text
discover
→ observe metadata
→ fingerprint candidates
→ extract deterministic content
→ OCR where required
→ metadata enrich
→ search-index update
→ embeddings where enabled
→ semantic classification
→ relationships
→ planning
```

Each task should be restart-safe. A process crash should cause unfinished leases/tasks to become retryable rather than restart the entire 8TB job.

### Pause/resume

`pause` means stop taking new work and persist a clean checkpoint. `resume` means continue from persisted tasks/cursors, not restart traversal from zero.

This distinction is important: persistent rescanning is not the same as checkpoint resume.

---

## Layer 5 — Resource-aware scheduler

The scheduler controls work by resource class:

```text
filesystem metadata I/O
sequential content I/O
random hashing I/O
CPU extraction
GPU OCR
GPU/CPU LLM
network/cloud API
```

Concurrency must be device-aware. An HDD should not be hit with the same random hashing concurrency as an NVMe SSD.

Potentially use Taskflow for in-process task DAG execution, but durable tasks/checkpoints remain ours.

Expose user controls such as:

```text
--jobs
--io-jobs
--cpu-jobs
--gpu-jobs
--read-rate
--cloud-concurrency
--pause-on-battery
```

Defaults should be adaptive and conservative.

---

## Layer 6 — Content identity and cache

Expensive work is keyed to content identity, extractor/model version, and settings.

A staged strategy avoids full hashing every file during discovery:

```text
stable file identity + size + mtime
→ cheap/sampled fingerprint when required
→ BLAKE3 full content hash when justified
→ optional SHA-256 for interoperability/audit
```

If a file changes while being processed, discard/requeue results rather than attaching analysis to the wrong version.

A content-addressed cache lets renamed/moved identical files reuse extraction/OCR/embedding work.

---

## Layer 7 — Worker/plugin boundary

Large or language-specific dependencies should not all be linked into the main C++ binary.

Workers can be external processes with a versioned JSON/JSONL protocol or another narrow IPC contract.

```text
WorkerManifest
├── id
├── version
├── capabilities
├── supported formats/languages
├── resource requirements
└── protocol version
```

The host provides:

- bounded inputs;
- timeout/cancellation;
- stderr/log capture;
- retries;
- version/provenance recording;
- health checks;
- sandboxing/resource restrictions where practical.

This lets Python OCR, Rust search tooling, ExifTool, and other mature utilities participate without turning the main executable into an unmaintainable dependency bundle.

See `PERSONAL-ORGANIZER-DEPENDENCY-STRATEGY.md`.

---

## Layer 8 — Deterministic extraction and metadata

Cheap/deterministic extraction comes before LLM work.

Potential workers/libraries:

```text
upstream DocumentTextAnalyzer
PDFium
ExifTool
MediaInfo
libarchive
file/MIME detection
```

Output should carry provenance:

```text
field/value
source worker
worker version
source page/tag/path
confidence if relevant
```

Do not send every file to an LLM merely to learn metadata a parser can read exactly.

---

## Layer 9 — OCR

OCR is selective and cacheable.

Priority languages:

- Arabic;
- Urdu;
- English;
- mixed-language pages.

Candidate engines/workers:

```text
PaddleOCR      leading benchmark candidate
OCRmyPDF       PDF OCR orchestration
Tesseract      compatibility/lightweight fallback
Surya          OCR/layout benchmark candidate
```

Pipeline:

```text
measure usable text layer
→ identify whether OCR is needed
→ select strategic pages first
→ OCR
→ normalize Unicode
→ record page confidence/layout
→ escalate to more pages only if required
```

Full-document OCR should be an explicit consequence of need, not the default cost for every scanned book.

---

## Layer 10 — Search architecture

Search is layered rather than a single model query.

### Name/path search

EverythingProvider on Windows can provide extremely fast name/path lookup.

### Durable metadata search

SQLite handles structured filters and canonical state.

### Full-text search

Benchmark SQLite FTS5 vs Tantivy for extracted text/OCR at real corpus scale. Tantivy is a strong candidate for a rebuildable derived index.

Arabic/Urdu behavior must be tested explicitly rather than assuming English tokenization quality transfers.

### Semantic/vector search

USearch is a strong future C++-native candidate.

Vector search supplements exact/lexical search. It does not replace filenames, metadata, citations, or literal text search.

---

## Layer 11 — Model provider/router

Offline inference is first-class.

Provider classes may include:

```text
BundledLlamaCppProvider
OllamaProvider
LMStudioProvider
CustomOpenAICompatibleProvider
OpenAIProvider
GeminiProvider
other explicitly configured providers
```

Routing considers:

```text
capability
quality requirement
privacy policy
path sensitivity
local hardware
latency
cloud budget
context size
confidence from cheaper stages
```

Example:

```text
simple metadata/classification → deterministic/local small model
difficult Arabic document      → stronger local model
uncertain non-sensitive item   → optional cloud escalation
local-only path                → cloud forbidden
```

Every derived AI result records provider/model/version/configuration provenance.

---

## Layer 12 — Projects and structural protection

Read access and mutation authority are separate.

A Git/Node/Flutter/design project may be deeply indexed and searchable while generic organization is forbidden from rearranging its internal files.

Protection is a planner/apply constraint, not a reason to make useful source files invisible.

---

## Layer 13 — Personal policy

`ORGANIZE.md` or an equivalent declarative policy layer defines stable behavior without recompiling C++.

Possible rules:

```text
allowed roots/destinations
Arabic/English naming rules
protected paths
project behavior
archive behavior
local-only/cloud-ok paths
confidence thresholds
never-move patterns
always-review patterns
duplicate policy
bibliographic naming policy
```

Policy is parsed into structured constraints, not pasted into an opaque prompt.

---

## Layer 14 — Relationships and collections

Files can belong to higher-level collections:

```text
book + notes + annotations
source scan + OCR derivative
original + translation
paper + supplements
PSD/AI source + exports
video project + assets + render
repo + docs
course bundle
multi-volume work
```

Planning on individual files before detecting these relationships produces bad organization decisions.

---

## Layer 15 — Duplicate engine

Exact duplicate detection is deterministic and staged.

```text
size
→ cached/sampled fingerprint
→ full content hash only for candidates
→ exact duplicate group
```

fclones is a benchmark/reference and possible read-only accelerator because it is optimized for large duplicate workloads. The organizer should still reuse its own existing hashes rather than reread terabytes unnecessarily.

No duplicate tool's destructive mode is invoked automatically.

---

## Layer 16 — Bibliographic/document intelligence

Structured evidence can include:

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

Evidence priority:

```text
trusted embedded metadata
→ title/publication page
→ embedded text
→ OCR
→ existing path/name context
→ model inference only if necessary
```

Each field should retain evidence/provenance. Missing bibliographic facts are not fabricated merely to produce a cleaner filename.

---

## Layer 17 — Planning engine

The planner converts knowledge + requirements + policy into a serializable proposal.

```text
OrganizationPlan
├── source identity/path
├── proposed action
├── destination/name
├── explicit requirements used
├── policy rules used
├── evidence
├── model/provider provenance
├── confidence
├── relationships
├── conflicts
└── warnings/questions
```

The planner does not execute operations.

---

## Layer 18 — Validation, review, apply, audit

Apply is deterministic and conservative.

Before mutation, validate:

- source identity still matches;
- destination conflicts;
- protected/project constraints;
- plan freshness;
- required questions answered;
- path/permission rules;
- duplicate certainty;
- expected disk/volume state.

Audit records original/final identity/path and enough information to explain and, where technically possible, undo the change.

Reuse upstream review/apply/undo infrastructure where its contracts remain correct.

---

## Layer 19 — CLI architecture

The CLI is both a human tool and a stable machine API.

Long-term command families may include:

```text
aifs doctor
aifs provider ...
aifs index ...
aifs job status|pause|resume|cancel|tail ...
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
aifs shell
```

Requirements:

- human-readable default output;
- `--json` / `--jsonl` machine modes;
- stable exit codes;
- deterministic schemas;
- quiet/verbose/debug modes;
- no ANSI/progress noise when piped;
- shell completion;
- UTF-8/Unicode-safe paths;
- `--offline` and privacy controls;
- `--dry-run`/plan boundaries for mutation;
- `--explain` / `--why` where inference is involved.

CLI11 is a strong candidate for the expanding parser surface.

An optional future `aifs shell` can use a proper UTF-8 REPL library and maintain conversational context without weakening the underlying structured `JobSpec`/policy model.

---

## Layer 20 — GUI

The GUI is a secondary presentation client over the same services.

Potential views:

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

The GUI must not contain unique organization logic that cannot be reproduced by the CLI/core.

---

## Layer 21 — Agent/MCP

Agents call constrained services, not raw filesystem authority.

Safe capabilities can include:

```text
query providers/index
start analysis job
inspect status
answer a persisted question
search
build/inspect plan
request validated apply
```

Agent calls use the same policy, privacy, locks, questions, plans, and audit path as human CLI calls.

---

## Privacy architecture

Whole-PC indexing can expose sensitive paths/content.

Policy can mark scopes such as:

```text
local-only
ask-before-cloud
cloud-allowed
metadata-only-cloud
```

Remote calls should carry explicit provenance and never happen merely because a local model is slow.

The local index/search databases themselves are sensitive and should use user-private storage/permissions.

---

## Testing architecture

Tests must include:

- deterministic unit tests;
- temporary filesystem + SQLite integration tests;
- Windows Unicode/path/file-identity tests;
- provider parity/reconciliation tests;
- provider outage/journal-gap tests;
- schema migration + backup tests;
- crash/restart/checkpoint-resume tests;
- policy-change tests;
- Arabic/Urdu extraction/OCR/search fixtures;
- multi-million-entry synthetic scale benchmarks;
- long-session soak tests;
- per-device scheduling tests;
- plugin timeout/crash tests;
- CLI JSON/exit-code contract tests;
- plan/apply tests only on disposable fixtures;
- before/after source hashing for mutation-proof read-only phases.

A green focused test is not equivalent to a production binary test. Windows CI must eventually compile/link the real native application and invoke the actual packaged launcher.

---

## Storage strategy

Use SQLite as the durable control plane first.

SQLite stores canonical metadata/job/policy/question/provenance/audit state with WAL, bounded transactions, migrations, backups, and integrity checks.

Do not force every derived index into the same database:

```text
SQLite                canonical control state
Tantivy/FTS5          rebuildable full-text index
USearch               rebuildable vector index
content cache         extracted/OCR/chunk artifacts by identity
```

RocksDB or another storage engine is considered only if measured SQLite limits justify the operational complexity.

---

## Relationship with upstream

Upstream remains valuable for:

- Qt UI;
- existing document extraction;
- image analysis;
- `llama.cpp` integration;
- remote OpenAI/Gemini/custom endpoints;
- protected project detection;
- review/apply/undo;
- settings and SQLite infrastructure.

I reuse upstream services when their contracts fit, but the fork's CLI-first provider/job architecture should remain modular so upstream synchronization stays manageable.

---

## Documentation and backlog

```text
PERSONAL-ORGANIZER.md
PERSONAL-ORGANIZER-ROADMAP.md
PERSONAL-ORGANIZER-ARCHITECTURE.md
PERSONAL-ORGANIZER-DEPENDENCY-STRATEGY.md
PERSONAL-ORGANIZER-CLI.md
PERSONAL-ORGANIZER-DEVELOPMENT-LOG.md
AGENTS.md
Issues
Pull requests
```

Current architecture tracking:

- #2 — Phase 1 persistent filesystem state;
- #3 — CLI-first/offline-first/multi-terabyte architecture epic;
- #4 — EverythingProvider;
- #5 — dependency/delegation strategy.

Documentation changes with implementation; planned behavior must remain labelled as planned.
