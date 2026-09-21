# Personal AI File Organizer — CLI Contract

This document defines how I want the command-line and headless interface of my fork to evolve.

The GUI remains the normal interface. The CLI is intentionally designed as the **power-user superset**: anything important the GUI can do should eventually be scriptable, while some diagnostic, batch, experimental, and machine-oriented controls may exist only in the CLI.

## Status legend

Every command in this document is labelled so the documentation never implies that a planned interface already exists.

- **Implemented upstream** — already available in Hyperfield AI File Sorter.
- **Implemented in fork** — available in my fork.
- **In progress** — actively being implemented but not yet a stable command.
- **Planned** — design target only; do not try to use it yet.

## Existing upstream headless interface

**Status: Implemented upstream.**

Hyperfield already provides a headless analysis contract for categorization/renaming and review-plan application. The authoritative upstream contract remains [`headless-runtime-contract.md`](headless-runtime-contract.md).

Current upstream shape includes commands conceptually like:

```text
aifilesorter --headless --operation <categorize|rename|categorize-and-rename> --path <path> ...
aifilesorter --headless-apply --review-file <review-plan.json> ...
```

My personal-organizer CLI will build alongside this contract rather than silently breaking it.

## Design principles

1. GUI and CLI call the same core services.
2. Read-only commands are the easiest/default operations.
3. Mutation commands are explicit and plan-based.
4. JSON output is a first-class contract, not an afterthought.
5. Commands return meaningful exit codes.
6. Errors identify skipped/unreadable paths instead of pretending a scan was complete.
7. Elevated Windows privileges are never obtained by bypassing Windows security.
8. Experimental/dangerous switches are clearly named and documented.
9. No command may treat model output as permission to mutate files without the required policy/review/apply gate.
10. Planned syntax in this document may change until implemented and tested.

## Command family direction

The intended top-level model is:

```text
aifilesorter <command> [options]
```

with command families such as:

```text
index
search
inspect
extract
ocr
duplicates
policy
plan
review
apply
undo
history
db
doctor
```

Backward-compatible upstream `--headless` flags can remain supported.

---

## `index`

**Status: In progress (Phase 1).**

Purpose: scan selected filesystem roots and update `personal_file_index.db` without modifying source files.

Target syntax:

```powershell
aifilesorter index "C:\Users\Hp\Documents"
```

Multiple roots:

```powershell
aifilesorter index `
  --root "C:\Users\Hp\Documents" `
  --root "C:\Users\Hp\Downloads"
```

Planned options:

```text
--root <path>                 repeatable scan root
--include-hidden              include hidden entries
--follow-reparse-points       advanced; off by default
--hash <off|smart|all>        hashing policy
--include <pattern>           include filter
--exclude <pattern>           exclusion filter
--project-mode <protect|index|raw>
--json                        machine-readable result
--status-file <path>          persistent status JSON
--database <path>             explicit index DB for advanced/testing use
```

### Hash modes

**Planned.**

```text
off    metadata only
smart  hash when needed for identity/duplicate analysis
all    hash every regular file
```

`all` should not be the default for whole-drive scans because it creates unnecessary I/O.

### Project modes

**Planned refinement.**

```text
protect  detect project roots and protect internals from generic organization
index    index project internals for search, but keep mutation protection
raw      advanced traversal with project protection disabled
```

The current Phase 1 implementation conservatively treats strong project roots as protected units; the final CLI should separate read/index behavior from mutation protection.

### Example JSON result

**Planned contract.**

```json
{
  "kind": "aifs.personalIndexResult",
  "status": "completed",
  "runId": 42,
  "filesIndexed": 82417,
  "directoriesIndexed": 9128,
  "protectedProjects": 14,
  "skipped": 201,
  "errors": 27,
  "database": ".../personal_file_index.db",
  "warnings": []
}
```

---

## `search`

**Status: Planned.**

Purpose: search the local index using filenames, metadata, extracted text, bibliographic fields, and later semantic signals.

Examples:

```powershell
aifilesorter search "الإمام النووي"
```

```powershell
aifilesorter search "Flutter PDF outline" --json
```

Potential filters:

```text
--type pdf
--language ar
--root <path>
--modified-after <date>
--project <name>
--content-only
--filename-only
--limit <n>
```

Search must distinguish deterministic matches from later semantic/model-derived matches.

---

## `inspect`

**Status: Planned.**

Purpose: show what the system knows about one file/folder without changing it.

```powershell
aifilesorter inspect "C:\Books\book.pdf" --json
```

Possible output:

```text
path
file identity
hash state
text-layer state
OCR state
language
summary
bibliographic record
relationships
project membership
policy classification
```

This will be useful both for debugging and for agents.

---

## `extract`

**Status: Planned.**

Purpose: run deterministic document extraction and cache results.

```powershell
aifilesorter extract "C:\Books" --only-missing
```

Extraction should reuse upstream document-reading capabilities.

---

## `ocr`

**Status: Planned.**

Purpose: OCR files that lack sufficient embedded text.

Target usage:

```powershell
aifilesorter ocr "C:\Books\Arabic" `
  --language ara+urd+eng `
  --only-missing-text
```

