# Personal AI File Organizer — Architecture

This document describes how I want my fork to evolve technically. It is not a replacement for upstream `docs/architecture.md`; it documents the fork-specific architecture layered on top of Hyperfield AI File Sorter.

## Architectural goal

I want one shared core that can be used by three surfaces:

```text
                    Personal Organizer Core
         index / extraction / OCR / policy / planning
                    /          |          \
                   v           v           v
                 GUI          CLI       Agent/MCP
```

The GUI is the normal product experience. The CLI exposes a wider power-user surface. Agent/MCP support, if added, must call the same services rather than bypassing policy, review, or audit logic.

The core architectural rule is **analysis first, mutation second**.

```text
Filesystem
   ↓
Read-only index
   ↓
Content intelligence
   ↓
Relationships + taxonomy + policy
   ↓
Organization plan
   ↓
Review
   ↓
Apply
   ↓
Audit / undo
```

No lower layer should quietly move files merely because it has reached a classification decision.

## Relationship with upstream architecture

I am keeping the upstream application as the base rather than creating a parallel application.

Upstream already contains useful components for:

- Qt desktop UI;
- filesystem scanning;
- document text extraction;
- image analysis;
- local `llama.cpp` inference;
- remote OpenAI/Gemini/custom-compatible APIs;
- categorization and naming;
- review/apply workflows;
- undo/history;
- headless execution;
- protected project detection;
- settings and SQLite persistence.

Fork-specific services should reuse these pieces when their contracts fit. I do not want duplicate PDF readers, duplicate LLM clients, or separate GUI/CLI implementations of the same business logic.

## Branch architecture

```text
hyperfield/ai-file-sorter:main
           │
           │ upstream
           v
AbubakarYasir/ai-file-sorter:main
           │
           v
personal-organizer
           │
           ├── feature/indexer
           ├── feature/arabic-ocr
           ├── feature/organize-policy
           └── ...
```

`main` is the upstream-friendly branch. `personal-organizer` is the stable custom integration branch. Feature branches should be short enough to review and should merge into `personal-organizer` through PRs.

## Layer 1 — Persistent file index

### Purpose

The existing categorization database answers a different question from the one I need for a whole-PC organizer. A whole-PC index needs to remember files even before they are categorized and must support later extraction/OCR/search/relationship work.

For that reason Phase 1 introduces a separate `PersonalFileIndex` and a separate SQLite database (`personal_file_index.db`).

### Current schema direction

The index represents entries with fields such as:

```text
full path
parent path
name
extension
entry type
size
timestamps
hash/hash state
scan root
last seen run
present/stale state
project metadata
analysis state
content summary placeholders
errors
```

The schema intentionally reserves content-analysis fields so later phases do not require a second disconnected database.

### Traversal model

Whole-drive scans can contain hundreds of thousands of entries. The indexer should therefore use streaming traversal:

```text
read one entry
→ inspect metadata
→ persist/upsert
→ continue
```

rather than:

```text
collect entire drive into RAM
→ analyze later
```

SQLite writes are batched for performance while the source filesystem remains read-only.

### Incremental model

Each scan has a run ID. Entries record their last-seen run and scan root. A repeat scan can mark previous rows stale and mark currently observed rows present again.

Future optimization should avoid expensive re-analysis when size/timestamps/content identity indicate the file is unchanged.

## Layer 2 — Source safety and traversal policy

### Reparse points and symlinks

They are not followed by default. This prevents cycles and accidental traversal outside the selected root.

### Permissions

Permission failures should become warnings/errors attached to scan results; they should not cause the scanner to pretend the filesystem is complete.

### Windows exclusions

The current first-pass implementation uses directory-name exclusions. Before whole-drive use this must become path-aware because a name such as `AppData` has different meaning depending on location.

The policy should distinguish:

- operating-system internals;
- installed application internals;
- caches/build products;
- user-owned content;
- explicitly included roots.

