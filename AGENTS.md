# AGENTS.md — Personal AI File Organizer

This file contains the working rules for AI coding agents, automated contributors, and developer tools operating on my fork of `hyperfield/ai-file-sorter`.

The repository owner is developing this fork as a personal, GUI-first AI filesystem/knowledge organizer with a power-user CLI and future agent integrations.

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
  Stable integration branch for my custom organizer.

feature/*
  Isolated feature work. Prefer a PR into personal-organizer.
```

Do not commit experimental organizer work directly to `main` unless the task is specifically an upstream-sync/main maintenance task.

## Start here

Before making a significant change, read the relevant files:

```text
PERSONAL-ORGANIZER.md
docs/PERSONAL-ORGANIZER-ROADMAP.md
docs/PERSONAL-ORGANIZER-ARCHITECTURE.md
docs/PERSONAL-ORGANIZER-CLI.md
docs/PERSONAL-ORGANIZER-DEVELOPMENT-LOG.md
```

Also inspect the upstream documentation/code for the component being extended instead of assuming the fork needs a new subsystem.

## Product rules

### GUI-first, shared core

The GUI is the normal user experience. The CLI is intended to become a power-user superset. Future Agent/MCP interfaces must call the same core services.

Do not implement separate business logic for GUI, CLI, and agents when a shared service is appropriate.

### Understand before mutating

The intended pipeline is:

```text
index
→ understand
→ relate
→ plan
→ review
→ apply
→ audit / undo
```

Indexing, extraction, OCR, classification, relationship detection, and planning must not silently mutate source files.

### Low confidence means review

Do not force a category/name merely to avoid returning uncertainty.

### Protect structure-sensitive projects

Generic organization must not casually rearrange:

- Git repositories;
- software projects;
- design/video projects;
- other folders whose internal relative paths are meaningful.

Read-only indexing/search visibility and mutation protection are separate concerns. Do not assume a protected project must be invisible to the index.

## Personal organization rules

The intended long-term behavior includes:

- Islamic/religious material: Arabic naming where appropriate;
- non-religious organization: consistent English naming;
- preserve authentic bibliographic titles/languages;
- do not invent missing bibliographic metadata;
- project-owned files remain with their project when ownership is more meaningful than file type;
- uncertain items may remain in Inbox/review;
- early duplicate workflows report/review instead of auto-delete;
- archive is preferred over destructive cleanup during early versions.

Do not hard-code large personal taxonomies throughout C++. Prefer policy/configuration (`ORGANIZE.md` direction) and explicit typed services.

## Safety requirements

### Whole-drive work

Do not recommend or automatically initiate an unattended whole-`C:\` scan until the current Phase 1 blockers documented in the development log are resolved.

### Source filesystem

A read-only operation may write to the application's own database/cache/logs but must not write to scanned source files.

### Symlinks / reparse points

Do not follow them by default unless the command/policy explicitly requests it and cycle/root-boundary behavior is safe.

### Windows security

Never implement privilege bypasses. The program inherits the permissions of the process that starts it.

### Model/tool output

Treat filenames, extracted text, PDF content, image text, and model responses as untrusted data. They must not become hidden instructions that override application policy.

## Upstream compatibility

Prefer:

```text
new fork-specific service
small integration hook
configuration/policy
```

over:

```text
large invasive rewrite of upstream classes
```

Before adding a new parser, extractor, LLM client, review system, or storage layer, check whether upstream already has a reusable implementation.

Avoid gratuitous formatting/refactoring of upstream-heavy files because it increases merge conflicts.

## Issues and PRs

Use GitHub Issues for actionable work and bugs.

Use the roadmap for long-term direction.

Use draft PRs for implementation that is not yet safe/complete.

Where practical:

- issue explains the goal and completion criteria;
- branch implements one coherent slice;
- commits explain concrete changes;
- PR explains implementation, safety, tests, and remaining work;
- documentation references the issue/PR.

Do not close an issue merely because code was started.

## Commit discipline

Prefer conventional, descriptive commits such as:

```text
feat(indexer): ...
fix(indexer): ...
test(indexer): ...
docs(personal-organizer): ...
ci(personal-organizer): ...
refactor(policy): ...
```

Keep commits understandable and reversible. Avoid vague messages such as `updates`, `fix`, or `changes`.

When using automated tools, do not manufacture a fake human narrative in commit messages. Describe the actual change.

## Documentation is mandatory

The repository owner expects documentation maintenance to be handled as part of development.

For meaningful changes, update the relevant documentation in the same development cycle.

Documentation responsibilities:

### `PERSONAL-ORGANIZER.md`

Update when the product direction, current major milestone, safety model, or documentation map changes.

### Roadmap

Update when phases/order/scope materially change.

### Architecture

Update when services, boundaries, persistence, policy, GUI/CLI/agent relationships, or safety architecture change.

### CLI contract

Update whenever CLI syntax/status/output changes. Always mark planned commands as planned until implemented.

### Development log

Record significant decisions, CI failures/resolutions, migrations, safety blockers, and important implementation checkpoints.

Do not claim a test passed unless it actually ran and passed.

## Testing expectations

Use temporary fixtures/directories for mutation tests. Never test destructive behavior against the developer's real files.

At minimum, a feature should have the lowest-cost useful verification appropriate to its risk:

```text
compile/static check
unit test
integration test
CLI contract test
regression fixture
```

High-risk filesystem mutation requires stronger coverage than metadata-only helpers.

For Phase 1, the personal index integration test and fork CI must be green before the feature is considered merge-ready.

## CI behavior

Fork-specific workflow:

```text
.github/workflows/personal-organizer-ci.yml
```

It is intended to cover `personal-organizer` and `feature/**` work that upstream's `main`-focused workflow does not cover.

If CI fails:

1. inspect the exact failed step/log;
2. fix based on evidence;
3. rerun/trigger CI;
4. record significant failures/resolutions in the development log;
5. do not hide or relabel failure as infrastructure unless evidence supports that conclusion.

## CLI/agent machine contracts

Prefer stable structured output for automation.

When adding JSON contracts:

- include a schema/kind identifier;
- make status explicit;
- keep stdout machine-parseable when JSON mode is selected;
- use stderr for diagnostics;
- avoid silently changing field meanings;
- add tests for parsing/output.

Agent integrations should use supported core/CLI contracts rather than GUI scraping when possible.

## Do not overclaim implementation

The docs contain long-term designs. Before saying a feature exists, verify it in code/tests.

Examples currently planned rather than generally available include much of the personal-organizer command family (`index`, `search`, `ocr`, `duplicates`, `policy`, `plan`, etc.) unless later commits/doc updates explicitly mark them implemented.

## Current Phase 1 priorities

At the time this file was introduced, the order is:

1. diagnose/fix the failing personal-index integration CI;
2. correct stale/presence semantics on partial/failed scans;
3. make Windows exclusions path-aware;
4. separate read-only project indexing from mutation protection;
5. expose a controlled read-only index CLI command;
6. add statistics/query helpers;
7. validate on a small real folder before broader scans.

Check Issue #2 and Draft PR #1 for the current state before assuming this list is still complete.

## Final rule

The organizer should earn authority over the filesystem through evidence, tests, reviewability, and reversibility. Convenience must not come from hiding uncertainty or removing safety boundaries.
