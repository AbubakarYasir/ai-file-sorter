# Personal AI File Organizer

This repository is my personal fork of [`hyperfield/ai-file-sorter`](https://github.com/hyperfield/ai-file-sorter). I am turning the upstream project into a serious **CLI-first, offline-first filesystem intelligence and organization system** for large Windows filesystems and mixed personal knowledge/work data.

I am not trying to build a one-click “AI cleans Downloads” toy. I want a system that can run for hours or days, survive interruption, understand millions of files across multi-terabyte drives, ask me targeted questions when my requirements are ambiguous, explain what it believes I asked for, and give me precise control before it changes anything.

## Product direction

The **CLI is the canonical product surface**.

```text
                              Shared core
 filesystem providers / jobs / extraction / OCR / models / search / policy / planning
                    /                 |                    \
                   v                  v                     v
                 CLI                 GUI                Agent/MCP
              PRIMARY             secondary             automation
```

If an important capability cannot be invoked safely from the CLI, I do not consider it fully implemented in this fork.

The GUI remains useful for browsing, previews, visual review, search, rules, duplicates, and history, but it must be a client of the same services rather than the place where core behavior lives.

## Scale target

The design target is **multi-million-file, 8TB+ storage**, not a Downloads-folder benchmark.

That means:

- never requiring the complete filesystem or extracted corpus to fit in RAM;
- streaming/paged discovery;
- durable jobs and checkpoints;
- restart-safe, idempotent workers;
- per-volume I/O scheduling;
- explicit pause/resume/cancel/status controls;
- caching so unchanged terabytes are not reread;
- provider outages/inaccessible drives must never look like mass deletion;
- derived search/vector indexes can be rebuilt without losing canonical job/file state.

## Offline first, cloud optional

Offline operation is the baseline, not a degraded fallback.

Local capabilities should include filesystem discovery/indexing, metadata/document extraction, OCR, search, duplicate detection, local embeddings where enabled, and local LLM/vision inference.

Online providers such as OpenAI, Gemini, Anthropic/OpenRouter-style or custom OpenAI-compatible endpoints can be optional escalations for difficult tasks. Privacy policy decides what is ever allowed to leave the machine.

## Intelligent requirements, not blind prompts

Free-form requests are not executed directly.

```text
user request / flags / ORGANIZE.md
        ↓
Requirement + Intent Compiler
        ↓
explicit constraints + inferred assumptions + unresolved questions
        ↓
targeted questions when ambiguity materially changes the result
        ↓
immutable JobSpec
        ↓
analysis / planning
        ↓
reviewable plan
```

The system must distinguish what I explicitly said, what policy requires, what was inferred, how confident that inference is, and what still needs my answer.

An LLM never gets direct permission to improvise filesystem mutations.

## Core rule: understand first, mutate later

```text
locate
  ↓
index
  ↓
understand
  ↓
relate
  ↓
plan
  ↓
review
  ↓
apply
  ↓
audit / undo
```

Indexing, OCR, classification, relationship detection, search indexing, and planning are read-only with respect to source files. Moving, renaming, merging, archiving, linking, or deleting belongs to an explicit later apply stage.

Low confidence means review or a question, not permission to guess.

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

Everything is especially valuable because repeated recursive traversal is the wrong primitive for an 8TB Windows filesystem when the filesystem/change journal can tell us what changed.

```text
FilesystemProvider
├── EverythingProvider   Windows accelerated path
└── NativeProvider       portable fallback / verification path
```

Everything does **not** become the semantic database. It is a locator/change-feed provider. The organizer stores its own observation/policy states, file identities, analysis provenance, questions, and plans.

Tracking: [#4](../../issues/4).

## Important organization rules

The long-term organizer uses a stable personal taxonomy instead of inventing folders on every run.

Important rules include:

- religious/Islamic material uses Arabic naming where appropriate;
- non-religious categories and filenames use consistent English naming;
- bibliographic titles preserve their real language/spelling;
- development repositories are structure-sensitive and are not casually rearranged internally;
- project-owned assets stay with their project when ownership matters more than file type;
- exact duplicates are proven before deletion is considered;
- ambiguous files can remain in Inbox/review instead of being force-classified;
- archive is preferred over destructive cleanup in early versions;
- every applied organization plan is auditable and reversible where technically possible.

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

- Tracking: [#2](../../issues/2)
- Architecture epic: [#3](../../issues/3)
- Everything provider: [#4](../../issues/4)
- Dependency/delegation audit: [#5](../../issues/5)
- Draft implementation: [PR #1](../../pull/1)

The branch already contains a dedicated `PersonalFileIndex` backed by SQLite, explicit observation-vs-policy state, schema migration work, Unicode Windows handling, safe reparse behavior, protected-project awareness, machine-readable CLI work, and Linux/Windows regression CI.

It is still deliberately **not** approved for an unattended whole `C:\` run. A real native Windows production build/launcher smoke test and controlled copied-folder validation remain before Phase 1 is considered trustworthy.

## Documentation map

- [`docs/PERSONAL-ORGANIZER-ROADMAP.md`](docs/PERSONAL-ORGANIZER-ROADMAP.md) — build order and milestones.
- [`docs/PERSONAL-ORGANIZER-ARCHITECTURE.md`](docs/PERSONAL-ORGANIZER-ARCHITECTURE.md) — technical boundaries.
- [`docs/PERSONAL-ORGANIZER-DEPENDENCY-STRATEGY.md`](docs/PERSONAL-ORGANIZER-DEPENDENCY-STRATEGY.md) — what I build vs delegate.
- [`docs/PERSONAL-ORGANIZER-CLI.md`](docs/PERSONAL-ORGANIZER-CLI.md) — CLI contracts and status.
- [`docs/PERSONAL-ORGANIZER-DEVELOPMENT-LOG.md`](docs/PERSONAL-ORGANIZER-DEVELOPMENT-LOG.md) — chronological engineering record.
- [`AGENTS.md`](AGENTS.md) — rules for coding agents/tools.

Documentation is part of implementation. Planned behavior is labelled as planned; code, issues, PR state, tests, and docs should remain synchronized.

## First major milestone

The first meaningful milestone is not “I can recursively walk a folder.” It is:

> I can run a production CLI job against a controlled Windows dataset, persist trustworthy filesystem state, stop/restart safely, inspect exactly what happened, prove no source mutation occurred, and have a clear path to accelerate future refreshes through providers such as Everything.

From there, the project can layer durable content extraction, OCR, full-text/semantic search, requirements/questions, relationships, policy, planning, review, and eventually safe whole-PC organization.
