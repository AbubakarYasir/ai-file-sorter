# Personal Organizer — System Quality Bar

This document defines what I mean by an **amazing, serious CLI-first filesystem intelligence system**.

The target is not “AI File Sorter plus more commands.” The target is a durable local systems tool that I can trust with multi-million-entry / 8TB+ storage, run for hours or days, use almost entirely offline, optionally connect to strong online models, and control precisely without allowing AI guesses to become filesystem authority.

This quality bar is architectural. A feature is not complete merely because it works on a small folder once.

Related work:

- Issue #2 — Phase 1 filesystem state/index foundation
- Issue #3 — CLI-first/offline-first/multi-terabyte architecture epic
- Issue #4 — EverythingProvider
- Issue #5 — dependency/delegation strategy
- Issue #6 — durable daemon/job/checkpoint/resource engine
- Issue #7 — intent/questions/policy/explainability
- Issue #8 — 8TB scale/benchmark/fault-injection quality bar

---

## 1. Fast discovery and deep understanding are different products stages

The system must not equate “index my drive” with “read, OCR, hash, embed, and LLM-analyze every byte.”

The default intelligence ladder is progressive:

```text
locate/discover
→ physical metadata
→ deterministic metadata extraction
→ cheap content extraction
→ selective OCR
→ lexical/full-text indexing
→ relationships/bibliography
→ embeddings when useful
→ LLM reasoning only where useful
```

A fast 8TB inventory should be possible without committing to 8TB of expensive analysis.

This creates two major benefits:

1. the system becomes useful quickly;
2. expensive understanding can be driven by user goals, priority, and budget instead of blind completeness.

Example:

```text
7,800,000 files discovered
5,100,000 need no deep analysis
1,900,000 have deterministic metadata only
600,000 contain useful extractable text
130,000 appear to need OCR
12,000 are currently relevant to the user's active request
```

Only the smallest justified subset should reach the most expensive stages.

---

## 2. Canonical truth must be separated from derived intelligence

Do not collapse these states into one “index row.”

```text
Physical truth
  what was actually observed on a filesystem/provider

Policy truth
  whether the item is in scope, protected, excluded, local-only, etc.

Deterministic derived data
  hashes, parser metadata, embedded text, OCR output

Probabilistic/inferred data
  language, category, bibliography, relationships, semantic meaning

User intent
  explicit requirements, answers, preferences, constraints

Plan state
  proposed actions that have not happened yet

Audit state
  what was actually changed and with which pre/postconditions
```

An unavailable provider, skipped path, failed worker, uncertain model, or changed policy must never silently rewrite physical truth.

---

## 3. Observation epochs, snapshots, and reconciliation

At large scale, “seen this scan / not seen this scan” is too weak.

Each trustworthy inventory should have an observation epoch/snapshot identity with:

```text
root/volume identity
provider identity/version
provider cursor/journal position
start/end time
completeness state
coverage/capabilities
errors/gaps
```

Useful future commands:

```powershell
aifs snapshot create D:\
aifs snapshot list
aifs diff snapshot:123 snapshot:124
aifs reconcile D:\ --provider everything --against native
```

Deletion should only be concluded after a sufficiently trustworthy observation/reconciliation, not merely because one provider failed to return an item.

---

## 4. Stable file and volume identity

Paths are names, not perfect identities.

Where the platform supports it, track:

```text
volume identity
platform file ID
logical path
normalized comparison key
hardlink identity
size/timestamps
content fingerprint state
```

This matters for:

- rename/move detection;
- removable-drive reconnects;
- hardlinks vs duplicate bytes;
- avoiding repeated analysis after harmless renames;
- validating a plan immediately before apply.

Windows-specific edge cases must be deliberate, including long paths, case behavior, junctions, mount points, sparse files, compressed/encrypted files, alternate/reparse behaviors, and cloud placeholders.

The organizer should avoid accidentally hydrating cloud-placeholder files merely to index or analyze them unless policy explicitly permits that I/O cost.

---

## 5. Long jobs belong to a durable runtime, not a terminal process

For serious long-running work, the CLI should eventually be a client of a durable organizer runtime/daemon.

Conceptually:

```text
PowerShell / shell
      │
     aifs
      │
      ▼
 durable organizer service
      │
 ┌────┼─────────────┐
 │    │             │
DB   worker pool   providers
```

Potential surfaces:

```powershell
aifs daemon start
aifs daemon status
aifs job list
aifs job status 194
aifs job tail 194
aifs job pause 194
aifs job resume 194
aifs job cancel 194
```

The exact service shape is not fixed yet, but the invariant is: closing a shell must not destroy hours of completed work.

