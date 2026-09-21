# Personal Organizer CLI

## Status

This document defines the intended fork-specific CLI contract. Commands marked **implemented** exist in the shared core or test harness; commands marked **planned** are design targets and must not be assumed to work until wired into the application entry point.

The CLI is intended to be a power-user superset of the GUI while calling the same underlying services.

## Command philosophy

1. Read-only commands are safe by default.
2. Mutation is never an accidental side effect of analysis.
3. Machine-readable JSON is a first-class output mode.
4. Advanced flags may expose more control than the GUI but do not bypass Windows permissions.
5. Commands should be composable for PowerShell, scheduled jobs, and future agent/MCP integrations.

## Planned command groups

```text
aifilesorter index ...
aifilesorter search ...
aifilesorter inspect ...
aifilesorter plan ...
aifilesorter apply ...
aifilesorter ocr ...
aifilesorter duplicates ...
aifilesorter taxonomy ...
aifilesorter policy ...
aifilesorter export ...
aifilesorter db ...
aifilesorter doctor ...
```

The existing upstream headless interface remains valid and separate:

```text
aifilesorter --headless ...
aifilesorter --headless-apply ...
```

## `index` — planned application entry point

Purpose: populate/update the persistent personal filesystem index without modifying scanned source files.

Examples:

```powershell
aifilesorter index "C:\Users\Hp"
```

```powershell
aifilesorter index `
  "C:\Users\Hp\Documents" `
  "C:\Users\Hp\Downloads" `
  --json
```

Advanced example:

```powershell
aifilesorter index "C:\Users\Hp" `
  --include-hidden `
  --hash duplicate-candidates `
  --json `
  --output "index-run.json"
```

### Intended options

```text
--recursive / --no-recursive
--include-hidden
--follow-reparse-points
--protect-projects / --no-protect-projects
--exclude <path-or-pattern>
--include <path-or-pattern>
--hash <none|changed|duplicate-candidates|all>
--json
--output <file>
--database <file>
--rescan
--statistics
```

### Current Phase 1 engine support

The underlying `PersonalFileIndex` currently supports:

- multiple roots;
- streaming recursive traversal;
- hidden-file control;
- reparse/symlink control;
- protected-project detection;
- configurable excluded directory names;
- optional SHA-256 for all encountered regular files;
- configurable SQLite commit batch size;
- persistent scan-run history.

The public `aifilesorter index` application command is not considered implemented until it is wired into the main argument parser and covered by CLI regression tests.

## `search` — planned

Search the local index rather than only filenames.

Examples:

```powershell
aifilesorter search "الإمام النووي"
```

```powershell
aifilesorter search "Flutter PDF outline" --project Development --json
```

Planned search sources:

- filename/path;
- extracted document text;
- summaries;
- OCR text;
- image descriptions;
- bibliographic metadata;
- project metadata;
- taxonomy labels;
- relationships/collections.

## `inspect` — planned

Explain what the organizer knows about one file/folder and why.

```powershell
aifilesorter inspect "C:\Books\scan.pdf" --json
```

Potential output fields:

```text
path
type
size
timestamps
hash
language
text-layer status
OCR status
summary
bibliographic metadata
project membership
current taxonomy
proposed taxonomy
confidence
warnings
```

## `plan` — planned and read-only

Generate organization proposals without changing source files.

```powershell
aifilesorter plan "C:\Users\Hp\Downloads" `
  --policy ORGANIZE.md `
  --output plan.json
```

Plan actions may include:

```text
keep
move
rename
move_and_rename
archive
manual_review
possible_duplicate
protected
```

## `apply` — planned mutation command

Apply a previously generated/reviewed plan.

```powershell
aifilesorter apply plan.json
```

The default should require a plan produced by a compatible version and preserve audit/undo information.

Advanced explicit override:

```powershell
aifilesorter apply plan.json --force
```

`--force` may relax validation checks but must not turn unrelated read-only commands into mutation commands.

## `ocr` — planned

Batch OCR for documents that lack useful text layers.

```powershell
aifilesorter ocr "C:\Books\Arabic" `
  --language ara+urd+eng `
  --only-missing-text
```

Planned options:

```text
--language <languages>
--only-missing-text
--pages <selection>
--full-document
--refresh
--json
```

Default behavior should use strategic pages before escalating to whole-document OCR.

## `duplicates` — planned

Exact duplicate analysis should use size grouping before hashing candidates.

```powershell
aifilesorter duplicates "C:\Users\Hp" --exact --json
```

No duplicate command should delete files by default.

Potential later modes:

```text
--exact
--near-images
--near-documents
--alternate-editions
```

## `taxonomy` — planned

```powershell
aifilesorter taxonomy show
aifilesorter taxonomy validate
aifilesorter taxonomy export taxonomy.json
aifilesorter taxonomy import taxonomy.json
```

## `policy` — planned

Work with `ORGANIZE.md`.

```powershell
aifilesorter policy validate ORGANIZE.md
aifilesorter policy explain "C:\some\file.pdf"
```

`explain` should show which inherited rules apply to the target.

## `export` — planned

Examples:

```powershell
aifilesorter export index --format json --output index.json
aifilesorter export index --format csv --output index.csv
aifilesorter export duplicates --output duplicates.json
```

Exports must avoid leaking file contents unless the user explicitly requests content fields.

## `db` — planned developer/admin surface

Examples:

```powershell
aifilesorter db status
aifilesorter db compact
aifilesorter db migrate
aifilesorter db integrity-check
```

Direct arbitrary SQL is not part of the normal user contract. If later exposed, it should be clearly marked developer-only.

## `doctor` — planned

Diagnostics for:

- database access;
- OCR runtime availability;
- PDF backend;
- local LLM runtime;
- GPU backend;
- permissions;
- policy parsing;
- index schema;
- filesystem capabilities.

Example:

```powershell
aifilesorter doctor --json
```

## Output contract

Human output is useful interactively; JSON is the automation contract.

Example index result:

```json
{
  "kind": "aifs.personalIndexResult",
  "status": "completed",
  "runId": 42,
  "filesIndexed": 82417,
  "directoriesIndexed": 9128,
  "protectedProjects": 14,
  "skipped": 3021,
  "errors": 7,
  "database": "personal_file_index.db"
}
```

Stable machine output should include a `kind`/schema discriminator so future versions can evolve without ambiguous parsing.

## Permissions

The CLI inherits the permissions of the process that launches it.

Normal PowerShell:

```text
normal user access
```

Administrator PowerShell:

```text
elevated access where Windows permits it
```

The application must never attempt to bypass Windows ACLs, UAC, encryption, or other operating-system security mechanisms.

## Future agent/MCP use

The machine-readable CLI and shared core are intended to support a later agent layer.

Preferred architecture:

```text
Agent/MCP
   ↓ structured request
shared organizer services
   ↓
index / inspect / plan / review / apply
```

Agents should not automate unrestricted filesystem mutation. They should operate through the same plan/review/audit boundaries as human-facing interfaces.
