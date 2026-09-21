# Personal AI File Organizer — CLI Contract

This document defines the command-line and headless interface of my fork.

The GUI remains the normal everyday interface. The CLI is the **power-user and automation surface**: it should expose the same core services without creating a second implementation of organizer logic. Some diagnostics, batch controls, and machine-readable operations may intentionally remain CLI-only.

## Status legend

Every command is labelled so this document does not present designs as finished functionality.

- **Implemented upstream** — already provided by Hyperfield AI File Sorter.
- **Implemented in fork** — available in my personal-organizer code.
- **In progress** — code exists but the public contract is not ready to rely on.
- **Planned** — design direction only.

## Core rules

1. GUI and CLI call the same core services.
2. Analysis comes before mutation.
3. Source-filesystem read-only commands may still write application state, caches, logs, or the local index database.
4. JSON output is a first-class machine contract.
5. Commands use meaningful exit codes.
6. Partial/inaccessible scans are reported instead of being disguised as complete scans.
7. Windows permissions are respected; the organizer does not bypass ACLs or security boundaries.
8. Model output is data, not authorization to mutate files.
9. Mutation must go through the appropriate plan/review/apply/audit boundary.
10. A future agent should prefer CLI/core contracts over visually driving the GUI when an equivalent supported command exists.

---

## Existing upstream headless interface

**Status: Implemented upstream.**

Hyperfield already provides headless categorization/renaming and approved-review application. The authoritative upstream contract remains [`headless-runtime-contract.md`](headless-runtime-contract.md).

Typical upstream forms include:

```text
aifilesorter --headless --operation <categorize|rename|categorize-and-rename> --path <path> ...
aifilesorter --headless-apply --review-file <review-plan.json> ...
```

My command family is added alongside this behavior rather than replacing it.

The parsers are intentionally independent: the upstream parser only activates for explicit `--headless*` request flags, while my personal index parser activates for the `index` command.

---

# Implemented fork commands

## `index`

**Status: Implemented in fork (Phase 1).**

`index` updates my persistent `personal_file_index.db` inventory without moving, renaming, deleting, or editing the files being scanned.

The command does **not** require an LLM. It is routed before GUI startup and before LLM-selection logic.

### Syntax

One or more positional roots:

```powershell
aifilesorter index "C:\Users\Hp\Documents" --json
```

```powershell
aifilesorter index `
  "C:\Users\Hp\Documents" `
  "D:\Books" `
  --json
```

Repeatable `--root` form:

```powershell
aifilesorter index `
  --root "C:\Users\Hp\Documents" `
  --root "C:\Users\Hp\Downloads" `
  --json
```

The two forms may be combined.

### Implemented options

```text
--root <path>               add a scan root; repeatable
--json                      emit one machine-readable result object
--hash <off|all>            SHA-256 policy; default is off
--include-hidden            include hidden entries
--follow-reparse-points     follow symlinks/reparse points; off by default
--project-root-only         mark protected projects but do not index their contents
--no-project-protection     disable protected-project detection for this scan
--help, -h                  show command help
```

Not implemented yet:

```text
--hash smart
--include <pattern>
--exclude <pattern>
--status-file <path>
--database <path>
```

Do not rely on those planned forms until they are implemented and tested.

### Default project behavior

The default is deliberately different from the first prototype.

A recognized Git/Node/Python/etc. project is marked as a **protected project** so later organization planning knows its relative structure must not be casually rearranged. The read-only index may still traverse useful source files inside it.

Generated/internal directories covered by the normal exclusion rules remain skipped, including examples such as:

```text
.git
node_modules
.next
.venv
venv
__pycache__
.cache
.idea
```

Therefore:

```text
protected from generic filesystem mutation
!=
hidden from the read-only knowledge index
```

`--project-root-only` restores the conservative behavior of indexing only the recognized project root.

`--no-project-protection` is an advanced indexing option; it removes project protection metadata/detection for that scan. It is not permission for future mutation code to ignore unrelated safety rules.

### Hash modes

Current contract:

```text
off   do not calculate new SHA-256 hashes during the scan
all   hash each indexed regular file
```

`off` is the default because hashing an entire drive can create substantial unnecessary I/O.

