# Personal Organizer — Dependency and Delegation Strategy

This document records which technical problems I intend to solve inside the Personal Organizer and which I intend to delegate to mature external tools or libraries.

The rule is simple: I do not want to spend months rebuilding solved low-level infrastructure merely because this project started as a file sorter. I want to spend engineering effort on the parts that make this system uniquely useful to me: interpretation, content intelligence, requirements, policy, relationships, planning, questions, safety, provenance, and control.

## Product boundary

The Personal Organizer owns:

- durable job/session state;
- the user's interpreted requirements;
- questions and answers;
- policy and privacy rules;
- provenance and confidence;
- content-analysis cache identity;
- relationships/collections;
- organization plans;
- validation and review gates;
- filesystem mutation safety;
- audit and undo history;
- local/remote model routing;
- machine-readable CLI contracts.

External tools are workers/providers. They do not become the authority for filesystem state, user intent, or organization decisions.

## Integration principles

1. Prefer a narrow adapter over copying another project's logic into the core.
2. Prefer SDK/API/JSON interfaces over scraping human output.
3. Record the external tool/version that produced derived data.
4. Treat workers as fallible: timeout, retry, capture diagnostics, and preserve partial work.
5. Never interpret a worker outage as evidence that files were deleted.
6. Keep offline operation as the baseline.
7. Do not invoke destructive modes of delegated tools automatically.
8. Benchmark before promoting a candidate into a required dependency.
9. Keep fallback paths so one optional tool cannot brick the application.
10. Separate the small durable control plane from large/search-specific derived indexes.

---

## Windows filesystem discovery — voidtools Everything

**Decision: strong integration candidate / optional Windows fast path.**

Everything already solves Windows filename/path discovery extremely well, especially on NTFS/ReFS volumes. The official SDK exposes IPC access with Unicode support, and Everything 1.5 exposes journal/change information for file create/delete/rename/move/modify events.

Intended architecture:

```text
FilesystemProvider
├── EverythingProvider   Windows accelerated provider
└── NativeProvider       portable fallback and validation source
```

Everything should provide:

- fast initial enumeration;
- filename/path/size/timestamp/attribute discovery where available;
- incremental change feed/journal positions where supported;
- cheap search/lookup of filesystem entries.

Everything should **not** provide:

- the Personal Organizer semantic/content database;
- OCR/content indexing for the whole corpus;
- user requirement interpretation;
- organization decisions;
- filesystem mutation on behalf of the planner.

The organizer still resolves durable file identity where necessary (for example Windows volume/file IDs), stores its own observation/policy state, and reconciles provider changes safely.

A future optional managed/private Everything instance may be useful, but it must not silently alter an existing user's Everything configuration.

Tracking: Issue #4.

---

## Metadata extraction — ExifTool

**Decision: strong candidate for a metadata worker.**

ExifTool supports an unusually broad range of image, camera, media, document, archive, font, design, and application file types. Reimplementing this metadata coverage would be wasteful.

Use it in read-only extraction mode through a long-lived worker process where practical. Normalize selected fields into our schema while retaining raw/provenance data for uncommon tags.

Do not use ExifTool write operations automatically.

---

## Audio/video metadata — MediaInfo

**Decision: keep/reuse upstream integration.**

MediaInfo is already present in upstream and is a mature source for audio/video technical and tag data. Do not create a competing parser unless a concrete unsupported format requires another backend.

---

## Archive inventory — libarchive

**Decision: strong library candidate.**

Use libarchive for streaming archive inventories and safe extraction/listing support across common archive formats.

Archive inspection must have resource limits to prevent decompression bombs, absurd member counts, path traversal, and accidental full extraction during indexing.

---

## Exact duplicate discovery — fclones and internal cache

**Decision: benchmark/reference/optional accelerator; do not make it the canonical mutation engine.**

fclones is explicitly optimized for duplicate discovery across large trees, includes low-memory path handling, staged hashing, persistent hash caching, JSON output, and different strategies for SSD/HDD access.

Useful options:

- benchmark our duplicate stages against it;
- optionally allow a read-only `fclones group` provider;
- reuse its algorithmic ideas: size grouping, partial hashes, per-device I/O tuning.

However, once the Personal Organizer already has durable file identity and cached hashes, we should avoid rereading terabytes merely to duplicate information we possess.

Never invoke `remove`, `link`, `move`, or filesystem dedupe modes automatically.

---

## Fast content fingerprints — BLAKE3

**Decision: strong candidate.**

Use BLAKE3 for fast fingerprints/content-addressed analysis caches where a modern fast cryptographic hash is useful. Keep SHA-256 available for interoperability/audit requirements.

Possible staged identity:

```text
metadata identity
→ prefix/suffix/sample fingerprint
→ BLAKE3 full content hash when justified
→ optional SHA-256 verification/reporting
```

Hashing should be demand-driven and scheduled per device rather than mandatory during discovery.

---

## OCR orchestration — OCRmyPDF

**Decision: useful worker for producing searchable PDFs, not necessarily the recognition engine for every case.**

OCRmyPDF is mature at PDF OCR orchestration and can add searchable text layers to scanned PDFs. It should be evaluated as a PDF-level worker, particularly when preserving a searchable output PDF is requested.

The Personal Organizer's analysis cache should not require rewriting the user's source PDF merely to understand it.

---

## Multilingual OCR — PaddleOCR

**Decision: leading offline candidate to benchmark.**