Persist tasks, attempts, leases, checkpoints, questions, answers, events, provider cursors, and worker/model versions.

---

## 6. Resource-aware scheduling and backpressure

An 8TB organizer can be destructive to usability even without modifying a single file if it saturates disks for hours.

Treat these as different resource classes:

```text
filesystem metadata I/O
sequential reads
random hash reads
CPU extraction
GPU OCR
CPU/GPU local LLM
network/cloud API
search-index writes
```

Queues must be bounded and apply backpressure.

Scheduler policy should eventually understand physical-device classes and user activity:

```text
HDD       low random-read concurrency
SATA SSD  moderate concurrency
NVMe      higher concurrency
battery   conservative/pause policy
active PC lower background budget
idle PC   optional higher throughput
```

Useful controls:

```text
--io-jobs
--cpu-jobs
--gpu-jobs
--read-rate
--cloud-concurrency
--idle-only
--pause-on-battery
--schedule-window
```

No default should turn the machine unusable merely to finish sooner.

---

## 7. Every expensive job should have an estimate and a budget

Before launching a huge analysis, estimate what is reasonably knowable:

```text
entries affected
bytes likely to be read
files likely to hash
pages likely to OCR
GPU/CPU task count
estimated cloud tokens/cost
estimated cache growth
```

Useful future flow:

```powershell
aifs estimate ocr D:\Library
```

```text
Documents considered:       148,220
Likely scanned PDFs:         31,480
Strategic pages first:       94,440 pages
Estimated full-OCR fallback: 4,200 documents
Cloud usage:                 forbidden
Estimated derived cache:     38–62 GB
```

Then allow hard budgets such as:

```text
--max-read
--max-pages
--max-cloud-cost
--max-cloud-tokens
--max-runtime
--max-cache-growth
```

When a budget is exhausted, checkpoint cleanly instead of improvising.

---

## 8. Requirements compile into a typed JobSpec

Natural language is useful input, but it is not an execution format.

Both this:

```powershell
aifs plan D:\Downloads --policy ORGANIZE.md --offline
```

and this:

```powershell
aifs organize "clean my old downloads, keep projects intact, ask me about uncertain books, never use cloud"
```

should compile into the same typed requirement model.

Important fields preserve their source:

```text
explicit CLI
explicit user language
policy
profile/default
previous answer
inference
```

Precedence must be explicit and inspectable.

The system should be able to print the interpreted job before running it:

```powershell
aifs explain-request <job-or-draft>
```

---

## 9. Questions are a first-class workflow primitive

The organizer should be intelligent enough to ask, but disciplined enough not to annoy constantly.

Question priority should consider:

```text
filesystem impact
scope
privacy
cost
confidence
reversibility
number of affected files
```

Low-impact ambiguity can use explicit policy/defaults. High-impact ambiguity becomes a persisted question.

Related questions should be batched.

Independent work should continue while only affected tasks wait in `needs_user`.

Answers can be scoped deliberately:

```text
this item only
this job
this session/profile
promote to ORGANIZE.md/policy
```

---

## 10. Policy is code-like configuration and must be testable

`ORGANIZE.md` is not merely prose injected into a prompt.

It should compile into a policy AST / structured rules with precedence and validation.

Target commands:

```powershell
aifs policy validate
aifs policy lint
aifs policy explain <path>
aifs policy test
aifs policy diff old new
```

Policy tests should use fixtures:

```text
input item/context
expected protection/privacy/category/naming decision
```

A policy edit that would unexpectedly affect thousands of files should be visible before a new plan is accepted.

---

## 11. Progressive intelligence and model routing

LLMs should be the last useful intelligence layer, not the first reflex.

Routing order should prefer:

```text
exact deterministic fact
→ cheap local classifier/rules
→ stronger local model
→ optional online model when policy permits and expected value justifies it
```

The model router considers:

- task capability;
- privacy;
- local hardware;
- confidence from cheaper stages;
- latency;
- context limits;
- user cost budget;
- whether cloud is allowed for the specific path/content.

Offline is not degraded mode. It is the baseline product.

---

## 12. Model quality should be benchmarked on my actual workloads

Do not choose models from generic leaderboards alone.

Maintain task-oriented evaluation fixtures, especially for:

```text
Arabic Islamic book identification
Arabic/Urdu OCR cleanup
bibliographic extraction
English development/project files
mixed-language classification
file relationship decisions
organization-plan reasoning
```

`aifs models benchmark` should eventually measure quality, latency, VRAM/RAM use, and tokens/cost on representative fixtures.

The router can then choose based on measured local performance instead of marketing names.

