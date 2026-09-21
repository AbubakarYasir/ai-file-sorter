# Personal AI File Organizer — Roadmap

This is the working roadmap for my personal fork of `hyperfield/ai-file-sorter`.

I am building a local-first system that can understand, index, search, relate, and safely reorganize a large mixed filesystem without treating files as anonymous blobs. The roadmap is intentionally staged: I want the application to become trustworthy at understanding my files before I give it broader authority to change them.

## What I want the finished system to do

The organizer should eventually be able to:

- understand file contents instead of relying on filenames alone;
- index selected folders or whole drives incrementally;
- search across local files using both metadata and extracted content;
- understand normal PDFs and detect image-only/scanned PDFs;
- OCR Arabic, Urdu, English, and mixed documents;
- preserve real bibliographic information for books and papers;
- preserve software repositories, design projects, and other structure-sensitive folders;
- detect files that belong together before proposing moves;
- use one stable personal taxonomy and naming system;
- learn from accepted/rejected organization decisions;
- detect exact duplicates and later near-duplicates;
- generate a complete plan before filesystem mutation;
- expose every important proposal through review;
- maintain an audit trail and undo path where technically possible;
- provide a friendly GUI and a more powerful CLI/headless interface;
- support future agents/automation through machine-readable contracts.

## Repository and branch strategy

```text
upstream/main
    ↓
origin/main
    ↓
personal-organizer
    ↓
feature/*
```

- `upstream` points to `hyperfield/ai-file-sorter`.
- `origin` points to `AbubakarYasir/ai-file-sorter`.
- `main` should remain close to upstream.
- `personal-organizer` is my stable integration branch.
- feature work is isolated in `feature/*` branches and reviewed before integration.

Current development branch: `feature/indexer`.