If a previously indexed file already has a complete hash, a later metadata-only scan preserves that complete hash rather than erasing it.

`smart` remains planned for later duplicate/identity work.

### Presence semantics

The index distinguishes absence from inability to observe.

For a **complete rescan** of a root:

```text
previously indexed entry not seen now
→ mark it not present/stale
```

For a **missing, inaccessible, or incomplete root**:

```text
root could not be observed reliably
→ preserve previous presence state
→ record the run as partial
```

This prevents a disconnected drive, permission problem, or failed traversal from being misinterpreted as mass deletion.

### System-path safety

Windows system roots are detected by their actual paths/environment roots rather than by globally blacklisting ordinary folder names.

For example, a user-owned path such as:

```text
D:\Archive\Windows\notes.txt
```

must not be excluded merely because one directory happens to be called `Windows`.

Actual protected Windows/system locations are refused before traversal. Whole-drive use is still governed by the Phase 1 validation status documented in the roadmap/development log.

### JSON result

With `--json`, stdout contains a compact JSON object with the stable kind:

```json
{
  "kind": "aifs.personalIndexResult",
  "status": "completed",
  "runId": 42,
  "filesIndexed": 82417,
  "directoriesIndexed": 9128,
  "protectedProjects": 14,
  "skipped": 201,
  "errors": 0,
  "database": ".../personal_file_index.db",
  "roots": ["D:/Books"],
  "warnings": [],
  "stats": {
    "totalEntries": 91559,
    "presentEntries": 91559,
    "presentFiles": 82417,
    "presentDirectories": 9128,
    "presentProtectedProjects": 14,
    "staleEntries": 0,
    "hashedFiles": 0,
    "presentBytes": 123456789,
    "latestRunId": 42,
    "latestRunStatus": "completed",
    "latestRunErrors": 0
  }
}
```

Counts under the top level describe the **current scan run**. `stats` describes the persisted index after the scan.

Possible `status` values currently include:

```text
completed
partial
failed
```

### Exit codes

The implemented personal index command uses:

```text
0   success
1   operation failed
2   invalid usage
3   busy / shared runtime lock already held
5   completed with partial access/errors
```

The command participates in the same `AnalysisRuntimeLock` used by the GUI/headless workflows. It does not start a competing index job when another protected AI File Sorter job holds the lock.

### Windows packaged launcher

The Windows starter recognizes `index` as a synchronous command invocation. It forwards the command to the main executable, keeps the console path available for output, waits for the result, and does not require CUDA/Vulkan/GGML discovery because indexing itself does not use an LLM.

This keeps a metadata-only command independent from GPU/model readiness.

### Read-only boundary

“Read-only index” means read-only **with respect to scanned source files**.

The command is allowed to write only organizer-owned state such as:

```text
personal_file_index.db
SQLite WAL/support files
runtime-lock metadata
normal application logs/state
```

It may not:

```text
rename scanned files
move scanned files
delete scanned files
rewrite scanned documents
modify project contents
silently fix permissions
```

---

# Planned command families

The longer-term top-level model is:

```text
aifilesorter <command> [options]
```

with command families such as:

```text
index        implemented
search       planned
inspect      planned
extract      planned
ocr          planned
duplicates   planned
policy       planned
plan         planned
review       planned fork command
apply        planned fork command
undo         planned fork command
history      planned fork command
db           planned
doctor       planned
```

## `search`

**Status: Planned.**

Search filenames, metadata, extracted text, bibliographic fields, and later semantic signals.

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

Deterministic matches and semantic/model-derived matches should remain distinguishable.

## `inspect`

**Status: Planned.**

Show everything the organizer currently knows about one file/folder without changing it.

```powershell
aifilesorter inspect "D:\Books\book.pdf" --json
```

Possible output fields include identity/hash state, extraction/OCR state, language, summary, bibliography, relationships, project membership, and policy classification.

## `extract`

**Status: Planned.**

Run deterministic document extraction and persist/cache the result.

```powershell
aifilesorter extract "D:\Books" --only-missing
```

This should reuse upstream document-reading capabilities rather than creating a separate parser stack.

## `ocr`

**Status: Planned.**

OCR documents that lack sufficient embedded text.

