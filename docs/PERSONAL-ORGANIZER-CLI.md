# Personal AI File Organizer — CLI Contract

This document defines the command-line and headless interface of my fork.

The CLI is the **canonical product surface** for human control, automation, scripting, diagnostics, and long-running jobs. The GUI is a secondary client over the same core services. Some diagnostics, batch controls, and machine-readable operations may intentionally remain CLI-only.

If an important capability cannot be invoked safely through the CLI/core contract, I do not consider it fully implemented in this fork.

## Status legend

Every command is labelled so this document does not present designs as finished functionality.

- **Implemented upstream** — already provided by Hyperfield AI File Sorter.
- **Implemented in fork** — available in my personal-organizer code.
- **In progress** — code exists but the public contract is not ready to rely on.
- **Planned** — design direction only.

## Core rules

1. CLI, GUI, and future agents call the same core services.
2. Analysis comes before mutation.
3. Source-filesystem read-only commands may still write organizer-owned state, caches, logs, checkpoints, or local indexes.
4. JSON/JSONL output is a first-class machine contract.
5. Commands use meaningful stable exit codes.
6. Partial/inaccessible observations are reported instead of being disguised as complete scans.
7. Windows permissions are respected; the organizer does not bypass ACLs or security boundaries.
8. Model output is evidence/inference, not authorization to mutate files.
9. Mutation must go through plan/validation/simulation/review/apply/audit boundaries.
10. Long-running work should eventually be detachable from the terminal through the durable job runtime.
11. Explicit CLI flags, natural-language requests, policy, profiles/defaults, and inferred assumptions resolve into the same typed `JobSpec` model.
12. Machine mode must remain parseable: no progress/ANSI noise on stdout when structured output is requested.

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

The parsers are intentionally independent: the upstream parser only activates for explicit `--headless*` request flags, while the personal command parser activates for fork subcommands such as `index`.

---

# Implemented fork commands

## `index`

**Status: Implemented in fork (Phase 1), still under production Windows validation.**

`index` updates the persistent `personal_file_index.db` inventory without moving, renaming, deleting, or editing the files being scanned.

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

The two forms may be combined, but duplicate or overlapping ancestor/descendant roots are rejected because Phase 1 does not yet implement many-to-many root ownership.

### Implemented options

```text
--root <path>               add a scan root; repeatable
--json                      emit one machine-readable result object
--hash <off|all>            SHA-256 policy; default is off
--include-hidden            include hidden entries
--project-root-only         mark protected projects but do not index their contents
--no-project-protection     disable protected-project detection for this scan
--help, -h                  show command help
```

### Recognized but deliberately unavailable in Phase 1

```text
--follow-reparse-points
```

Reparse/symlink following is currently **fail-closed**. The option is rejected until bounded traversal, cycle detection, root-boundary semantics, and regression tests exist. It must not silently enable unbounded traversal.

Not implemented yet:

```text
--hash smart
--include <pattern>
--exclude <pattern>
--status-file <path>
--database <path>
--metadata-only
```

Do not rely on planned forms until they are implemented and tested.

### Default project behavior

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

`--project-root-only` indexes only the recognized project root.

`--no-project-protection` is an advanced indexing option; it removes project protection metadata/detection for that scan. It is not permission for future mutation code to ignore unrelated safety rules.

### Hash modes

Current contract:

```text
off   do not calculate new SHA-256 hashes during the scan
all   hash each indexed regular file
```

`off` is the default because hashing an entire drive can create substantial unnecessary I/O.

If a previously indexed file already has a complete hash, a later metadata-only scan preserves that complete hash rather than erasing it.

`smart` remains planned for later duplicate/content-identity work. Long-term identity is expected to use staged file identity/fingerprint logic rather than full-hashing every file during discovery.

### Observation and policy semantics

The index distinguishes physical observation from scan policy.

Conceptually:

```text
observation_state
  present / missing / unknown

policy_state
  included / hidden / excluded / system / reparse-skipped / protected / ...
```

A file omitted because `--include-hidden` changed is not automatically a deleted file. A missing/inaccessible root preserves prior trustworthy physical knowledge as unknown/unchanged rather than manufacturing mass deletion.

### System-path safety

Windows system roots are detected by actual paths/environment roots rather than by globally blacklisting ordinary folder names.

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

Unicode root arguments are decoded from the native wide Windows command line rather than depending on the active local code page.