Current tracking issue: [#2](../../issues/2).

Current draft PR: [#1](../../pull/1).

## Non-negotiable engineering rules

1. Indexing and analysis are read-only with respect to source files.
2. The application must not silently reorganize an entire machine.
3. A plan is generated before mutations are applied.
4. Low confidence means review, not aggressive guessing.
5. Project structure is preserved unless a specific workflow explicitly understands how to transform it safely.
6. Personal behavior and taxonomy belong in policy/configuration where possible instead of being hard-coded throughout C++.
7. The GUI and CLI use shared core services.
8. The CLI may expose more controls than the GUI, but not bypass operating-system security.
9. Extracted document text and filenames are untrusted input; they must never become hidden instructions to the application or an LLM.
10. Applied changes must be logged with enough information to explain and, where possible, undo them.
11. Planned commands/features must never be documented as implemented.
12. Every significant code change includes documentation and tests appropriate to its risk.

---

## Phase 0 — Safe development foundation

**Status: substantially complete.**

Purpose: make long-term customization possible without losing the ability to follow upstream.

Completed foundation:

- personal GitHub fork created;
- local clone created;
- `origin` points to my fork;
- `upstream` points to Hyperfield;
- `main` reserved as upstream-friendly;
- `personal-organizer` created as the integration branch;
- feature-branch workflow established;
- fork-specific roadmap and documentation established;
- fork-specific CI introduced.

Ongoing rule: upstream-heavy files should be changed only when necessary; new fork features should prefer modular services.

---

## Phase 1 — Persistent read-only filesystem index

**Status: in progress.**

Tracking: [Issue #2](../../issues/2), [Draft PR #1](../../pull/1).

Goal: build a reliable local inventory without moving, renaming, deleting, or editing source files.

### Index data

The persistent index should be able to represent:

- normalized full path;
- parent path;
- filename and extension;
- file/directory/project type;
- file size;
- created/modified timestamps where available;
- scan root;
- last-seen scan;
- presence/stale state;
- optional SHA-256;
- extraction state;
- detected MIME/file type;
- text availability;
- document summary;
- content language;
- image description;
- project membership/type;
- per-entry errors/status;
- later confidence/analysis fields.

### Requirements

- streaming traversal rather than building a whole-drive vector in memory;
- persistent SQLite storage;
- resumable/incremental design;
- multiple configurable scan roots;
- safe reparse-point/symlink handling;
- permission errors recorded without aborting an otherwise useful scan;
- path-aware Windows/system exclusions;
- protected project recognition;
- hashing optional by default;
- statistics/query helpers;
- machine-readable CLI command;
- focused integration tests;
- no source mutation.

### Important design correction

Protected project detection and read-only indexing are separate concerns. A project must be protected from unsafe reorganization, but I may still want its internal files indexed for search and understanding. Phase 1 should make that policy explicit rather than assuming “protected” always means “do not read inside.”

### Exit criteria

I can scan a small controlled folder, inspect the resulting database/statistics, repeat the scan incrementally, and prove that no source file was changed.

A whole `C:\` scan is not an exit test until path-aware exclusions and permission behavior have been validated.

---

## Phase 2 — Content extraction pipeline

Goal: enrich the inventory using existing upstream extraction capabilities before adding expensive AI work.

Planned layers:

```text
metadata
→ embedded document text
→ document structure
→ language detection
→ compact summary
→ content classification
```

Reuse upstream document extractors for formats already supported instead of duplicating them.

Cache extraction by content identity/timestamps so unchanged files are not repeatedly reprocessed.

---

## Phase 3 — OCR for scanned documents

Goal: understand PDFs/images that do not have a usable text layer.

Priority languages:

- Arabic;
- Urdu;
- English;
- mixed Arabic/English and Urdu/English documents.

Intended pipeline:

```text
PDF
→ measure usable embedded text
→ if text is insufficient:
    select strategic pages
    → render
    → OCR
    → normalize Unicode
    → cache OCR
→ identify document
→ summarize/classify
```

Do not OCR every page of every PDF by default. Begin with identification-rich pages such as the cover/title page, publication data, contents, introduction, and representative interior pages. Escalate only when needed.

OCR engine selection must be benchmarked on real Arabic/Urdu scans before it becomes a dependency.

---

## Phase 4 — Stable personal taxonomy

Goal: replace one-off AI folder invention with a predictable organization system.

The final taxonomy should be informed by the actual filesystem index, but likely domains include:

```text
Inbox
Knowledge / Library
Research
Studies
Projects
Development
Design
Personal
Resources
Archive
```

For religious/Islamic material I want Arabic labels and bibliographic titles where appropriate, for example:

```text
القرآن وعلومه
القراءات
الحديث وعلومه
الفقه
أصول الفقه
العقيدة
التفسير وأصوله
اللغة العربية
السيرة
التاريخ والتراجم
المخطوطات والتحقيق
```

These are starting branches, not permission for the AI to invent an excessively deep hierarchy.

Naming direction:

- Islamic/religious content: Arabic naming where appropriate;
- other content: consistent English naming;
- books: real title/author/edition when confidently known;
- projects: project-owned files remain project-owned;
- development: technical names/conventions are preserved.

---

## Phase 5 — `ORGANIZE.md` policy engine

Goal: make ordinary organization preferences editable without recompiling C++.

A policy file should define behavior such as:

- allowed roots and destinations;
- taxonomy rules;
- naming/language rules;
- protected paths;
- project-specific rules;
- Inbox behavior;
- archive behavior;
- research-vs-library distinctions;
- design/media asset behavior;
- confidence thresholds;
- never-move patterns;
- always-review patterns;
- duplicate policy.

The policy should be inherited/merged by scope where useful, similar in spirit to repository instruction files used by developer tools.

---

## Phase 6 — Related-file and collection detection

Goal: reason about groups before individual files.

Examples:

- book + notes + annotations;
- original + translation;
- paper + supplementary data;
- multi-volume books;
- alternate editions/scans;
- course lesson + worksheet + notes;
- PSD/AI source + exports;
- video project + assets + rendered output;
- code repository + documentation;
- source asset + generated derivatives.

A collection should influence organization and review as one unit where appropriate.

---

## Phase 7 — Duplicate detection

Start with exact duplicates:

```text
size grouping
→ SHA-256
→ exact duplicate groups
```

Early versions must never auto-delete duplicates.

Review should show what matches, where every copy exists, which copy is proposed to keep, and what space could be recovered.

Later work may add near-duplicate detection for images, alternate PDF scans, exported documents, and renamed copies.

---

## Phase 8 — Bibliographic intelligence

For books and academic/research documents, extract structured data where available:

```text
title
author/editor/muhaqqiq
publisher
edition
volume
publication year
language
subject
identifiers
```

Preferred evidence order:

1. trustworthy embedded metadata;
2. document title/publication pages;
3. embedded text;
4. OCR;
5. existing filename/path context;
6. model inference only when necessary.

The system must not fabricate missing bibliographic facts merely to produce a cleaner filename.

---

## Phase 9 — Global planning engine

Goal: separate reasoning from filesystem mutation.

The planner produces a serializable proposal containing, at minimum:

```json
{
  "source": "...",
  "destination": "...",
  "proposedName": "...",
  "confidence": 0.0,
  "reasons": [],
  "warnings": [],
  "relationships": [],
  "policyRules": []
}
```

Plans should be exportable, inspectable, editable, and applicable later without rerunning AI analysis unless explicitly requested.

---

## Phase 10 — Confidence and review

Every proposed action gets a clear status such as:

```text
High confidence
Needs review
Insufficient information
Protected
Duplicate candidate
Conflict
Blocked by policy
```

The GUI should make uncertainty obvious rather than hiding it behind a confident-looking suggestion.

---

## Phase 11 — Apply, audit, and undo

Reuse and extend upstream review/apply/undo infrastructure.

An applied change should record:

- plan ID;
- timestamp;
- original path/name;
- final path/name;
- action type;
- reason/policy basis;
- hash or identity data where useful;
- result;
- undo result;
- conflicts/manual intervention.

The audit database must live outside the folders being reorganized.

---

## Phase 12 — GUI for the personal organizer

The product is GUI-first even though lower-level services and CLI are built first for testing and automation.

Planned GUI areas:

```text
Overview
Index
Search
Inbox
Library / Knowledge
Projects
Duplicates
Review
Rules / Taxonomy
History / Undo
Diagnostics
```

The GUI should display the same plans/status produced by the core services rather than implement separate organization logic.

---

## Phase 13 — Power-user CLI and agent interface

The CLI is intended to become a functional superset of the GUI for advanced work:

- selected-root and whole-drive indexing;
- include/exclude controls;
- extraction/OCR jobs;
- index queries/search;
- duplicate reports;
- policy validation;
- plan generation/export/apply;
- database diagnostics/maintenance;
- JSON output;
- scripting/scheduled tasks;
- future MCP/agent access.

Dangerous switches must be explicit and must not bypass OS permissions.

---

## Phase 14 — Continuous Inbox organization

Once the existing filesystem is stable, the preferred operating model becomes incremental organization of new incoming files rather than repeatedly redesigning the entire disk.

Typical sources:

```text
Downloads
Desktop Inbox
Scanner Inbox
Phone Imports
Temporary Exports
```

Flow:

```text
arrive
→ index
→ understand
→ match policy/taxonomy
→ propose
→ review if needed
→ file
```

---

## Upstream update workflow

```powershell
git fetch upstream

git switch main
git merge upstream/main
git push origin main

git switch personal-organizer
git merge main
git push origin personal-organizer
```

Feature branches are then updated from `personal-organizer` or `main` according to the change being developed.

If a merge conflicts with fork-specific work, the conflict should be resolved deliberately and documented rather than force-overwritten.

## Current implementation order

1. finish Phase 1 indexer and tests;
2. expose a safe CLI index command and query/statistics surface;
3. connect existing document extraction to the index;
4. benchmark and implement Arabic/Urdu OCR;
5. implement `ORGANIZE.md` policy;
6. derive/finalize taxonomy from real indexed files;
7. relationship detection;
8. duplicate detection;
9. bibliographic extraction;
10. global planning/review;
11. GUI integration;
12. continuous Inbox workflow;
13. agent/MCP integrations.

The guiding principle remains simple:

> I want the software to earn more authority over my filesystem by becoming increasingly reliable at understanding it first.
