# Personal Organizer Architecture

## Purpose

This document defines the fork-specific architecture layered on top of AI File Sorter. It is intentionally separate from the upstream `docs/architecture.md` so upstream architectural documentation can continue to merge cleanly.

## Core principle

The GUI, CLI, and future agent/MCP interfaces must call the same underlying services.

```text
                         +----------------------+
                         |   Shared Services    |
                         |----------------------|
                         | PersonalFileIndex    |
                         | Content extraction   |
                         | OCR                  |
                         | Taxonomy / policy    |
                         | Planning             |
                         | Duplicate detection  |
                         | Relationships        |
                         | Review / apply       |
                         | Audit / undo         |
                         +----------+-----------+
                                    |
               +--------------------+--------------------+
               |                    |                    |
               v                    v                    v
             GUI                  CLI                Agent/MCP
        safe everyday       advanced/scripting      automation
          workflows              workflows            layer
```

No feature should have a separate GUI-only or CLI-only implementation unless there is a strong platform-specific reason.

## Upstream components we reuse

The fork should continue to reuse upstream services wherever they already solve the problem well:

- Qt desktop application and dialogs;
- `AnalysisCoordinator` and workflow infrastructure;
- `DocumentTextAnalyzer` for text-bearing documents;
- visual/image analysis;
- local llama.cpp and remote API clients;
- categorization/taxonomy infrastructure;
- review/apply workflow;
- persistent undo/history mechanisms;
- `ProtectedProjectDetector`;
- headless runtime and Explorer-integration foundations;
- settings and configuration infrastructure.

## Fork-specific components

### `PersonalFileIndex`

A dedicated persistent inventory separate from the normal categorization cache.

Why separate:

- the categorization cache answers “what category did this file receive?”;
- the personal index answers “what exists on the selected filesystem, what do we know about it, and when did we last observe it?”;
- later phases require hashes, extraction status, OCR status, summaries, project relationships, bibliographic fields, confidence, and other metadata that should not overload the categorization cache schema.

Current database:

```text
personal_file_index.db
```

Current tables:

```text
personal_index_scan_runs
personal_index_entries
```

Planned additional logical data areas:

```text
content extractions
OCR cache
bibliographic metadata
relationships / collections
duplicate groups
organization proposals
policy decisions
```

Schema evolution should use explicit migrations once the first draft stabilizes.

## Indexing pipeline

Target architecture:

```text
selected roots
    ↓
filesystem traversal
    ↓
metadata inventory
    ├── paths
    ├── sizes
    ├── timestamps
    ├── file type
    ├── project membership
    └── optional hashes
    ↓
content capability detection
    ├── existing text layer
    ├── image/visual content
    ├── media metadata
    ├── archive members
    └── OCR required?
    ↓
content extraction / OCR
    ↓
normalized local index
    ↓
policy + taxonomy + relationships
    ↓
organization planning
```

The index stage is non-mutating with respect to source files.

## Streaming traversal

Whole-PC scans can contain hundreds of thousands or millions of filesystem entries. The personal index must not require all paths to be held in RAM before persistence.

Preferred model:

```text
read one entry
→ inspect metadata
→ persist/upsert
→ continue
```

SQLite writes are batched to avoid one transaction per file.

## Projects

Project protection and project indexing are separate concerns.

Final intended semantics:

1. Detect project roots such as Git, Node.js, Flutter/Gradle, Rust, Go, .NET, Unity, Unreal, Godot, Blender, and similar structures.
2. Mark the project boundary and type in the index.
3. Prevent organization plans from moving individual internal files out of protected projects unless explicitly requested.
4. Still allow useful read-only indexing of project contents where configured.
5. Exclude generated/dependency directories such as `.git`, `node_modules`, build outputs, caches, and virtual environments.

The first draft currently indexes a strongly protected project as a single unit and skips its internals. This is safe but intentionally conservative; deeper read-only project indexing is a Phase 1 hardening item before the full-PC index is considered complete.

## Exclusions

Exclusions fall into different classes and should eventually be represented separately:

### Absolute/protected system paths

Examples on Windows:

```text
C:\Windows
C:\Program Files
C:\Program Files (x86)
C:\ProgramData
C:\System Volume Information
```

### User/application data

Examples:

```text
AppData
browser profiles
application caches
Temp
```

### Generated project directories

Examples:

```text
.git
node_modules
.next
build
dist
.venv
venv
__pycache__
```

The initial implementation uses directory-name exclusions. Before whole-drive production use, system exclusions should become path-aware so an unrelated user folder named `Windows` is not excluded merely because of its name.

## Hash strategy

Hashing is optional during general indexing.

Reason:

- metadata indexing is relatively cheap;
- hashing every byte on a large drive creates substantial I/O;
- exact duplicate detection can first group files by size, then hash only candidate collisions.

Long-term preferred modes:

```text
--hash none
--hash changed
--hash duplicate-candidates
--hash all
```

`all` should remain an explicit power-user choice.

## Content extraction

Content extraction is layered.

### Text-bearing documents

Reuse upstream `DocumentTextAnalyzer` for supported formats such as PDF, DOCX, XLSX, PPTX, ODT/ODS/ODP, Markdown, text, HTML, XML, JSON, and related formats.

### Scanned PDFs

If a PDF contains insufficient usable text:

```text
PDF
→ text-layer density check
→ render strategic pages
→ OCR
→ Unicode normalization
→ cached OCR result
→ summarization / bibliography
```

Arabic, Urdu, and English are priority languages for the fork.

OCR should be content-addressed/cached so unchanged scans are not repeatedly processed.

### Images

Reuse the existing visual model pipeline. Index descriptions separately from suggested filenames so descriptions can support later search and organization decisions.

## Policy system

A future `ORGANIZE.md` loader should supply human-owned rules to AI-assisted planning.

Policy should cover:

- allowed taxonomy;
- naming language;
- Arabic religious-title preservation;
- research vs reference-library separation;
- protected paths;
- Markaz/project-specific ownership rules;
- archive rules;
- confidence thresholds;
- never-move patterns;
- manual-review patterns.

The policy should be configuration, not hard-coded C++ personal data.

## Planning boundary

Indexing and planning must remain separate.

```text
index database
     ↓
planning engine
     ↓
proposal JSON / database rows
     ↓
review UI / CLI
     ↓
explicit apply
```

The planner may recommend:

```text
Keep
Move
Rename
Move + Rename
Merge collection
Archive
Manual Review
Possible Duplicate
Protected
```

It must not execute those recommendations while generating them.

## GUI vs CLI

### GUI

Designed for ordinary use:

- dashboard/index status;
- choose scan roots;
- search;
- browse taxonomy;
- review proposed changes;
- OCR/index status;
- duplicates;
- policy/rule editing;
- history/undo.

### CLI

The CLI is intentionally a superset:

- all core operations;
- machine-readable JSON;
- advanced include/exclude controls;
- diagnostics;
- raw index export/query helpers;
- batch OCR;
- policy validation;
- automation and scheduled jobs;
- experimental switches;
- future MCP/agent entry points.

CLI power does not bypass operating-system permissions. Elevated filesystem access still requires the process to be launched with the appropriate Windows permissions.

## Safety levels

Commands should be described by capability rather than hiding risk:

```text
READ ONLY
  index, search, inspect, plan, duplicate analysis

REVIEWED MUTATION
  apply a saved/reviewed plan

ADVANCED MUTATION
  explicit bulk operations with additional confirmation/force flags

DEVELOPER
  database maintenance, migrations, diagnostics, experimental features
```

A `--force` flag must never silently turn a read-only command into a mutating command.

## Database and privacy

Sensitive user content should remain local by default.

If a remote LLM provider is selected for content analysis, the UI/CLI should make that boundary visible. Index metadata itself should not automatically be uploaded to any remote service.

## Upstream merge strategy

Prefer adding new fork-specific classes rather than modifying central upstream files.

When modification is necessary:

- keep patches narrow;
- avoid unrelated formatting changes;
- do not rename upstream classes for fork aesthetics;
- document the reason in the development log;
- add focused regression coverage.

This lowers conflict cost when syncing `upstream/main`.