A full native MSVC/vcpkg build plus packaged-launcher smoke test remains a Phase 1 exit gate before whole-drive use is approved.

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
aifs <command> [options]
```

The `aifs` name is the long-term product shorthand; current builds still use the upstream executable naming unless/until packaging is changed deliberately.

Planned command families include:

```text
index             implemented foundation
job               planned
daemon            planned
snapshot          planned
diff              planned
reconcile         planned
estimate          planned
search            planned
inspect           planned
extract           planned
ocr               planned
related           planned
duplicates        planned
policy            planned
plan              planned
review            planned fork command
apply             planned fork command
undo              planned fork command
history           planned fork command
models            planned
components        planned
db                planned
doctor            planned
explain-request   planned
shell             planned
```

## `job`

**Status: Planned. Tracking: Issue #6.**

Long jobs become durable entities rather than terminal-bound function calls.

Target forms:

```powershell
aifs job list
aifs job status 194
aifs job tail 194
aifs job pause 194
aifs job resume 194
aifs job cancel 194
```

`resume` means continuing from persisted tasks/checkpoints, not merely launching a new scan that reuses some rows.

## `daemon`

**Status: Planned. Tracking: Issue #6.**

Potential durable organizer runtime for long work:

```powershell
aifs daemon start
aifs daemon status
aifs daemon stop
```

The exact service/Windows-service packaging is not fixed yet. The invariant is that hours of completed work should not disappear merely because a shell closes.

## `snapshot`, `diff`, and `reconcile`

**Status: Planned. Tracking: Issue #8.**

Represent trustworthy observation epochs and provider reconciliation.

```powershell
aifs snapshot create D:\
aifs snapshot list
aifs diff snapshot:123 snapshot:124
aifs reconcile D:\ --provider everything --against native
```

Provider disagreement/gaps become explicit reconciliation work rather than automatic deletion.

## `estimate`

**Status: Planned.**

Estimate the cost/scope of expensive work before starting it.

```powershell
aifs estimate ocr D:\Library
aifs estimate analyze D:\Archive --json
```

Potential estimates:

```text
entries
bytes likely to read
hash candidates
OCR pages/documents
cache growth
local compute tasks
cloud tokens/cost if enabled
```

Future hard budgets may include `--max-read`, `--max-pages`, `--max-runtime`, `--max-cache-growth`, `--max-cloud-cost`, and `--max-cloud-tokens`.

## `search`

**Status: Planned.**

Search filenames, metadata, extracted text, OCR, bibliographic fields, relationships, and later semantic signals.

Examples:

```powershell
aifs search "الإمام النووي"
```

```powershell
aifs search "Flutter PDF outline" --json
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

Results should expose why they matched: path, metadata, exact text/OCR, bibliography, relationship graph, or semantic similarity. Deterministic and semantic evidence remain distinguishable.

## `inspect`

**Status: Planned.**

Show everything the organizer currently knows about one file/folder without changing it.

```powershell
aifs inspect "D:\Books\book.pdf" --json
```

Possible output fields include physical/file identity, hash state, extraction/OCR state, language, summary, bibliography, relationships/lineage, project membership, policy/privacy classification, and provenance.

## `extract`

**Status: Planned.**

Run deterministic document/metadata extraction and persist/cache the result.

```powershell
aifs extract "D:\Books" --only-missing
```

This should reuse upstream document-reading capabilities and delegated metadata workers rather than creating a competing parser stack.

## `ocr`

**Status: Planned.**

OCR documents that lack sufficient embedded text.

```powershell
aifs ocr "D:\Books\Arabic" `
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

`identify` means strategic-page OCR first, not blindly OCRing every page of every PDF.

## `related`

**Status: Planned.**

Inspect evidence-backed relationships/lineage:

```powershell
aifs related "D:\Books\book.pdf"
```

Potential relation types include edition, translation, volume, annotation, OCR derivative, export, project membership, supplement, exact duplicate, and possible near duplicate.

## `duplicates`

**Status: Planned.**

Early versions report duplicate candidates; they do not auto-delete them.

```powershell
aifs duplicates --root "C:\Users\Hp" --exact --json
```

Potential modes:

```text
--exact
--near-images
--near-documents
--minimum-size <bytes>
--report <file>
```

Hardlinks, derivatives, editions, translations, and exact duplicate bytes are distinct concepts.

## `policy`

**Status: Planned. Tracking: Issue #7.**

Validate, test, diff, and explain structured `ORGANIZE.md` policy without changing files.

```powershell
aifs policy validate ORGANIZE.md
aifs policy lint ORGANIZE.md
aifs policy test ORGANIZE.md
aifs policy diff old/ORGANIZE.md new/ORGANIZE.md
aifs policy explain "C:\Users\Hp\Downloads\some-file.pdf"
```

Policy is parsed into structured rules/AST rather than pasted into an opaque model prompt. Policy errors must fail before a later planning/apply operation.

## `explain-request`

**Status: Planned. Tracking: Issue #7.**

Show how natural-language intent, CLI flags, profiles/defaults, policy, and prior answers resolved into the typed `JobSpec` before expensive work begins.

```powershell
aifs explain-request <job-or-draft>
```

The output should make inferred assumptions and unresolved high-impact questions explicit.

## `plan`

**Status: Planned.**

Generate an organization plan without applying it.

```powershell
aifs plan `
  --root "C:\Users\Hp\Downloads" `
  --policy ORGANIZE.md `
  --output organization-plan.json