```powershell
aifilesorter ocr "D:\Books\Arabic" `
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

`identify` should mean strategic-page OCR first, not blindly OCRing every page of every PDF.

## `duplicates`

**Status: Planned.**

Early versions report duplicate candidates; they do not auto-delete them.

```powershell
aifilesorter duplicates --root "C:\Users\Hp" --exact --json
```

Potential modes:

```text
--exact
--near-images
--near-documents
--minimum-size <bytes>
--report <file>
```

## `policy`

**Status: Planned.**

Validate and explain `ORGANIZE.md` policy without changing files.

```powershell
aifilesorter policy validate ORGANIZE.md
```

```powershell
aifilesorter policy explain "C:\Users\Hp\Downloads\some-file.pdf"
```

Expected subcommands:

```text
policy validate
policy print
policy explain
policy test
```

Policy errors must fail before a later planning/apply operation.

## `plan`

**Status: Planned.**

Generate an organization plan without applying it.

```powershell
aifilesorter plan `
  --root "C:\Users\Hp\Downloads" `
  --policy ORGANIZE.md `
  --output organization-plan.json
```

A plan should record proposed paths/names, evidence, reasons, confidence, warnings, relationships, and policy decisions.

## `review`

**Status: Planned as a personal-organizer command; upstream already has review-plan concepts.**

```powershell
aifilesorter review organization-plan.json
```

Machine validation may later use:

```powershell
aifilesorter review organization-plan.json --check --json
```

## `apply`

**Status: Planned personal-organizer command; upstream already has `--headless-apply`.**

Apply an already generated and approved plan.

```powershell
aifilesorter apply organization-plan.json
```

An `apply` operation must not silently regenerate its approved plan with fresh model output.

Required safeguards include:

```text
validate plan schema
validate source identity/current state
validate conflicts
validate policy
acquire runtime lock
apply
write audit record
return per-entry result
```

A future `--force` must be narrow; it is never a permission to bypass Windows ACLs or arbitrary safety boundaries.

## `undo` and `history`

**Status: Planned fork CLI; upstream already has persistent undo/review-history infrastructure.**

Possible forms:

```powershell
aifilesorter history --limit 20
```

```powershell
aifilesorter undo --plan-id 123
```

Undo should accurately report anything that can no longer be reversed.

## `db`

**Status: Planned.**

Power-user index/database maintenance.

Possible commands:

```text
db stats
db check
db vacuum
db export
db stale
db reset-analysis
```

The current `PersonalFileIndexQuery` already provides a read-only statistics facade internally; the public `db stats` command has not been added yet.

Arbitrary SQL is not intended as the normal user interface.

## `doctor`

**Status: Planned.**

Readiness/diagnostic report without changing source files.

Potential checks:

```text
application/build version
personal index health
configured LLM
GPU/runtime availability
OCR engines/language packs
PDF extraction support
filesystem permissions
ORGANIZE.md policy status
upstream/fork build metadata
```

---

# Safety/effect classes

Commands should be classified by effect:

```text
SOURCE-FILESYSTEM READ ONLY
  index, search, inspect, extract, ocr, duplicates, policy validate, plan, doctor

DATABASE MAINTENANCE
  selected db commands

FILESYSTEM MUTATION
  apply, undo

DEVELOPER / EXPERIMENTAL
  raw traversal, diagnostics, test hooks
```

Extraction/OCR may write organizer-owned cache/index data while remaining read-only toward source documents.

## Windows privilege model

The CLI inherits the privileges of the process that launched it.

```text
normal PowerShell       → normal user access
Administrator PowerShell → elevated user access
```

The application may report inaccessible paths. It must not attempt to defeat Windows security.

## Output rules

Human output is for interactive use. JSON is for automation.

Machine output should use stable `kind` identifiers, explicit statuses, and predictable types. Diagnostics belong on stderr when stdout is intended to remain parseable.

## Agent usage

Future agents should call supported machine-readable commands/core APIs instead of controlling the GUI visually.

Preferred model:

```text
agent asks core for facts
→ core returns structured result
→ agent generates/proposes a plan
→ policy/user approval is obtained where required
→ core applies
→ audit/undo state is recorded
```

Agents never receive a hidden filesystem-mutation bypass.
