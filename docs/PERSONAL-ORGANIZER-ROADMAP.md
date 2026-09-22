# Personal AI File Organizer — Roadmap

This is the working roadmap for my fork of `hyperfield/ai-file-sorter`.

I am building a **CLI-first, offline-first filesystem intelligence system** that can operate across multi-million-file, 8TB+ storage, run for hours or days without losing progress, understand multilingual content, ask focused questions when requirements are ambiguous, and produce safe explainable organization plans before any mutation.

The GUI remains useful, but it is a secondary client of the same core. The CLI is the canonical human and automation surface.

## Finished-system goals

The finished system should be able to:

- discover enormous Windows filesystems efficiently without repeatedly walking unchanged trees;
- use voidtools Everything as an optional Windows discovery/change-feed accelerator;
- fall back to its own native filesystem provider when Everything is absent or unsuitable;
- survive crashes/reboots through durable jobs/tasks/checkpoints;
- pause, resume, cancel, inspect, and tail long jobs;
- interpret natural-language requirements into an explicit inspectable `JobSpec`;
- distinguish explicit instructions, policy, remembered defaults, and model inference;
- ask targeted questions rather than guessing when ambiguity materially changes the result;
- operate fully offline with local extraction/OCR/search/models as the baseline;
- optionally escalate selected work to configured online LLMs under privacy policy;
- understand PDFs, Office documents, images, media, archives, source projects, and other common file types;
- OCR Arabic, Urdu, English, and mixed documents;
- search filenames, metadata, extracted text/OCR, bibliographic fields, and semantic embeddings;
- preserve authentic bibliographic information with evidence/provenance;
- recognize projects and structural relationships before organization;
- detect exact duplicates without rereading terabytes unnecessarily;
- apply one stable personal taxonomy and naming policy;
- generate a complete serializable plan before mutation;
- expose reasoning, confidence, evidence, model/provider provenance, and unresolved questions;
- validate source identity/state immediately before apply;
- maintain audit and undo information where technically possible;
- expose machine-readable JSON/JSONL contracts for scripting and future agents.

## Scale assumptions

Design for:

```text
8TB+ total storage
millions to tens of millions of filesystem entries
multiple volumes/devices
hours-to-days jobs
mixed SSD/NVMe/HDD storage
large Arabic/Urdu PDF libraries
large project/design/media trees
```

Non-negotiable consequences:

1. never require all paths or extracted content in RAM;
2. page/stream provider output;
3. persist work queues/checkpoints;
4. cache by content/file identity;
5. schedule I/O per device;
6. never turn provider/drive failure into mass deletion;
7. keep derived search/vector indexes rebuildable;
8. benchmark at million-entry scale before calling a subsystem scalable.

## Repository strategy

```text
upstream/main
    ↓
origin/main
    ↓
personal-organizer
    ↓
feature/*
```

- `upstream` = `hyperfield/ai-file-sorter`;
- `origin` = `AbubakarYasir/ai-file-sorter`;
- `main` remains upstream-friendly;
- `personal-organizer` is my stable custom integration branch;
- coherent work happens on `feature/*` and merges through review.

Current implementation branch: `feature/indexer`.

Tracking:

- [#2](../../issues/2) — Phase 1 filesystem state/index foundation;
- [#3](../../issues/3) — CLI-first/offline-first/multi-terabyte architecture epic;
- [#4](../../issues/4) — EverythingProvider;
- [#5](../../issues/5) — dependency/delegation strategy;
- [PR #1](../../pull/1) — current Phase 1 implementation.

## Non-negotiable engineering rules

1. CLI is the canonical product surface.
2. Offline operation is first-class; cloud access is optional and policy controlled.
3. Discovery/analysis/planning do not mutate source files.
4. Natural-language requests compile into explicit structured requirements before execution.
5. High-impact ambiguity becomes a question, not a model guess.
6. A plan exists before filesystem mutation.
7. Low confidence means question/review/unchanged state.
8. Project structure is protected unless a dedicated workflow understands it safely.
9. Islamic/religious organization uses Arabic naming where appropriate; non-religious organization uses consistent English naming.
10. Extracted/model content is untrusted data and cannot override policy.
11. Long jobs are durable and restart-safe.
12. External tools are narrow workers/providers, not authorities over intent or mutation.
13. Model/provider/tool provenance is retained for inferred/derived data.
14. Planned behavior is never documented as implemented.
15. Every significant feature includes tests and documentation appropriate to its risk.

---

## Phase 0 — Safe development foundation

**Status: substantially complete.**

Completed:

- personal fork/local clone/remotes;
- upstream-friendly `main`;
- `personal-organizer` integration branch;
- feature-branch + draft PR workflow;
- fork-specific CI;
- owner documentation and `AGENTS.md`;
- GitHub Issues as backlog.

Ongoing rule: prefer modular fork services and small upstream integration hooks over invasive rewrites.

---

## Phase 1A — Trustworthy filesystem observation and CLI index

**Status: in progress.**

Tracking: [#2](../../issues/2), [PR #1](../../pull/1).

Goal: establish trustworthy persistent filesystem state without mutating source files.

Implemented/being validated:

- dedicated SQLite `personal_file_index.db`;
- streaming native traversal;
- scan-run history;
- metadata + optional SHA-256;
- protected-project awareness;
- useful source indexing inside protected projects;
- generated project internals excluded;
- explicit `observation_state` vs `policy_state`;
- missing/inaccessible-root preservation;
- overlapping-root rejection;
- Windows Unicode command-line/path handling;
- path-aware Windows system-root protections;
- reparse traversal fail-closed;
- schema versioning/migration backup;
- read-only query/statistics facade;
- controlled `index` CLI + JSON result contract;
- Linux + Windows regression tests.

### Important wording correction

Current Phase 1A supports **persistent incremental rescanning**. It does **not** yet provide true checkpoint resume in the middle of a long scan. True pause/resume belongs to the durable job engine below and must not be claimed until implemented.

### Remaining exit gates

- native MSVC/vcpkg production Windows build in CI;
- invoke the actual packaged `aifilesorter.exe index ...` path;
- use a non-Latin/Arabic/Urdu-named root in that smoke test;
- verify resulting SQLite state;
- hash controlled source fixtures before/after to prove no mutation;
- run a small copied real Windows fixture owned by me;
- synchronize issue/PR/docs with final observed behavior.

Do not run an unattended whole `C:\` scan before these gates.

---

## Phase 1B — Filesystem providers and Everything acceleration

**Status: planned; design researched.**

Tracking: [#4](../../issues/4).

Goal: stop treating recursive traversal as the only discovery mechanism.

Architecture:

```text
FilesystemProvider
├── EverythingProvider   Windows fast path
└── NativeProvider       portable fallback / reconciliation
```

EverythingProvider should:

- auto-detect compatible Everything;
- use official SDK/IPC;
- retain Unicode end-to-end;
- stream/page millions of results;
- expose capabilities rather than assuming Everything 1.5 features;
- use 1.5 journal positions/change events when available;
- support safer basic discovery on compatible stable versions;
- persist provider cursor/health;
- detect journal gaps/rebuilds;
- trigger reconciliation instead of inventing deletion;
- never become the semantic/content database.

A private named Everything instance can be researched later, but must not silently change an existing user's Everything setup.

---

## Phase 2 — Durable job engine and true pause/resume

**Priority: before large-scale content intelligence.**

Goal: make multi-hour/day jobs operationally trustworthy.

Persist:

```text
jobs
phases
tasks
attempts
checkpoints
questions
answers
provider cursors
worker/model versions
resource leases
events/errors
```

Required behavior:

- `job status`;
- `job pause`;
- `job resume` from checkpoints;
- `job cancel`;
- `job tail`;
- restart after crash/reboot without repeating completed expensive work;
- retry failed tasks without restarting the whole corpus;
- explicit `needs_user` state;
- idempotent task semantics.

---

## Phase 3 — Requirement / Intent Compiler

Goal: make the CLI intelligent without letting a model improvise operational authority.

Inputs:

```text
explicit CLI flags
natural-language request
ORGANIZE.md
stored defaults/profile
answers from this job/session
```

Output: immutable structured `JobSpec`.

The compiler records each requirement's source and confidence.

Examples:

```text
root = D:\Library             source: explicit_cli
cloud = forbidden             source: policy
language = Arabic             source: inferred, 0.96
keep exports with source      source: unresolved_question
```

High-impact ambiguity becomes a persisted question. Independent work can continue while the affected subset waits.

---

## Phase 4 — Resource-aware scheduling

Goal: run fast without abusing disks/GPU/CPU.

Schedule separately:

```text
metadata I/O
sequential content reads
random hashing reads
CPU extraction
GPU OCR
GPU/CPU LLM
cloud/API work
```

Per-device behavior matters: HDD random-I/O concurrency should differ from NVMe.

Potential controls:

```text
--jobs
--io-jobs
--cpu-jobs
--gpu-jobs
--read-rate
--cloud-concurrency
--pause-on-battery
```

Taskflow may help in-process DAG execution; persistent job state remains our responsibility.

---

## Phase 5 — Content identity and reusable analysis cache

Goal: do not reread/reanalyze terabytes unnecessarily.

Possible staged identity:

```text
platform file identity + size + mtime
→ sampled fingerprint when needed
→ BLAKE3 full hash when justified
→ SHA-256 when interoperability/audit requires it
```

Extraction/OCR/chunk/embedding results are keyed by content identity + worker/model version + relevant settings.

A renamed/moved identical file should reuse expensive analysis.

---

## Phase 6 — Worker/plugin protocol and deterministic extraction

Goal: delegate mature technical primitives without turning the main binary into a dependency dump.

Strong candidates:

- upstream `DocumentTextAnalyzer` / PDFium;
- ExifTool;
- MediaInfo;
- libarchive;
- MIME/file-type tools.

External workers use a versioned structured protocol with timeout, cancellation, provenance, logs, and health reporting.

See [`PERSONAL-ORGANIZER-DEPENDENCY-STRATEGY.md`](PERSONAL-ORGANIZER-DEPENDENCY-STRATEGY.md).

---

## Phase 7 — Arabic/Urdu/English OCR

Goal: understand scanned books/documents offline.

Benchmark candidates on real documents:

```text
PaddleOCR      leading multilingual candidate
OCRmyPDF       PDF orchestration
Tesseract      compatibility/fallback
Surya          OCR/layout benchmark candidate
```

Pipeline:

```text
measure text layer
→ strategic pages first
→ OCR only where needed
→ Unicode normalize
→ page/layout confidence
→ escalate to more pages if identification/search needs it
```

Do not blindly OCR every page of every PDF.

---

## Phase 8 — Search stack

Goal: make the machine searchable like a personal knowledge filesystem.

Layers:

```text
filename/path     EverythingProvider on Windows / provider search
structured state SQLite
full text         benchmark FTS5 vs Tantivy
semantic vectors  USearch candidate
```

Arabic/Urdu literal, normalized, diacritic, stemming/tokenization, and mixed-language behavior must be benchmarked explicitly.

Example target:

```powershell
aifs search "the paper where I collected Ibn Hajar's comments on ثعلبة"
```

The result should explain whether a hit came from path, literal text, OCR, metadata, bibliography, or semantic similarity.

---

## Phase 9 — Offline/online model router

Goal: excellent offline intelligence with optional online escalation.

Potential providers:

```text
bundled llama.cpp
Ollama
LM Studio
custom OpenAI-compatible endpoint
OpenAI
Gemini
other explicitly configured providers
```

Routing uses capability, privacy, local hardware, latency, cloud budget, and confidence from cheaper stages.

Path/privacy policy can mark:

```text
local-only
ask-before-cloud
cloud-allowed
metadata-only-cloud
```

Every model-derived record retains provider/model/version provenance.

---

## Phase 10 — Stable personal taxonomy + `ORGANIZE.md`

Goal: predictable organization, not one-off AI folder invention.

Top-level starting direction:

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

Islamic/religious content uses Arabic naming where appropriate; other organization uses consistent English naming.

`ORGANIZE.md` should define structured constraints such as roots/destinations, taxonomy, naming, privacy, protected paths, project behavior, archive behavior, confidence thresholds, never-move/always-review rules, and duplicate policy.

---

## Phase 11 — Relationships and collections

Reason about groups before individual files:

- book + notes + annotations;
- source scan + OCR derivative;
- original + translation;
- multi-volume books;
- paper + supplements;
- source design + exports;
- video project + assets;
- repository + project docs;
- course bundles.

Collections become planner context and review units where appropriate.

---

## Phase 12 — Duplicate engine

Start exact and deterministic:

```text
size
→ cached/sampled fingerprint
→ full hash only for candidates
→ exact duplicate group
```

Benchmark/reference fclones for massive duplicate workloads and per-device strategy. Never invoke its destructive modes automatically.

Near-duplicate image/document logic is later and remains clearly distinct from proven exact identity.

---

## Phase 13 — Bibliographic/document intelligence

Structured fields may include title, author/editor/muhaqqiq, publisher, edition, volume, year, language, subject, identifiers.

Evidence order:

1. trusted embedded metadata;
2. publication/title pages;
3. embedded text;
4. OCR;
5. existing path context;
6. model inference only when necessary.

Every inferred field keeps evidence/confidence/provenance. Missing facts are not fabricated for prettier names.

---

## Phase 14 — Global planner + questions/review

Goal: turn knowledge + policy + requirements into a serializable proposal, not direct changes.

Plan entries include:

```text
source identity/path
proposed action/destination/name
explicit requirements
policy rules
evidence
model/tool provenance
confidence
relationships
conflicts
warnings
unresolved questions
```

The planner never executes operations.

---

## Phase 15 — Apply, audit, undo

Before apply, validate source identity, freshness, conflicts, project/protected constraints, policy, required answers, permissions, duplicate certainty, and destination state.

Reuse upstream review/apply/undo infrastructure where its safety contracts fit.

Audit stores original/final state and enough data to explain and undo where technically possible.

---

## Phase 16 — CLI product quality

CLI quality is not decoration; it is part of the product.

Required direction:

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

Support human output + JSON/JSONL, stable exit codes, no progress/ANSI noise when piped, UTF-8 paths, shell completion, `--offline`, privacy controls, `--dry-run`, `--explain`, and `--why`.

CLI11 is the leading parser candidate as the command tree grows.

---

## Phase 17 — GUI client

The GUI becomes a secondary client over the same jobs/providers/search/planner services.

Useful views:

```text
Overview
Jobs
Index
Search
Questions
Collections
Duplicates
Review
Rules/Policy
History/Undo
Diagnostics
```

No unique organization logic lives only in GUI widgets.

---

## Phase 18 — Agent/MCP

Agents call constrained core operations, not raw filesystem authority.

They may query/search/start jobs/answer questions/build plans/request validated apply through the same policy/privacy/audit path as CLI users.

---

## Phase 19 — Continuous operation

Once initial organization is stable, new items arrive through Downloads/Desktop/scanner/phone/import/export inboxes and are incrementally understood and proposed under the same stable rules.

Everything/change-feed providers can make this nearly continuous without repeatedly rescanning entire drives.

---

## Dependency strategy

See [`PERSONAL-ORGANIZER-DEPENDENCY-STRATEGY.md`](PERSONAL-ORGANIZER-DEPENDENCY-STRATEGY.md).

Guiding rule:

> Build the intelligence, control, provenance, job semantics, policy, questions, planning, and safety that make this system unique; delegate mature low-level primitives to proven tools behind narrow adapters when doing so is safer and faster.

## Immediate implementation order

1. finish Phase 1A production Windows build/launcher smoke gate;
2. controlled copied real-folder validation;
3. merge PR #1 only when those gates pass;
4. introduce `FilesystemProvider` abstraction;
5. build durable JobEngine + true checkpoint pause/resume;
6. implement/benchmark EverythingProvider;
7. add million-entry scale/soak benchmark harness;
8. define worker/plugin protocol and integrate deterministic metadata workers;
9. implement content cache/fingerprints;
10. benchmark extraction/OCR/full-text search components;
11. build requirement/question engine;
12. build model router/privacy policy;
13. build relationships/taxonomy/policy/planner;
14. build apply/audit and secondary GUI/agent clients.

The system earns authority over my filesystem through evidence, persistence, explicit requirements, tests, questions, reviewability, and reversibility—not by hiding uncertainty behind an AI-looking interface.
