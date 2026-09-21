# Personal AI File Organizer

This repository is my personal fork of [`hyperfield/ai-file-sorter`](https://github.com/hyperfield/ai-file-sorter). I am turning the upstream project into a serious **CLI-first, offline-first filesystem intelligence and organization system** for large Windows filesystems and mixed personal knowledge/work data.

I am not trying to build a one-click “AI cleans Downloads” toy. I want a systems tool that can run for hours or days, survive interruption, understand millions of files across multi-terabyte drives, ask me targeted questions when my requirements are ambiguous, explain what it believes I asked for, estimate the work/cost before doing it, and give me precise control before it changes anything.

## Product direction

The **CLI is the canonical product surface**.

```text
                              Shared core
 providers / durable jobs / extraction / OCR / search / models / policy / planning
                    /                 |                    \
                   v                  v                     v
                 CLI                 GUI                Agent/MCP
              PRIMARY             secondary             automation
```

If an important capability cannot be invoked safely from the CLI, I do not consider it fully implemented in this fork.

The GUI remains useful for browsing, previews, visual review, search, rules, duplicates, and history, but it must be a client of the same services rather than the place where core behavior lives.

## What “amazing” means for this project

The finished system should feel like a trustworthy local filesystem intelligence platform, not a batch script with an LLM attached.

It should:

- discover millions of files quickly;
- become useful before deep analysis is complete;
- deepen understanding progressively instead of blindly OCRing/hashing/embedding everything;
- continue long jobs after terminal closure, crash, or reboot through durable checkpoints;
- understand exactly which provider/worker/model produced each derived fact;
- distinguish physical observation from policy, inference, user intent, plan state, and audit state;
- ask only questions that materially affect scope, privacy, cost, or filesystem outcome;
- estimate expensive work before launching it;
- allow resource budgets so an 8TB scan does not make the PC unusable;
- work well offline and use cloud only when explicitly allowed and useful;
- simulate plans before apply;
- explain why a file matched a search result, why a plan proposes an action, and what evidence supports inferred metadata;
- degrade safely when drives, providers, workers, or models fail.

The detailed acceptance standard lives in [`docs/PERSONAL-ORGANIZER-SYSTEM-QUALITY-BAR.md`](docs/PERSONAL-ORGANIZER-SYSTEM-QUALITY-BAR.md).

## Scale target

The design target is **multi-million-file, 8TB+ storage**, not a Downloads-folder benchmark.

That means:

- never requiring the complete filesystem or extracted corpus to fit in RAM;
- streaming/paged discovery;
- bounded queues and backpressure;
- durable jobs and checkpoints;
- restart-safe, idempotent workers;
- per-volume I/O scheduling;
- explicit pause/resume/cancel/status controls;
- caching so unchanged terabytes are not reread;
- provider outages/inaccessible drives must never look like mass deletion;
- derived search/vector indexes can be rebuilt without losing canonical job/file state;
- benchmark and fault-injection gates at million-entry scale before claiming scalability.

## Progressive intelligence

A whole-drive inventory is not the same thing as a whole-drive deep analysis.

The preferred pipeline is:

```text
locate/discover
→ physical metadata
→ deterministic metadata extraction
→ cheap content extraction
→ selective OCR
→ lexical/full-text indexing
→ relationships/bibliography
→ embeddings when useful
→ LLM reasoning only where useful
```

The tool should answer the user's current need with the cheapest trustworthy stage available. Expensive stages are demand-driven, cacheable, schedulable, and resumable.

## Offline first, cloud optional

Offline operation is the baseline, not a degraded fallback.

Local capabilities should include filesystem discovery/indexing, metadata/document extraction, OCR, search, duplicate detection, local embeddings where enabled, and local LLM/vision inference.

Online providers such as OpenAI, Gemini, Anthropic/OpenRouter-style or custom OpenAI-compatible endpoints can be optional escalations for difficult tasks. Privacy policy decides what is ever allowed to leave the machine.

Privacy is scope-aware rather than one global switch. A path/task may be:

```text
local-only
ask-before-cloud
cloud-allowed
metadata-only-cloud
```

## Intelligent requirements, not blind prompts

Free-form requests are not executed directly.

```text
user request / flags / ORGANIZE.md / profile
        ↓
Requirement + Intent Compiler
        ↓
explicit constraints + inherited policy/defaults + inferred assumptions + unresolved questions
        ↓
targeted questions when ambiguity materially changes the result
        ↓
immutable JobSpec
        ↓
analysis / planning
        ↓
reviewable plan
```

The system must distinguish what I explicitly said, what policy requires, what a saved profile supplied, what was inferred, how confident that inference is, and what still needs my answer.

An LLM never gets direct permission to improvise filesystem mutations.

Tracking: [#7](../../issues/7).

## Long-running work should outlive the shell

For serious jobs, the CLI should eventually become a client of a durable organizer runtime/service.

```text
shell / PowerShell
       ↓
      aifs
       ↓
durable job runtime
       ↓
providers / workers / models / indexes
```

Long jobs persist tasks, attempts, leases, checkpoints, questions, answers, events, and provider cursors. Closing a terminal must not discard hours of completed work.

Tracking: [#6](../../issues/6).

## Resource budgets and estimates

Before launching expensive work, the system should estimate what it can reasonably know:

```text
entries affected
bytes likely to read
files likely to hash
pages likely to OCR
cache growth
local compute/GPU work
cloud tokens/cost when enabled
```

Users should eventually be able to set budgets such as maximum read volume, OCR pages, cloud cost/tokens, runtime, and cache growth. Hitting a budget should checkpoint cleanly rather than silently exceeding it.

## Core rule: understand first, mutate later

```text
locate
  ↓
observe/index
  ↓
extract/OCR
  ↓
search/understand
  ↓
relate
  ↓
plan
  ↓
validate/simulate
  ↓
questions/review
  ↓
apply
  ↓
audit / undo
```

Indexing, OCR, classification, relationship detection, search indexing, and planning are read-only with respect to source files. Moving, renaming, merging, archiving, linking, or deleting belongs to an explicit later apply stage.

Low confidence means review or a question, not permission to guess.

## Snapshots and reconciliation

At multi-terabyte scale, “not seen this scan” is not a sufficient deletion model.

The long-term observation layer uses trustworthy epochs/snapshots with root/volume identity, provider version/cursor, completeness, errors, and coverage. Everything/native disagreement should become a reconciliation task rather than mass deletion.

Useful future surfaces include:

```powershell
aifs snapshot create D:\
aifs snapshot list
aifs diff snapshot:123 snapshot:124
aifs reconcile D:\ --provider everything --against native
```

Tracking: [#8](../../issues/8).

## Delegate solved infrastructure

I do not want this fork to rebuild mature tools merely for the sake of owning every line of code.

Current integration direction includes:

- **voidtools Everything** as an optional Windows filesystem discovery/change-feed accelerator;
- **ExifTool** for broad metadata extraction;
- existing **MediaInfo** support for media metadata;
- **libarchive** for archive inspection;
- **OCRmyPDF** for PDF OCR orchestration where useful;
- **PaddleOCR** as a leading Arabic/Urdu/English offline OCR candidate;
- **Tantivy** as a large full-text search candidate;
- **USearch** as a local vector-search candidate;
- **BLAKE3** for fast fingerprints/content cache identity;
- **CLI11** for the growing CLI contract;
- selected terminal/REPL libraries for human CLI UX.

External tools are providers/workers. The Personal Organizer still owns requirements, durable state, policy, privacy, provenance, questions, planning, safety, audit, and mutation authority.

See [`docs/PERSONAL-ORGANIZER-DEPENDENCY-STRATEGY.md`](docs/PERSONAL-ORGANIZER-DEPENDENCY-STRATEGY.md).

## Windows indexing direction

Everything is especially valuable because repeated recursive traversal is the wrong primitive for an 8TB Windows filesystem when an index/change journal can tell us what changed.

```text
FilesystemProvider
├── EverythingProvider   Windows accelerated path
└── NativeProvider       portable fallback / verification path
```

Everything does **not** become the semantic database. It is a locator/change-feed provider. The organizer stores its own observation/policy states, file identities, analysis provenance, questions, and plans.

Tracking: [#4](../../issues/4).

## Filesystem edge cases are product requirements

A serious Windows systems tool needs deliberate behavior for more than ordinary files:

- stable volume and platform file IDs where available;
- hardlinks vs duplicate bytes;
- long paths and Unicode;
- case-only collisions;
- junctions/reparse points/mount points;
- sparse/compressed/encrypted files;
- removable/offline volumes;
- cloud placeholders that should not be accidentally hydrated;
- permission failures and partial visibility.

These are tested as correctness cases, not treated as rare afterthoughts.

## Important organization rules

The long-term organizer uses a stable personal taxonomy instead of inventing folders on every run.

Important rules include:

- religious/Islamic material uses Arabic naming where appropriate;
- non-religious categories and filenames use consistent English naming;
- bibliographic titles preserve their real language/spelling;
- development repositories are structure-sensitive and are not casually rearranged internally;
- project-owned assets stay with their project when ownership matters more than file type;
- exact duplicates are proven before deletion is considered;
- derivatives/editions/translations/exports are relationships, not automatically duplicates;
- ambiguous files can remain in Inbox/review instead of being force-classified;
- archive is preferred over destructive cleanup in early versions;
- every applied organization plan is auditable and reversible where technically possible.

## Explainability and provenance

Search hits, inferred metadata, relationships, and plan decisions should retain enough provenance to answer `why`.

Examples of evidence include:

```text
embedded metadata
PDF title page
OCR page/box
exact extracted text
existing path context
relationship evidence
provider/worker/model/version
policy rule
JobSpec requirement/answer
```

This makes explainability operational rather than cosmetic.

## Plans are artifacts, not ephemeral model output

A plan should be serializable, versioned, and tied to the source state, JobSpec, policy version, answers, evidence, and model/worker provenance that produced it.

Before apply, run deterministic validation plus a virtual-filesystem simulation for collisions, invalid names, path-length/case issues, protected structures, changed sources, insufficient space, circular moves, and cross-volume operations.

Apply must not silently regenerate an approved plan using fresh model output.

## Relationship with upstream

I am building on Hyperfield's Qt/C++ application rather than discarding useful upstream work. Upstream already provides document/image analysis, local `llama.cpp`, remote model support, review/apply, undo/history, protected-project detection, GUI infrastructure, headless behavior, settings, and SQLite persistence.

```text
hyperfield/ai-file-sorter
        │ upstream
        v
AbubakarYasir/ai-file-sorter
        ├── main                upstream-friendly
        ├── personal-organizer  stable custom integration
        └── feature/*           isolated development
```

Fork-specific work should remain modular so upstream improvements can continue to be merged deliberately.

## Current development status

**Current implementation work: Phase 1 — trustworthy persistent filesystem state and CLI indexing.**

- Phase 1 index foundation: [#2](../../issues/2)
- Architecture epic: [#3](../../issues/3)
- Everything provider: [#4](../../issues/4)
- Dependency/delegation audit: [#5](../../issues/5)
- Durable runtime/scheduler: [#6](../../issues/6)
- Intent/questions/policy/explainability: [#7](../../issues/7)
- 8TB quality/benchmarks/fault injection: [#8](../../issues/8)
- Draft implementation: [PR #1](../../pull/1)

The branch already contains a dedicated `PersonalFileIndex` backed by SQLite, explicit observation-vs-policy state, schema migration work, Unicode Windows handling, safe reparse behavior, protected-project awareness, machine-readable CLI work, and Linux/Windows regression CI.

It is still deliberately **not** approved for an unattended whole `C:\` run. A real native Windows production build/launcher smoke test and controlled copied-folder validation remain before Phase 1 is considered trustworthy.

## Documentation map

- [`docs/PERSONAL-ORGANIZER-ROADMAP.md`](docs/PERSONAL-ORGANIZER-ROADMAP.md) — build order and milestones.
- [`docs/PERSONAL-ORGANIZER-ARCHITECTURE.md`](docs/PERSONAL-ORGANIZER-ARCHITECTURE.md) — technical boundaries.
- [`docs/PERSONAL-ORGANIZER-SYSTEM-QUALITY-BAR.md`](docs/PERSONAL-ORGANIZER-SYSTEM-QUALITY-BAR.md) — what production-quality means for this system.
- [`docs/PERSONAL-ORGANIZER-DEPENDENCY-STRATEGY.md`](docs/PERSONAL-ORGANIZER-DEPENDENCY-STRATEGY.md) — what I build vs delegate.
- [`docs/PERSONAL-ORGANIZER-CLI.md`](docs/PERSONAL-ORGANIZER-CLI.md) — CLI contracts and status.
- [`docs/PERSONAL-ORGANIZER-DEVELOPMENT-LOG.md`](docs/PERSONAL-ORGANIZER-DEVELOPMENT-LOG.md) — chronological engineering record.
- [`AGENTS.md`](AGENTS.md) — rules for coding agents/tools.

Documentation is part of implementation. Planned behavior is labelled as planned; code, issues, PR state, tests, and docs should remain synchronized.

## First major milestone

The first meaningful milestone is not “I can recursively walk a folder.” It is:

> I can run the production CLI against a controlled Windows dataset, persist trustworthy filesystem state, inspect exactly what happened, prove no source mutation occurred, and have a tested path to provider-accelerated incremental refreshes. True checkpoint resume is introduced by the durable job engine rather than being falsely claimed by the Phase 1 scanner.

From there, the project layers durable jobs, provider reconciliation, deterministic extraction, OCR, full-text/semantic search, intent/questions, relationships, policy, planning, simulation, review, and eventually safe whole-PC organization.