A whole `C:\` scan is blocked as a recommended workflow until this is validated.

## Layer 3 — Project awareness

Upstream `ProtectedProjectDetector` recognizes structures such as Git, Node.js, Python, Rust, Go, Gradle, .NET, Unity, Unreal, Godot, Xcode, and Blender projects.

There are two different concepts that must not be conflated:

1. **read protection** — whether the indexer traverses and understands a project;
2. **mutation protection** — whether the organizer is allowed to rearrange files inside it.

My intended long-term behavior is generally:

```text
project may be indexed/searchable
but internal structure is protected from generic organization
```

The current Phase 1 implementation initially records strong project roots as protected units. That behavior is being treated as a temporary conservative default, not the final semantic model.

## Layer 4 — Content extraction

Content intelligence should build in stages and cache results.

```text
metadata
→ existing embedded text
→ text quality check
→ OCR if required
→ language identification
→ compact summary
→ subject/type signals
```

Existing upstream `DocumentTextAnalyzer` should remain the primary extractor for supported text-bearing formats.

Content extraction should not immediately invoke an LLM for every file. Cheap deterministic extraction happens first; models are used only where they add information.

## Layer 5 — OCR

OCR is mainly required for image-only PDFs/scans and images containing meaningful text.

The OCR service should eventually expose an interface independent of any specific engine:

```text
OcrService
├── can_handle(...)
├── inspect_text_layer(...)
├── select_pages(...)
├── recognize(...)
└── normalize(...)
```

Requirements:

- Arabic;
- Urdu;
- English;
- mixed-language pages;
- page-level confidence;
- cache by file/page identity;
- avoid full-document OCR when identification can be achieved from strategic pages.

The engine should be selected from real benchmarks, not familiarity alone.

## Layer 6 — Personal policy

Hard-coded C++ should not contain every preference about my folders.

A future `ORGANIZE.md` / policy loader should provide a declarative layer for:

```text
roots
allowed destinations
taxonomy
language/naming rules
protected paths
project behavior
archive behavior
review thresholds
never-move rules
```

Possible components:

```text
PersonalPolicyLoader
TaxonomyGraph
PolicyMatcher
NamingPolicy
ConfidencePolicy
```

Policy should be inspectable and testable; it should not just be inserted as an opaque prompt blob.

## Layer 7 — Relationships and collections

Organization quality improves when related files are identified before individual moves are proposed.

A `RelationshipGrouper` or equivalent service should represent groups such as:

```text
book + annotations
original + translation
source design + exports
video project + assets
repo + project docs
course bundle
multi-volume set
paper + supplements
```

The planner should receive collections as first-class context.

## Layer 8 — Duplicate engine

Exact duplicate detection should remain deterministic:

```text
size bucket
→ SHA-256
→ duplicate group
```

Hashing an entire drive during every index pass is wasteful, so duplicate hashing should be demand-driven or incremental.

Potential components:

```text
DuplicateDetector
DuplicateGroup
DuplicateReviewModel
```

Near-duplicate detection is a later feature and should never weaken certainty around exact duplicates.

## Layer 9 — Bibliographic analysis

Books/research PDFs need more than a category label.

Potential structured record:

```text
BibliographicRecord
├── title
├── author
├── editor / muhaqqiq
├── publisher
├── edition
├── year
├── volume
├── language
├── subject
└── evidence/confidence per field
```

Each field should preserve provenance where possible. A model-generated guess is not equivalent to a title read directly from a publication page.

## Layer 10 — Planning engine

The planner is the boundary between understanding and mutation.

Suggested components:

```text
PlanningEngine
├── inputs
│   ├── ContentIndex
│   ├── TaxonomyGraph
│   ├── Policy
│   ├── Relationships
│   ├── Duplicates
│   └── Bibliographic records
└── output
    └── OrganizationPlan
```

An `OrganizationPlan` should be serializable to JSON and contain enough evidence to explain every proposed operation.

The planner does not execute operations.

## Layer 11 — Review and apply

I want to reuse upstream review/apply/undo infrastructure rather than create an unrelated second mutation system.

Review should be able to display:

- current path;
- detected identity/type;
- proposed destination/name;
- confidence;
- reasons/evidence;
- policy rules involved;
- relationships affected;
- conflicts;
- duplicate implications.

Apply receives an approved plan and records an audit result.

## Layer 12 — GUI

The GUI is the everyday interface and should be a thin presentation/orchestration layer over core services.

Potential areas:

```text
Overview
Index
Search
Inbox
Collections
Duplicates
Review
Taxonomy / Rules
History / Undo
Diagnostics
```

The GUI must not invent alternate categorization logic that the CLI cannot reproduce.

## Layer 13 — CLI/headless

The CLI is the advanced interface and a machine contract.

It should support human output and structured JSON output. The long-term CLI can expose advanced switches not shown prominently in the GUI, provided they remain explicit and safe.

Examples in `PERSONAL-ORGANIZER-CLI.md` that are marked **planned** are design targets, not current commands.

## Layer 14 — Agent/MCP integration

An agent should never receive direct “do anything to the filesystem” authority merely because it can call a tool.

Agent integrations should call constrained services such as:

```text
index roots
query index
extract/OCR
build plan
inspect plan
request apply
```

Mutation still goes through policy, review/apply rules, locks, and audit history.

Machine-readable results should make it possible for an agent to reason without scraping GUI text.

## Data and privacy direction

Whole-PC organization can touch sensitive material. Local processing is preferred where practical, especially for bulk indexing and content extraction.

Remote model usage, when enabled, should be explicit about what content leaves the machine. Future policy should allow paths/categories to be marked local-only.

The index itself may contain sensitive filenames and extracted summaries, so its storage location and permissions matter even though it is not a copy of the original files.

## Error and confidence model

The architecture should avoid collapsing all outcomes into success/failure.

Useful states include:

```text
indexed
partially indexed
unreadable
protected
not analyzed
analysis failed
needs review
conflict
stale / missing
```

Confidence applies to inferred information, not deterministic metadata such as file size.

## Testing architecture

Tests should be layered:

- unit tests for deterministic policy/parsing/grouping logic;
- integration tests for temporary filesystem + SQLite behavior;
- CLI contract tests for parse/output/exit codes;
- regression fixtures for Arabic/Urdu/PDF/OCR behavior;
- plan/apply tests using temporary directories only;
- no test should require destructive operations on the developer's real filesystem.

Fork-specific CI should run against `personal-organizer` and `feature/**` work even though upstream CI is primarily targeted at `main`.

## Documentation architecture

Fork documentation is deliberately namespaced with `PERSONAL-ORGANIZER-*` to reduce upstream conflicts.

The documentation contract is:

```text
PERSONAL-ORGANIZER.md                 project entry point
PERSONAL-ORGANIZER-ROADMAP.md         what I plan to build
PERSONAL-ORGANIZER-ARCHITECTURE.md    how it should fit together
PERSONAL-ORGANIZER-CLI.md             CLI contract/status
PERSONAL-ORGANIZER-DEVELOPMENT-LOG.md chronological engineering record
AGENTS.md                              rules for coding agents/tools
Issues                                actionable backlog / bugs
Pull requests                         reviewable implementation units
```

Documentation is updated alongside code. Future behavior is always labelled as planned.

## Current architecture boundary

As of Phase 1, the implemented fork-specific core is still intentionally small:

```text
PersonalFileIndex
    ↓
personal_file_index.db
```

It already provides the persistence/traversal foundation, but content extraction integration, OCR, policy, relationships, planner, GUI surfaces, and the expanded CLI remain later work unless their documentation explicitly says otherwise.