---

## 13. Search should fuse evidence instead of hiding it

Search results can combine:

```text
filename/path match
structured metadata
exact extracted text
OCR text
bibliographic fields
relationship graph
semantic/vector similarity
```

But the UI/CLI should explain *why* a result matched.

Example:

```text
Result: ...\ثعلبة بن حاطب\notes.md
Match:
  exact OCR/text phrase      0.94
  filename topic             0.73
  semantic similarity        0.88
```

Deterministic and semantic evidence must remain distinguishable.

---

## 14. Relationships should form an evidence-backed graph

A flat category is insufficient for real personal storage.

Store typed relationships such as:

```text
is_edition_of
is_translation_of
is_volume_of
is_annotation_of
is_ocr_derivative_of
is_export_of
belongs_to_project
uses_asset
has_supplement
is_exact_duplicate_of
is_possible_near_duplicate_of
```

Every edge can retain evidence, confidence, and source.

This allows planning at the correct unit: a book collection, design project, course bundle, or software repo instead of independent files.

---

## 15. Content lineage prevents bad duplicate decisions

An OCR derivative, compressed export, thumbnail, translated edition, source PSD, and rendered JPG may share content without being disposable duplicates.

Track lineage separately from duplicate identity.

Exact duplicate means proven same content bytes/identity according to the chosen method.

Near duplicate / derivative / related are different claims and require different user choices.

---

## 16. Evidence and provenance are mandatory for inferred facts

Any important inferred field should be explainable.

Conceptual provenance:

```text
title = "..."
source = OCR title page
page = 3
worker = paddleocr@x.y
model = ...
confidence = 0.96
normalization = arabic-v1
```

For model reasoning:

```text
provider/model/version
prompt/template version
input evidence IDs
policy version
JobSpec hash
output confidence/validation state
```

This is what makes `aifs why` useful rather than decorative.

---

## 17. Plans should be reproducible artifacts

An `OrganizationPlan` should carry enough identity to answer:

- what exact source state was this based on?
- which policy version?
- which user requirements/answers?
- which evidence?
- which models/workers?
- what assumptions remain?
- has anything changed since planning?

A plan should have a stable schema/version and input/config hashes.

Fresh model output must not silently replace an already approved plan during apply.

---

## 18. Simulate before apply

Before touching the real filesystem, run the plan through a virtual/overlay filesystem model.

Detect:

```text
path collisions
case-only collisions
Windows-invalid names
path-length problems
cross-volume operations
broken collection/project relationships
circular moves
insufficient destination space
protected-path violations
files changed since plan creation
```

Useful lifecycle:

```text
plan
→ validate
→ simulate
→ review/questions
→ approve
→ apply
```

---

## 19. Mutation should be transactional where the filesystem allows it

Not every filesystem operation can be made globally atomic, so the tool must be honest about guarantees.

Same-volume rename/move can often be simpler.

Cross-volume move should conceptually be:

```text
copy to controlled destination
→ verify identity/content
→ durable audit checkpoint
→ only then remove source when authorized
```

Operations should be grouped into recoverable chunks with journals/checkpoints.

Never claim “undo” for an operation if the necessary state no longer exists.

---

## 20. Privacy must be scope-aware and fail closed

Privacy is not one global “cloud on/off” switch.

Possible policy states:

```text
local-only
ask-before-cloud
cloud-allowed
metadata-only-cloud
```

Cloud routing is evaluated per task/content scope.

Unknown/sensitive scopes should prefer local handling rather than silently escalating.

Diagnostics exported for support should support path/user redaction without corrupting the internal local audit trail.

---

## 21. External tools are capabilities, not dependencies on success

Use mature tools aggressively where they save engineering time, but wrap them behind capability contracts.

Examples:

```text
Everything        discovery accelerator
ExifTool          metadata
MediaInfo         media metadata
libarchive        archive inventory
OCRmyPDF          PDF OCR orchestration
PaddleOCR         multilingual OCR
Tesseract         compatibility OCR
Tantivy/FTS5      lexical search candidates
USearch           vector search candidate
fclones           duplicate benchmark/optional accelerator
```

`aifs doctor` should report each capability as:

```text
healthy
missing
unsupported
degraded
misconfigured
version-incompatible
```

One optional provider disappearing must not brick the CLI.

---

## 22. Components/workers need explicit lifecycle management

As the tool gains optional workers/models, the user needs control over what is installed and used.

Potential commands:

```powershell
aifs components list
aifs components doctor
aifs components paths
aifs models list
aifs models benchmark
```

Do not silently install large runtimes/models or change another application's configuration.

Every worker invocation records the version/capability used.

