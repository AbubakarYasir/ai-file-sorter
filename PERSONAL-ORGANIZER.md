# Personal AI File Organizer

This fork extends [hyperfield/ai-file-sorter](https://github.com/hyperfield/ai-file-sorter) into a GUI-first personal knowledge and filesystem organizer with a more powerful CLI/headless surface.

The project is intentionally built on top of the upstream application rather than as a separate rewrite. Upstream remains the source for the existing Qt GUI, document/image analysis, categorization, review/apply flow, undo history, local/remote LLM support, and project protection.

## Product direction

```text
                         Shared core services
                    indexing / OCR / planning / policy
                               /       \
                              /         \
                             v           v
                           GUI           CLI
                       everyday use   power-user surface
                              \         /
                               \       /
                                v     v
                              Agent/MCP
                              automation
```

The GUI is the primary everyday experience. The CLI is intended to be a functional superset for automation, diagnostics, advanced indexing, batch OCR, policy validation, machine-readable output, and future agent integrations.

## Safety model

The organizer follows an analyze-first workflow:

```text
scan/index
   ↓
understand
   ↓
plan
   ↓
review
   ↓
apply
   ↓
audit / undo
```

Indexing must never rename, move, delete, or edit scanned source files. Filesystem mutations belong to a later explicit apply stage.

## Branch model

- `main` — kept close to upstream Hyperfield.
- `personal-organizer` — stable integration branch for this fork.
- `feature/*` — isolated feature branches reviewed before integration.

Current work:

- `feature/indexer` → Phase 1 read-only persistent filesystem index.
- Draft PR #1 → `feature/indexer` into `personal-organizer`.

## Documentation

- [`docs/PERSONAL-ORGANIZER-ROADMAP.md`](docs/PERSONAL-ORGANIZER-ROADMAP.md) — long-term roadmap and implementation order.
- [`docs/PERSONAL-ORGANIZER-ARCHITECTURE.md`](docs/PERSONAL-ORGANIZER-ARCHITECTURE.md) — system boundaries and component design.
- [`docs/PERSONAL-ORGANIZER-CLI.md`](docs/PERSONAL-ORGANIZER-CLI.md) — CLI design and safety levels.
- [`docs/PERSONAL-ORGANIZER-DEVELOPMENT-LOG.md`](docs/PERSONAL-ORGANIZER-DEVELOPMENT-LOG.md) — fork-specific commits, decisions, tests, and known issues.

## Current milestone

The first milestone is deliberately not automatic sorting:

> Scan selected filesystem roots, persist a trustworthy inventory, understand as much as possible, and later generate a reviewable organization plan without moving a single source file.

The first implementation creates a dedicated SQLite inventory (`personal_file_index.db`) with scan-run history, metadata fields, optional SHA-256 hashing, protected-project recognition, and placeholders for later document/OCR/image analysis.

## Upstream policy

Fork-specific behavior should be implemented in new, modular services wherever practical. Changes to upstream-heavy files should be kept small so `upstream/main` can continue to be merged with minimal conflict.