PaddleOCR has multilingual support including Arabic and Urdu and is Apache-2.0 licensed. It should be benchmarked on my actual Arabic/Urdu printed scans before adoption.

Potential worker shape:

```text
ocr-paddle worker
stdin/json job
→ page text + boxes + confidence + language/model metadata
```

Keep it out-of-process initially so the C++ CLI does not inherit a large Python/Paddle runtime dependency directly.

---

## OCR/layout alternative — Surya

**Decision: benchmark candidate, not yet a dependency.**

Surya provides OCR, layout analysis, reading order, tables, and document-oriented recognition in many languages. It is attractive for complex pages, but packaging/model/runtime/license implications must be reviewed before adoption.

Do not make it a required dependency until real Arabic/Urdu benchmarks justify it.

---

## Tesseract

**Decision: retain as a lightweight/offline compatibility option.**

Tesseract has Arabic script/language models and broad deployment support. It may remain useful for OCRmyPDF workflows and low-resource fallback cases even if a neural OCR engine wins the main benchmark.

---

## Full-text search — Tantivy

**Decision: strong candidate; benchmark against SQLite FTS5 before committing.**

Tantivy is an embedded full-text search library with incremental/multithreaded indexing, low startup time, compressed document storage, and an architecture designed for large indexes.

It is attractive for extracted text/OCR search at the scale where storing and querying all content through ordinary SQLite rows becomes awkward.

Do not put canonical filesystem/job state in Tantivy. Treat it as a rebuildable derived search index keyed back to our durable file/content IDs.

Arabic/Urdu tokenization/search behavior must be benchmarked on real data, including exact text, normalized variants, diacritics, and mixed-language documents.

---

## Vector/semantic search — USearch

**Decision: strong C++-native candidate for later semantic search.**

USearch is portable, supports C++ directly, can persist/view indexes from disk, and avoids requiring a separate vector database server.

Use only after we have a good chunking/embedding identity model. Vector search must supplement, not replace, deterministic filename/full-text search.

`sqlite-vec` is interesting but remains pre-v1; do not make it a foundational dependency yet.

---

## Direct fallback search — ripgrep

**Decision: useful fallback/diagnostic worker.**

ripgrep is excellent for direct line-oriented content searches and supports Windows/macOS/Linux. It is not our persistent index, but it is valuable for validation, one-off searches, and comparing indexed search results against disk.

---

## Fast fallback enumeration — fd

**Decision: reference/optional worker, not primary Windows index.**

fd provides fast parallel traversal and good ignore semantics. It may be useful on non-Windows platforms or for diagnostics, but on Windows a healthy EverythingProvider should beat recursive traversal for repeated inventory work.

---

## CLI parsing — CLI11

**Decision: strong candidate for replacing ad-hoc parsing as the CLI expands.**

CLI11 supports subcommands, nested commands, validators, configuration, Unicode considerations, helpful errors, and cross-platform C++ builds without a large dependency footprint.

Our CLI is becoming too important to maintain through hand-written argument loops indefinitely.

---

## Terminal UX — indicators / tabulate / replxx

**Decision: candidates for specific UX needs.**

- `indicators`: progress bars/multi-progress for interactive terminals;
- `tabulate`: UTF-8 tables for human output;
- `replxx`: optional future `aifs shell` interactive REPL with history, hints, completion, and Unicode support.

Machine modes (`--json`, `--jsonl`, piping) must never depend on these presentation libraries.

---

## Parallel execution — Taskflow

**Decision: possible in-process scheduler only.**

Taskflow can help execute CPU/GPU/I/O stage DAGs inside one process, but it is not a durable job system.

Persistent tasks/checkpoints/retries/leases must remain in our own database so a crash or reboot can resume safely.

---

## Core durable database — SQLite first

**Decision: keep SQLite as the control-plane database unless benchmarks prove it is the bottleneck.**

Use SQLite for:

- jobs/tasks/checkpoints;
- roots/providers/cursors;
- file identities and observations;
- policy states;
- questions/answers;
- analysis provenance;
- plan/audit metadata.

Use WAL, batched writes, careful indexes, schema migrations, integrity checks, and backups.

Do not put multi-gigabyte raw OCR/content blobs or vector structures into the control tables merely because SQLite can store them.

RocksDB is a valid future candidate for high-volume key/value workloads, but adopting a second database engine before measurements require it would increase packaging and migration complexity.

---

## Core architecture after delegation

```text
                         aifs CLI
                            │
                Requirement / Intent Compiler
                            │
                 Durable Job / Question Engine
                            │
     ┌──────────────────────┼────────────────────────┐
     │                      │                        │
Filesystem providers   Analysis workers        Model providers
     │                      │                        │
Everything          ExifTool / MediaInfo       llama.cpp
Native walker       PDF/document extractors    Ollama / LM Studio
                   OCR workers                 OpenAI / Gemini / etc.
                   libarchive
     │                      │                        │
     └──────────────────────┼────────────────────────┘
                            │
                Durable metadata/control DB
                            │
          ┌─────────────────┴─────────────────┐
          │                                   │
   Full-text derived index             Vector derived index
      Tantivy/FTS5                          USearch
          │                                   │
          └─────────────────┬─────────────────┘
                            │
             Policy / Relationships / Planner
                            │
                 Review → Apply → Audit
```

The CLI remains usable if optional accelerators are unavailable. `aifs doctor` should report exactly which capabilities are installed, healthy, degraded, or missing.

Tracking: Issues #3, #4, #5.