---

## 23. Observability is part of usability

Long jobs should expose structured events and useful progress.

Human view:

```text
Job 194 — OCR enrichment
Discovered: 2,830,442
Eligible:     84,221
Done:         31,990
Running:           4
Blocked:          12 needs_user
Failed:            3 retryable
Read:          82 GB
Cache added:    11 GB
GPU util:        54%
```

Machine view should be available as JSON/JSONL/event stream.

Progress must say when totals are unknown or estimated.

---

## 24. Profiles/recipes give power without hiding behavior

Users should be able to save repeatable operational profiles, for example:

```text
personal-pc
library-deep
markaz-local-only
fast-downloads-cleanup
night-ocr
```

A profile can set resource budgets, providers, model routing, privacy, and policy selection, but the resolved values must always be inspectable.

Profiles are convenience, not secret defaults.

---

## 25. The CLI should support three levels of control

### Direct explicit commands

For precise users/scripts:

```powershell
aifs index D:\ --metadata-only
aifs ocr D:\Books --only-missing-text --pages identify
```

### Guided interactive commands

For high-control human workflows:

```powershell
aifs plan D:\Downloads --interactive
```

### Natural-language intent

For complex goals:

```powershell
aifs organize "clean this archive, keep source projects intact, group book editions, never use cloud, and ask me about uncertain cases"
```

All three resolve into the same typed core model.

---

## 26. Machine contracts are first-class API contracts

CLI JSON/JSONL schemas need versions and stability expectations.

When stdout is machine-readable:

- progress and diagnostics go elsewhere;
- ANSI decoration is disabled;
- field meanings do not silently change;
- schemas have `kind` and version identifiers;
- exit codes are stable and documented.

This enables PowerShell, Python, GUI, MCP, agents, and other programs to use the same product surface reliably.

---

## 27. 8TB acceptance testing must include failure, not just speed

Scalability gates should eventually include:

```text
1M-entry synthetic benchmark
10M-entry synthetic benchmark
bounded-memory verification
multi-hour soak test
kill/restart resume test
drive disconnect/reconnect test
Everything/provider reset test
journal-gap reconciliation test
worker timeout/crash test
SQLite busy/recovery test
migration/backup test
Unicode/long-path test
hardlink test
cloud-placeholder non-hydration test
before/after read-only source hashes
```

Performance regressions need thresholds, not “felt fast enough.”

---

## 28. Derived indexes are disposable; user state is not

Canonical durable state belongs in the control plane.

Potential storage split:

```text
SQLite
  jobs / tasks / questions / policy references / file identities / observations / plans / audit

Tantivy or FTS5
  rebuildable lexical index

USearch
  rebuildable vector index

content cache
  OCR/extraction/chunks/thumbnails/derived artifacts
```

A corrupt/rebuilt search index should be inconvenient, not catastrophic.

Backup/restore and integrity checks belong to the core operational story.

---

## 29. Security assumes every file is hostile input

Treat these as untrusted:

```text
filenames
paths
archives
metadata
PDF/document text
OCR text
image text
worker output
model output
policy input from untrusted sources
```

Required defenses include bounded archive inspection, path traversal prevention, decompression-bomb limits, prompt-injection separation, worker timeouts, and never interpreting content text as hidden control instructions.

---

## 30. “Amazing” means the user stays in control

The finished product should be able to say:

```text
I found 2.8 million items.

I can answer your current request accurately by deeply analyzing about 18,400 of them.
I do not need to OCR the other 31,000 scanned PDFs yet.

This plan would:
  Keep       21,230
  Move        8,412
  Rename      3,804
  Group         622
  Archive       184
  Review         27

3 decisions materially affect 9,411 files.
I need your answer before planning those subsets.

Cloud usage: disabled
Estimated additional disk reads: 41 GB
Estimated cache growth: 6.2 GB
No source files have been changed.
```

That is the target experience: fast where facts are cheap, deep where intelligence matters, cautious where uncertainty matters, and explicit before anything irreversible happens.

---

## Definition of production-ready for a major subsystem

A major subsystem is not “done” until appropriate items below are true:

- typed/stable core interface;
- CLI access;
- machine-readable contract;
- bounded-memory behavior;
- cancellation semantics;
- durable/retry semantics if long-running;
- provenance/version recording;
- privacy/effect classification;
- deterministic failure behavior;
- Unicode tests;
- migration/backward-compatibility strategy where stateful;
- integration tests;
- failure/fault tests appropriate to risk;
- performance benchmark at representative scale;
- documentation updated with implemented vs planned status;
- no source mutation unless the command is explicitly a mutation command.