Potential controls:

```text
--pages identify
--pages all
--language <set>
--min-text-density <value>
--force
--engine <id>
--json
```

`identify` should mean strategic-page OCR rather than full-document OCR.

---

## `duplicates`

**Status: Planned.**

Exact duplicate analysis:

```powershell
aifilesorter duplicates --root "C:\Users\Hp" --exact --json
```

Early versions report duplicate candidates but do not delete them automatically.

Potential modes:

```text
--exact
--near-images
--near-documents
--minimum-size <bytes>
--report <file>
```

---

## `policy`

**Status: Planned.**

Purpose: validate and inspect `ORGANIZE.md` behavior without touching files.

```powershell
aifilesorter policy validate ORGANIZE.md
```

```powershell
aifilesorter policy explain "C:\Downloads\some-file.pdf"
```

Useful commands:

```text
policy validate
policy print
policy explain
policy test
```

Policy errors should fail before plan/apply operations.

---

## `plan`

**Status: Planned.**

Purpose: generate an organization plan without applying it.

```powershell
aifilesorter plan `
  --root "C:\Users\Hp\Downloads" `
  --policy ORGANIZE.md `
  --output organization-plan.json
```

This command is read-only with respect to the source filesystem.

A plan records proposed paths/names, reasons, confidence, warnings, relationships, and policy evidence.

---

## `review`

**Status: Planned as a personal-organizer command; upstream already has review-plan concepts.**

Purpose: inspect or edit a saved plan/review file.

Possible forms:

```powershell
aifilesorter review organization-plan.json
```

or machine-readable validation:

```powershell
aifilesorter review organization-plan.json --check --json
```

---

## `apply`

**Status: Planned personal-organizer command; upstream has `--headless-apply`.**

Purpose: apply an already generated/approved plan.

```powershell
aifilesorter apply organization-plan.json
```

An `apply` command must never silently regenerate the plan with new AI output unless explicitly requested.

Expected safeguards:

```text
validate plan schema
validate current source identity
validate conflicts
validate policy
acquire runtime lock
apply
write audit record
return per-entry result
```

Potential `--force` switches must be narrow and explicit. `--force` is never permission to bypass Windows ACLs or arbitrary safety checks.

---

## `undo` and `history`

**Status: Planned fork CLI; upstream already has undo/review-history infrastructure.**

Examples:

```powershell
aifilesorter history --limit 20
```

```powershell
aifilesorter undo --plan-id 123
```

Undo should report operations that cannot be reversed cleanly rather than claiming complete success.

---

## `db`

**Status: Planned.**

Power-user index/database maintenance.

Potential commands:

```text
db stats
db check
db vacuum
db export
db stale
db reset-analysis
```

Destructive database maintenance must require explicit command names/options.

I do not intend to expose arbitrary SQL as the normal interface, although advanced/debug builds may offer it.

---

## `doctor`

**Status: Planned.**

Purpose: report environment/readiness without changing anything.

Potential checks:

```text
application version
index database health
configured LLM
GPU/runtime availability
OCR engines/language packs
PDF extraction support
filesystem permissions
policy file status
upstream/fork build metadata
```

This should become the first troubleshooting command.

---

## Safety levels

Commands should be categorized internally/documented by effect:

```text
READ ONLY
  index, search, inspect, extract, ocr, duplicates, policy validate, plan, doctor

DATABASE MAINTENANCE
  selected db commands

FILESYSTEM MUTATION
  apply, undo

DEVELOPER / EXPERIMENTAL
  raw traversal, diagnostics, test hooks
```

OCR/extraction are “read only” with respect to source files even though they write caches/index data.

## Windows privilege model

The CLI inherits the privileges of the process that launched it.

Normal PowerShell:

```text
normal user access
```

Administrator PowerShell:

```text
elevated user access
```

The application may detect elevation and report inaccessible locations, but it must not attempt to bypass Windows security.

## Output contract

Human output is for interactive use; JSON is for automation.

JSON objects should include a stable `kind`/schema identity and explicit status. Stderr is for diagnostics; structured stdout/status files should remain parseable.

Possible exit-code direction:

```text
0 success
1 operation failed
2 invalid usage
3 busy/locked
4 unsupported
5 completed with partial access/errors
```

Final exit-code allocation must remain compatible with upstream headless behavior where the interfaces overlap.

## Agent usage

Future agents should use machine-readable CLI/core contracts rather than controlling the GUI visually when a supported command exists.

Preferred pattern:

```text
agent asks core for facts
→ core returns structured result
→ agent proposes/requests a plan
→ user/policy approves where required
→ core applies and audits
```

Agents do not get a hidden mutation bypass.

## Current Phase 1 CLI deliverable

The next real CLI deliverable is intentionally small:

```text
aifilesorter index <selected-root> --json
```

or an equivalent syntax integrated cleanly with the existing argument parser.

It must:

- explicitly require a target root;
- use `PersonalFileIndex`;
- produce scan counts/warnings/database location;
- never mutate scanned source files;
- be covered by parser/behavior tests;
- be tested on a temporary/small directory before broader use.

Until that command lands, examples above marked **planned** are documentation of intent only.
