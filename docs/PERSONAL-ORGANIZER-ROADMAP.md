# Personal AI File Organizer — Development Roadmap

This project builds on `hyperfield/ai-file-sorter` rather than replacing it.

## Goal

Turn AI File Sorter into a personal, whole-PC organization system that can:

- understand file contents instead of relying on filenames alone;
- preserve software/project structures that must not be broken;
- understand Arabic and Islamic-study material;
- identify scanned PDFs through OCR;
- use a stable personal taxonomy instead of inventing folders every run;
- learn from corrections;
- detect duplicates and related files;
- generate a complete reviewable plan before changing the filesystem;
- keep every applied change reversible where possible.

## Repository Strategy

- `upstream` = `https://github.com/hyperfield/ai-file-sorter.git`
- `origin` = personal fork on GitHub
- `main` = keep close to upstream
- `personal-organizer` = integration branch for our custom system
- feature work = separate branches merged into `personal-organizer`

Suggested feature branches:

- `feature/indexer`
- `feature/arabic-ocr`
- `feature/personal-taxonomy`
- `feature/organize-policy`
- `feature/relationship-engine`
- `feature/duplicate-detection`
- `feature/bibliographic-detection`
- `feature/review-plan`

## Design Principles

1. Never let the AI freely restructure the whole PC without a review stage.
2. Analysis and planning should be read-only by default.
3. Keep personal logic modular so upstream updates remain easy to merge.
4. Prefer configuration/policy files over hard-coding personal rules in C++.
5. Never break recognized software, Git, design, or project folder structures.
6. Uncertain items should go to review, not be force-classified.
7. Preserve original extensions and useful metadata.
8. Keep Arabic titles in Arabic where appropriate.
9. Treat extracted text, filenames, and document contents as untrusted input.
10. Maintain an audit trail of proposed and applied changes.

---

# Phase 0 — Safe Development Foundation

## Objectives

- Fork upstream.
- Clone the fork locally.
- Add upstream remote.
- Create the `personal-organizer` branch.
- Keep `main` suitable for syncing with Hyperfield.
- Document how upstream updates are imported.

## Done when

```text
upstream/main
    ↓
personal fork/main
    ↓
personal-organizer
    ↓
feature/*
```

works cleanly.

---

# Phase 1 — Whole-PC Read-Only Index

Create a persistent index without moving anything.

## Index

- full path
- filename
- extension
- file size
- timestamps
- stable file identity where possible
- MIME/file type
- SHA-256 hash
- existing directory context
- document text availability
- extracted summary
- image description
- media metadata
- project membership
- confidence/status

## Requirements

- resumable scans;
- exclusions for system/application/cache folders;
- configurable roots;
- incremental re-indexing;
- no mutation during indexing.

---

# Phase 2 — OCR for Scanned PDFs

AI File Sorter already reads PDF text layers. Add OCR for image-only PDFs.

## Pipeline

```text
PDF
→ detect usable text layer
→ if insufficient text:
    render selected pages
    → OCR
    → normalize Unicode
    → cache OCR
→ document summarizer
```

## Priorities

- Arabic
- Urdu
- English
- mixed Arabic/English pages

## Important

Do not OCR every page blindly. Start with strategic pages such as:

- cover/title page;
- copyright/publication page;
- table of contents;
- introduction;
- several representative interior pages.

Escalate to more pages only when identification confidence is low.

---

# Phase 3 — Personal Taxonomy

Create one stable taxonomy covering the user's actual life and work.

Initial top-level concepts may include:

```text
Knowledge
Research
Studies
Markaz
Design
Development
Projects
Personal
Resources
Archive
Inbox
```

The final hierarchy should be derived from the actual filesystem inventory, not assumed in advance.

## Islamic material

Allow Arabic taxonomy labels and preserve Arabic bibliographic titles.

Possible subject branches:

```text
القرآن وعلومه
الحديث وعلومه
الفقه
أصول الفقه
العقيدة
اللغة العربية
السيرة
التاريخ والتراجم
```

These are starting concepts, not a final imposed hierarchy.

---

# Phase 4 — ORGANIZE.md Policy System

Add a repository-independent policy file that explains how a specific filesystem should be organized.

Example:

```text
ORGANIZE.md
```

