# Personal AI File Organizer — Development and Testing Process

This document defines **how work is performed** on the Personal AI File Organizer fork.

It complements:

```text
PERSONAL-ORGANIZER.md
AGENTS.md
docs/PERSONAL-ORGANIZER-CHECKPOINTS.md
docs/PERSONAL-ORGANIZER-ROADMAP.md
docs/PERSONAL-ORGANIZER-ARCHITECTURE.md
docs/PERSONAL-ORGANIZER-SYSTEM-QUALITY-BAR.md
docs/PERSONAL-ORGANIZER-DEPENDENCY-STRATEGY.md
docs/PERSONAL-ORGANIZER-CLI.md
docs/PERSONAL-ORGANIZER-DEVELOPMENT-LOG.md
```

The checkpoints document defines **what must be true before a stage passes**. This document defines **how implementation, review, testing, evidence, documentation, and handoff are carried out**.

The process is deliberately explicit because this project is intended to become a trustworthy systems tool operating across multi-million-file / multi-terabyte storage. Development discipline is part of the product quality bar.

---

## 1. Roles and working model

The default working split is:

### GitHub-side implementation

The coding agent / assistant handles, where permissions allow:

- architecture-consistent implementation;
- feature branches;
- source-code edits;
- automated tests;
- CI workflow changes;
- documentation;
- issue maintenance;
- PR descriptions and evidence;
- commit structure;
- regression/fault tests that can run in CI;
- inspection of GitHub Actions results;
- preparation of exact Windows/local validation instructions.

### Native Windows qualification

The repository owner handles tests that require the real local Windows environment, installed application behavior, local hardware, GUI/manual interaction, or a deliberately controlled local fixture.

Typical local-owner responsibilities include:

- pulling an exact branch/commit;
- native MSVC/vcpkg build when required;
- running the real packaged launcher;
- checking GUI/manual interaction;
- testing realistic copied/synthetic fixtures;
- testing hardware/device-specific behavior;
- reporting exact command output or observations;
- never testing unsafe new behavior against valuable personal data merely for convenience.

### Shared rule

GitHub CI and local Windows testing are complementary.

Neither is allowed to silently substitute for the other when a checkpoint explicitly requires both.

---

## 2. Mandatory communication contract

During active development, every progress/update message to the repository owner must state what action is required from the owner.

Use a clear statement such as:

```text
What I need from you: nothing right now.
```

or:

```text
What I need from you:
1. Pull branch <branch> at <commit>.
2. Run <exact command>.
3. Paste the complete output.
```

Rules:

1. Never leave the owner guessing whether they should pull, build, test, or wait.
2. If no owner action is needed, say so explicitly.
3. Do not ask the owner to repeat information already available in the repo, current conversation, CI, issue, PR, or documented evidence.
4. When local testing is required, provide the exact branch, expected commit SHA, commands, expected safe scope, and what evidence to return.
5. Do not ask for broad exploratory testing when a narrow reproducible test can answer the question.
6. Do not send the owner into a test that is not yet safe according to the active checkpoint.

---

## 3. One active checkpoint at a time

Before substantial implementation begins, identify the active checkpoint from:

```text
docs/PERSONAL-ORGANIZER-CHECKPOINTS.md
```

The implementation must answer:

```text
What checkpoint is active?
Which exit criteria does this work satisfy?
Which later checkpoint remains locked?
What evidence will prove completion?
```

Do not drift into later features simply because they are interesting or convenient.

Example:

```text
CP-01A active
→ production index qualification is allowed
→ CP-01B provider design may be prepared/documented
→ CP-02 durable jobs must not be mixed into the CP-01A PR
```

A deliberate checkpoint reordering requires an explicit architecture/product decision and corresponding documentation update.

---

## 4. Branch strategy

Canonical branch flow:

```text
upstream/main
    ↓
origin/main
    ↓
personal-organizer
    ↓
feature/*
```

Rules:

- `main` remains upstream-friendly.
- `personal-organizer` is the stable custom integration branch.
- substantial custom work happens in focused `feature/*` branches.
- use one coherent branch/PR per implementation slice when practical.
- do not stack unrelated future work indefinitely on an old branch.
- qualification-sensitive branches should be frozen while their final production gate runs.
- documentation work that would move a qualifying head should be prepared on a separate branch until the gate result is known.

Typical naming:

```text
feature/indexer
feature/checkpoints
feature/filesystem-provider
feature/everything-provider
feature/job-runtime
feature/policy-engine
feature/ocr
feature/search
```

---

## 5. Work lifecycle for every implementation slice

A normal slice follows this sequence:

```text
1. identify active checkpoint + acceptance criterion
2. inspect current docs/issues/upstream implementation
3. define smallest coherent code change
4. create/use focused feature branch
5. implement
6. add or update tests
7. update documentation in the same development cycle
8. commit with a descriptive reversible commit
9. push
10. inspect CI
11. fix only evidence-backed failures
12. perform required native/local qualification
13. record evidence in issue/PR/dev log/checkpoint
14. final diff review
15. merge only after stage gates are satisfied
16. run post-merge sanity checks when required
```

Do not treat documentation as cleanup after the feature is finished. Documentation is part of the implementation slice.

---

## 6. Documentation requirements

The repository itself must be sufficient to reconstruct project state without relying on chat history.

### Canonical documents

```text
PERSONAL-ORGANIZER.md
  product vision and current high-level status

AGENTS.md
  mandatory coding/agent rules

docs/PERSONAL-ORGANIZER-CHECKPOINTS.md
  canonical stage gates and current checkpoint status

docs/PERSONAL-ORGANIZER-DEVELOPMENT-PROCESS.md
  how implementation/testing/evidence/handoffs are performed

docs/PERSONAL-ORGANIZER-ROADMAP.md
  product/build sequence

docs/PERSONAL-ORGANIZER-ARCHITECTURE.md
  system architecture and boundaries

docs/PERSONAL-ORGANIZER-SYSTEM-QUALITY-BAR.md
  definition of serious production quality

docs/PERSONAL-ORGANIZER-DEPENDENCY-STRATEGY.md
  build vs delegate decisions

docs/PERSONAL-ORGANIZER-CLI.md
  public CLI/machine contract and implementation status

docs/PERSONAL-ORGANIZER-DEVELOPMENT-LOG.md
  chronological engineering decisions, failures, corrections, evidence
```

### Documentation update triggers

Update the relevant files whenever any of these change:

- architecture;
- checkpoint status;
- CLI behavior;
- public schema/exit code;
- safety contract;
- provider/worker/model decision;
- branch/PR strategy;
- production qualification result;
- benchmark threshold/result;
- known limitation;
- dependency/runtime requirement;
- local testing procedure;
- checkpoint sequencing;
- implementation status previously described as planned/in-progress.

### Status honesty

Use explicit wording:

```text
IMPLEMENTED
QUALIFIED LOCALLY
QUALIFIED IN CI
ACTIVE
PLANNED
DEFERRED
BLOCKED
```

Do not write "done", "production ready", "resumable", "safe for whole drive", or equivalent unless the required checkpoint evidence supports that statement.

---

## 7. Development log discipline

`docs/PERSONAL-ORGANIZER-DEVELOPMENT-LOG.md` records important engineering history.

A meaningful entry should capture:

```text
date
checkpoint
branch / commit / PR
problem or goal
decision made
why the decision was made
important implementation details
test/CI evidence
local/native evidence if any
known limitation
next gate
```

The development log should record failures that materially influenced the architecture or safety model, not just successes.

Examples worth logging:

- a production launcher boundary failure;
- a migration-order bug caught by regression tests;
- a provider outage being incorrectly interpreted as deletion;
- a dependency/packaging problem requiring CI hardening;
- a benchmark that rejects a proposed backend;
- a local/CI discrepancy;
- a deliberate architecture pivot.

Git history remains the exact record of code changes; the development log explains their significance.

---

## 8. Commit discipline

Every commit should have one clear purpose and be understandable/reversible.

Preferred forms:

```text
feat(indexer): ...
fix(indexer): ...
feat(provider): ...
feat(job-engine): ...
feat(policy): ...
feat(search): ...
feat(ocr): ...
test(indexer): ...
test(provider): ...
docs(personal-organizer): ...
ci(personal-organizer): ...
```

Avoid vague messages such as:

```text
update
fix stuff
changes
work
final
```

### Commit separation

Keep unrelated concerns separate when practical.

Example:

```text
Fix Windows packaged index launcher
Harden Windows production index gate
Document CP-01A production qualification
```

Do not use `git add .` as a habit when a narrow explicit file list is safer and clearer.

Before committing:

```text
inspect git status
inspect relevant diff
git diff --check
verify no temporary/backup/generated files are staged
```

---

## 9. Pull request discipline

A PR is an implementation/evidence container, not just a code-diff transport.

The PR body should state:

```text
checkpoint / scope
goal
implemented behavior
safety boundaries
known non-goals
important design decisions
automated test status
local/native qualification status
remaining blockers
documentation changed
merge target
```

Before merge, the PR should contain or point to the required evidence pack.

Draft PRs are preferred while a stage is incomplete.

Do not mark a PR ready merely because code compiles.

---

## 10. Issue discipline

Issues own actionable scope/checklists and architecture epics.

Rules:

- keep the issue checklist synchronized with observed reality;
- do not close an issue merely because implementation started;
- do not leave stale blockers checked/unchecked after evidence changes;
- important follow-up work that is intentionally deferred should become a scoped follow-up issue rather than disappearing into chat notes;
- cross-link relevant PRs/checkpoints/docs.

---

## 11. Automated testing hierarchy

Use the cheapest trustworthy test first, but production boundaries require production-representative tests.

Typical hierarchy:

```text
unit tests
→ temporary filesystem/SQLite integration tests
→ CLI parse/schema/exit-code tests
→ platform-specific focused tests
→ packaged executable smoke tests
→ synthetic scale tests
→ fault injection
→ native/local qualification where required
→ realistic controlled copied fixture
```

Focused test binaries are not substitutes for the real packaged executable when the bug can occur at launcher/runtime/packaging boundaries.

---

## 12. Local Windows testing handoff protocol

When owner testing is required, provide one explicit handoff block.

Required fields:

```text
Checkpoint:
Purpose:
Branch:
Expected commit SHA:
Safety scope:
Prerequisites:
Exact commands:
Expected result:
What to inspect manually:
What output/evidence to return:
Stop conditions:
```

Example:

```text
Checkpoint: CP-01B
Purpose: verify NativeProvider parity on packaged Windows build
Branch: feature/filesystem-provider
Expected commit: abc123...
Safety scope: copied synthetic fixture only; do not scan personal roots

Commands:
  git fetch origin
  git switch feature/filesystem-provider
  git pull --ff-only
  ...

Return:
  complete console output
  generated JSON result
  fixture before/after hash result
```

Rules:

1. Never tell the owner merely "pull and test it".
2. Always specify the exact intended commit.
3. Always specify whether personal data is forbidden.
4. Always define pass/fail observations.
5. If a test can modify files, use disposable/copy fixtures and state that explicitly.
6. If the owner reports a failure, preserve the raw evidence before changing code.

---

## 13. Safety testing rules

For read-only stages:

- use temporary/synthetic/copied fixtures;
- prove source preservation when the checkpoint requires it;
- never use an unattended whole `C:\` run as an early experiment;
- avoid user-profile/root scans until explicitly unlocked;
- do not enable reparse following without its checkpoint semantics/tests;
- do not silently hydrate cloud placeholders;
- do not bypass ACL/security boundaries.

For mutation stages later:

- use disposable fixtures first;
- plan before mutation;
- validate/simulate before apply;
- test rollback/audit honestly;
- never call destructive modes of external tools automatically.

---

## 14. Evidence pack standard

A checkpoint cannot become PASS from a verbal report alone.

Its evidence pack should include the relevant subset of:

```text
checkpoint ID
branch
commit SHA(s)
PR number
issue number
CI workflow/run/job IDs
commands executed
fixture description/version
result JSON/JSONL
SQLite verification
before/after hashes
screenshots/manual observations when appropriate
benchmark metrics
memory/resource metrics
fault-injection results
known limitations
documentation commits
post-merge result
```

The exact evidence belongs in the repository, issue, PR, CI artifacts, or development log.

Chat may coordinate the work, but chat is not the canonical evidence store.

---

## 15. CI failure protocol

When CI fails:

```text
1. identify exact failed job + step
2. obtain log/evidence
3. distinguish product bug vs test bug vs infrastructure/transient failure
4. patch only the demonstrated problem
5. add/strengthen regression coverage when appropriate
6. document significant failure/correction
7. commit narrowly
8. rerun the required gate
```

Do not weaken an assertion merely to make CI green unless the assertion itself is proven incorrect and the replacement preserves the intended safety contract.

Do not claim an infrastructure failure is a product PASS.

---

## 16. Local failure protocol

When the owner reports a local failure:

1. capture exact branch/SHA;
2. capture exact command;
3. preserve stdout/stderr/error code;
4. capture relevant generated artifact/log/database path;
5. determine whether the local tree was clean;
6. reproduce in CI/synthetic fixture where practical;
7. patch on GitHub;
8. commit and document;
9. give a new exact local handoff only after the patch is pushed.

Avoid ad-hoc manual edits on the owner's machine that are not represented in Git.

If an emergency local patch is necessary for diagnosis, turn it into a proper Git commit before treating it as part of the product.

---

## 17. Architecture/dependency change protocol

Before introducing a significant dependency, database, OCR engine, search backend, provider, model runtime, or scheduling framework:

1. inspect upstream capability;
2. inspect the dependency strategy;
3. define the capability gap;
4. compare realistic alternatives;
5. review license/packaging/runtime footprint;
6. benchmark where the decision is performance/quality sensitive;
7. prefer narrow adapters/workers/providers;
8. define failure/fallback semantics;
9. record provenance/version;
10. update architecture/dependency docs and issue.

Do not adopt tools because they are fashionable or benchmark well on unrelated datasets.

---

## 18. No hidden local-only product state

The working software must be reproducible from Git plus documented external prerequisites.

Do not allow essential product behavior to depend on:

- uncommitted local source edits;
- undocumented files in Downloads;
- manually copied DLLs with no documented build/install path;
- local-only scripts not represented in the repository when they become part of the development/qualification process;
- undocumented environment variables;
- chat-only architecture decisions.

Temporary diagnostic scripts may exist locally, but if they become part of a repeatable gate they should be moved into the repository or their logic should be represented in CI/tests/docs.

---

## 19. Handoff between checkpoints

A checkpoint handoff should state:

```text
Completed checkpoint:
Evidence:
Known limitations:
What is now unlocked:
What remains prohibited:
Next branch:
Next issue/PR:
First implementation slice:
Local testing expected during next checkpoint:
```

Do not begin a new checkpoint with stale documentation from the previous one.

---

## 20. Current project-specific workflow

Current checkpoint:

```text
CP-01A — Trustworthy native filesystem observation + CLI index
Status: ACTIVE — final production qualification
```

Current implementation branch:

```text
feature/indexer
```

Current documentation/checkpoint branch:

```text
feature/checkpoints
```

Current integration target:

```text
personal-organizer
```

Current rules:

1. Keep `feature/indexer` frozen while its final hosted Windows production gate runs.
2. Do not merge documentation work into it until the qualification result is known, because that would move the final tested head.
3. If the gate fails, fix the demonstrated CP-01A issue first.
4. If the gate passes, inspect artifacts/evidence, synchronize documentation, integrate the checkpoint/process docs, review the final diff, and complete CP-01A integration.
5. Only then begin CP-01B on a new provider-focused branch.

---

## 21. Final operating rule

Every meaningful software change should leave behind a trail that another engineer can follow without access to the original chat:

```text
why the work existed
which checkpoint it belonged to
what changed
which commit contains it
what tests ran
what failed and why
what evidence passed
what the owner tested locally
what remains uncertain
why the checkpoint advanced
what comes next
```

If that trail does not exist, the work is not fully documented yet.
