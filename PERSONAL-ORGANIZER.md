# Personal AI File Organizer

This repository is my personal fork of [`hyperfield/ai-file-sorter`](https://github.com/hyperfield/ai-file-sorter). I am extending the upstream application into a long-term, AI-assisted filesystem and knowledge organizer for my own Windows PC and workflows.

I am not trying to build a one-click “AI cleans your Downloads folder” toy. I want one system that can understand what my files actually are, preserve the relationships between them, propose a stable structure, and let me review every meaningful change before it touches the filesystem.

## Why I am building this

My files are mixed across very different kinds of work: research and books, Arabic and Urdu PDFs, scanned documents, design source files and exports, software repositories, course material, references, media, personal files, and temporary downloads. A filename-only sorter cannot organize this reliably.

The organizer therefore needs to understand content and context, not only extensions and filenames. It should eventually know the difference between a reference book and an active research source, between a design source and its export, between a Git repository and loose code, and between a searchable PDF and an image-only scan that needs OCR.

My target is a system that behaves more like a local file intelligence layer than a conventional folder sorter.

## Product direction

The application remains **GUI-first for normal use**, while the **CLI/headless interface is the power-user superset**.

```text
                         Shared core
       index / content extraction / OCR / policy / planning
                    /              |              \
                   v               v               v
                 GUI              CLI          Agent/MCP
            everyday use      full controls     automation
```

I want the GUI for browsing, search, review, previews, rules, duplicates, history, and normal organization. I want the CLI to expose deeper controls for scripting, diagnostics, batch operations, machine-readable output, experimental switches, and future agent integrations.

Both interfaces must call the same underlying services. Features should not be reimplemented separately for GUI and CLI.

## Core rule: understand first, mutate later

The system is designed around this pipeline:

```text
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

Indexing, OCR, classification, relationship detection, and planning are read-only with respect to source files. Moving, renaming, merging, archiving, or deleting files belongs to an explicit later apply stage.

Low confidence is not permission to guess. Uncertain items should remain unchanged and be surfaced for review.

## Important organization rules

The long-term organizer will use a stable personal taxonomy instead of inventing new folders on every run. My intended rules include:

- religious/Islamic material uses Arabic naming where appropriate;
- non-religious categories and filenames use consistent English naming;
- bibliographic titles should preserve their real language and spelling;
- development repositories are treated as structural units and must not be casually rearranged internally;
- project-owned assets stay with their project when ownership is more meaningful than file type;
- exact duplicates are detected before any deletion is considered;
- ambiguous incoming files can remain in an Inbox/review state instead of being force-classified;
- archive is preferred over destructive cleanup during early versions;
- every applied organization plan should be auditable and reversible where technically possible.

## Relationship with upstream

I am building this on top of Hyperfield's existing Qt/C++ application rather than rewriting the project from scratch. Upstream already provides useful foundations including GUI infrastructure, document/image analysis, categorization, review/apply behavior, undo/history, local and remote LLM support, headless integration, and protected-project detection.

Repository roles:

```text
hyperfield/ai-file-sorter
        │
        │ upstream
        v
AbubakarYasir/ai-file-sorter
        │
        ├── main                upstream-friendly branch
        ├── personal-organizer  stable custom integration branch
        └── feature/*           isolated development branches
```

I keep fork-specific work modular wherever practical so I can continue pulling improvements from `upstream/main` without turning every update into a large merge conflict.

## Current development status

**Current phase: Phase 1 — read-only persistent filesystem indexing.**

Issue: [#2 — Phase 1: Whole-PC read-only indexing foundation](../../issues/2)

Draft implementation: [PR #1 — Phase 1: add read-only personal file index foundation](../../pull/1)

The current branch adds a dedicated `PersonalFileIndex` backed by SQLite. The index is intentionally separate from the existing categorization cache and is being built to support large, incremental scans and later content intelligence.

Current implementation includes or is actively validating:

- streaming filesystem traversal;
- persistent scan-run history;
- file and directory metadata;
- safe skipping of symlinks/reparse points by default;
- recognition of protected project roots;
- optional SHA-256 hashing rather than mandatory whole-drive hashing;
- fields reserved for later document text, OCR, summaries, language, and image descriptions;
- dedicated integration tests and fork-specific CI.

**Do not treat the current branch as ready for a whole `C:\` scan yet.** Path-aware Windows exclusions, CLI exposure, statistics/query helpers, and test/CI hardening are still Phase 1 work.

## Documentation map

I keep fork documentation separate from upstream documentation so upstream sync remains manageable.

- [`docs/PERSONAL-ORGANIZER-ROADMAP.md`](docs/PERSONAL-ORGANIZER-ROADMAP.md) — what I am building and in what order.
- [`docs/PERSONAL-ORGANIZER-ARCHITECTURE.md`](docs/PERSONAL-ORGANIZER-ARCHITECTURE.md) — technical boundaries and design decisions.
- [`docs/PERSONAL-ORGANIZER-CLI.md`](docs/PERSONAL-ORGANIZER-CLI.md) — intended CLI contract, safety levels, and implemented/planned commands.
- [`docs/PERSONAL-ORGANIZER-DEVELOPMENT-LOG.md`](docs/PERSONAL-ORGANIZER-DEVELOPMENT-LOG.md) — chronological engineering record, decisions, CI results, and blockers.
- [`AGENTS.md`](AGENTS.md) — instructions for coding agents and automated contributors working on this fork.
- [`README.md`](README.md) and the original `docs/*.md` files — upstream AI File Sorter documentation unless explicitly marked otherwise.

## Documentation policy

Documentation is part of the implementation. A feature is not finished merely because code exists.

For meaningful changes I expect the relevant issue/PR and fork documentation to be updated with:

- what changed;
- why it changed;
- safety implications;
- commands or interfaces added/changed;
- tests performed and their result;
- known limitations;
- follow-up work.

Planned behavior must be labelled as planned. Documentation must not present an idea, mock command, or future architecture as already implemented.

## First milestone

The first real milestone is deliberately conservative:

> I can scan selected folders, persist a trustworthy local inventory, understand enough metadata/content to support later intelligence, and inspect the result without moving a single source file.

Once that foundation is trustworthy, OCR, taxonomy, relationship detection, duplicate detection, bibliographic extraction, organization planning, GUI workflows, and continuous Inbox organization can be layered on top of it safely.