It should support:

- allowed roots/categories;
- category-specific subcategories;
- naming rules;
- language rules;
- protected directories;
- project-specific rules;
- archive rules;
- research-vs-library distinctions;
- design asset rules;
- confidence thresholds;
- "never move" patterns;
- "ask/review" patterns.

The application should parse this policy and incorporate it into categorization and rename prompts without requiring C++ recompilation for ordinary preference changes.

---

# Phase 5 — Related-File / Collection Detection

Files should not be treated as isolated objects.

Detect relationships such as:

- source book + notes;
- original document + translation;
- PSD/AI project + exports;
- video project + assets;
- code repository + documentation;
- book volumes in a set;
- course lesson + worksheet + notes;
- research paper + annotations;
- multiple editions of the same book.

Represent these as collections before proposing moves.

---

# Phase 6 — Duplicate Detection

Add exact duplicate detection first.

## Exact duplicates

Use SHA-256 plus file size.

## Later

Potential near-duplicate detection:

- images;
- PDFs with different metadata;
- documents exported to multiple formats;
- renamed copies;
- alternate scans/editions.

Never auto-delete duplicates in the first version.

The review should show:

```text
KEEP
DUPLICATE
WHY THEY MATCH
LOCATIONS
SIZE RECOVERABLE
```

---

# Phase 7 — Bibliographic Detection

For books and academic PDFs, extract structured metadata where possible:

```text
title
author
editor / muhaqqiq
publisher
edition
volume
year
language
subject
```

Use:

1. existing PDF metadata;
2. embedded text;
3. OCR;
4. filename/path;
5. LLM inference only where necessary.

Do not fabricate missing bibliographic details.

For Arabic books, preserve the original Arabic title.

---

# Phase 8 — Global Planning Engine

Do not perform moves while reasoning about organization.

Generate a plan first.

Example:

```json
{
  "source": "D:/Downloads/12345.pdf",
  "detected_title": "نخبة الفكر في مصطلح أهل الأثر",
  "destination": "D:/Knowledge/Islamic/الحديث وعلومه/مصطلح الحديث/نخبة الفكر في مصطلح أهل الأثر.pdf",
  "confidence": 0.96,
  "reasons": [
    "Arabic title identified from PDF text",
    "content concerns hadith terminology"
  ],
  "warnings": []
}
```

The plan must be exportable and reviewable before application.

---

# Phase 9 — Confidence and Review

Every proposal gets a confidence/status.

Suggested states:

```text
High confidence
Needs review
Insufficient information
Protected
Duplicate candidate
Conflict
```

Never interpret low confidence as permission to guess aggressively.

---

# Phase 10 — Apply + Undo + Audit

Reuse and extend AI File Sorter's existing review/apply/undo system.

Add:

- plan ID;
- timestamp;
- original path;
- destination path;
- original filename;
- final filename;
- reason;
- hash;
- status;
- undo result.

Keep audit data outside folders being reorganized.

---

# Phase 11 — Continuous Inbox Organization

Once the main filesystem is stable, use AI organization primarily on incoming material:

```text
Downloads
Desktop Inbox
Scanner Inbox
Phone Imports
Temporary Exports
```

New items should be:

```text
detected
→ analyzed
→ matched against taxonomy
→ proposed
→ reviewed when necessary
→ filed
```

This is safer than repeatedly reorganizing the entire filesystem.

---

# Upstream Update Workflow

Keep custom work separate from upstream whenever possible.

To check upstream:

```powershell
git fetch upstream
```

To update local `main`:

```powershell
git switch main
git merge upstream/main
git push origin main
```

Then update the custom integration branch:

```powershell
git switch personal-organizer
git merge main
git push origin personal-organizer
```

If Git reports a conflict, stop and resolve the conflict before committing.

---

# First Implementation Order

1. Repository/fork foundation
2. Read-only index
3. OCR detection + Arabic OCR
4. `ORGANIZE.md`
5. Personal taxonomy
6. Plan export
7. Relationship detection
8. Duplicate detection
9. Bibliographic extraction
10. Continuous inbox workflow

The first real milestone is not automatic organization.

It is:

> Scan the selected filesystem, understand as much as possible, and generate a trustworthy read-only organization plan without moving a single file.