```

A plan records proposed paths/names/actions, source identity, requirements, policy rules, evidence, reasons, confidence, relationships, unresolved questions, and model/worker provenance. It also carries schema/config/JobSpec/policy identity needed for reproducibility and freshness validation.

## `review`

**Status: Planned as a personal-organizer command; upstream already has review-plan concepts.**

```powershell
aifs review organization-plan.json
```

Machine validation may later use:

```powershell
aifs review organization-plan.json --check --json
```

Interactive review may use a terminal UI later, but the underlying plan remains a structured artifact.

## `apply`

**Status: Planned personal-organizer command; upstream already has `--headless-apply`.**

Apply an already generated and approved plan.

```powershell
aifs apply organization-plan.json
```

An `apply` operation must not silently regenerate its approved plan with fresh model output.

Required safeguards include:

```text
validate plan schema/version
validate source identity/current state
validate destination conflicts
validate policy/answers
simulate paths/collisions/space/relationships
acquire mutation/runtime lock
apply recoverable chunks
verify cross-volume copies before authorized source removal
write audit checkpoints/results
return per-entry result
```

A future `--force` must be narrow; it is never permission to bypass Windows ACLs or arbitrary safety boundaries.

## `undo` and `history`

**Status: Planned fork CLI; upstream already has persistent undo/review-history infrastructure.**

Possible forms:

```powershell
aifs history --limit 20
aifs undo --plan-id 123
```

Undo should accurately report anything that can no longer be reversed. The tool must not claim global transaction guarantees the filesystem cannot provide.

## `models`

**Status: Planned.**

Inspect and benchmark local/remote model capabilities on representative tasks.

Potential commands:

```powershell
aifs models list
aifs models doctor
aifs models benchmark
```

Model selection should eventually use measured task quality, latency, RAM/VRAM, privacy, and cost rather than generic leaderboard reputation alone.

## `components`

**Status: Planned.**

Inspect optional providers/workers and where they are found.

```powershell
aifs components list
aifs components doctor
aifs components paths
```

The organizer should not silently install huge runtimes/models or modify another application's configuration.

## `db`

**Status: Planned.**

Power-user control-plane/index maintenance.

Possible commands:

```text
db stats
db check
db backup
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
control DB/index health
Everything/native provider health and capabilities
provider cursors/reconciliation state
configured local/remote LLMs
GPU/runtime availability
OCR engines/language packs
PDF extraction support
filesystem permissions
policy status
optional workers/components
cache/search-index health
upstream/fork build metadata
```

Each optional capability should report a state such as healthy, missing, unsupported, degraded, misconfigured, or version-incompatible.

## `shell`

**Status: Planned.**

Optional UTF-8 interactive REPL for advanced sessions. It must still compile interactions into structured commands/JobSpecs rather than creating a hidden conversational bypass around policy and safety.

---

# Safety/effect classes

Commands should be classified by effect:

```text
SOURCE-FILESYSTEM READ ONLY
  index, snapshot, diff, reconcile, estimate, search, inspect,
  extract, ocr, related, duplicates, policy, plan, doctor, explain-request

ORGANIZER STATE / CONTROL
  job, daemon, models, components, selected db commands

FILESYSTEM MUTATION
  apply, undo

DEVELOPER / EXPERIMENTAL
  raw traversal, diagnostics, benchmarks, fault/test hooks
```

Extraction/OCR may write organizer-owned cache/index data while remaining read-only toward source documents.

## Windows privilege model

The CLI inherits the privileges of the process that launched it.

```text
normal PowerShell        → normal user access
Administrator PowerShell → elevated user access
```

The application may report inaccessible paths. It must not attempt to defeat Windows security.

## Output rules

Human output is for interactive use. JSON/JSONL is for automation.

Machine output should use versioned stable `kind` identifiers, explicit statuses, predictable types, and documented exit codes. Diagnostics/progress belong on stderr or an event channel when stdout is intended to remain parseable.

Totals/progress must say when they are estimated or unknown rather than inventing a percentage.

## Budgets and control

Long-term commands may accept cross-cutting controls such as:

```text
--offline
--local-only
--max-read
--max-pages
--max-runtime
--max-cache-growth
--max-cloud-cost
--max-cloud-tokens
--io-jobs
--cpu-jobs
--gpu-jobs
--read-rate
--cloud-concurrency
--idle-only
--pause-on-battery
--profile <name>
--explain
--why
```

These remain planned until implemented and tested. Profiles are inspectable convenience bundles, not secret behavior.

## Agent usage

Future agents should call supported machine-readable commands/core APIs instead of controlling the GUI visually.

Preferred model:

```text
agent asks core for facts
→ core returns structured result
→ intent/policy/questions are resolved
→ core generates a plan
→ user/policy approval is obtained where required
→ core validates/simulates/applies
→ audit/undo state is recorded
```

Agents never receive a hidden filesystem-mutation bypass.
