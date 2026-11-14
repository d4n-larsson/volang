# Implementation Plan: Standard Library and Content-Pinned Packages

**Branch**: `007-stdlib-packages` | **Date**: 2026-11-14 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/007-stdlib-packages/spec.md`

## Summary

**1. The library is vo code** (research R1–R5): `std/json.vo`, `std/time.vo`, `std/text.vo`, `std/random.vo` and `std/http.vo` are ordinary vo files embedded in the binary and reached with `import std.MODULE.ITEM;`. Only what vo cannot express goes into the runtime: a network exchange primitive (`__http_exchange`, sockets and DNS by system calls), a round-trip float formatter (`__float_text`), and a real-time `now_ms`. Both primitives are callable only from `std`.

**2. Library effects behave like builtin effects** (R6): a call needs the effect at any scope, and the library function runs with its caller's grant, so `std.http.get` is limited by the caller's `net("host")` scope at run time. Standard functions are not listed in `vo.effects`.

**3. `vo doc`** (R7) prints signatures from the checker's own declarations and descriptions from the comments above each item in the library source; builtins come from the reference table; `--json` gives structured records.

**4. Packages** (R8): `vo.packages` (found like `vo.effects`) lists `NAME DIRECTORY sha256:HEX`; imports whose first segment is a package name resolve into its directory; the checker hashes a package's `.vo` files (SHA-256, new `src/sha256.s`) on first use and refuses a mismatch (E0701); `vo package` writes entries; package functions are in `vo.effects` as `@NAME/path.vo:fn`.

**5. HTTP** (R9–R11): HTTP/1.1 written in vo over the exchange primitive; hosts resolved from `/etc/hosts` or one DNS query; `https://` refused; exchanges recorded and replayed (spec 006).

**6. Evaluation** (R12): a signature conformance check over everything `vo doc` lists, a JSON corpus, a time check against Python, and at least 10 task programs written by a separate agent from `vo doc` output only.

## Technical Context

**Language/Version**: RISC-V assembly (RV64IMAFD, lp64d), GNU as, for the toolchain; vo for the standard library (embedded).

**Primary Dependencies**: none at run time (Linux system calls only); build: `binutils-riscv64-linux-gnu`; `qemu-user` on x86; SESI `riscv64` lint; Python 3 for tests (and its `datetime`, `json`, `http.server` as references and test fixtures only).

**Storage**: files: `vo.packages` manifests, package directories, recordings.

**Testing**:
- `make test` gains `tests/cases/{doc,json,time,text,random,http,packages}`, `tests/json_corpus.py`, `tests/time_check.py`, `tests/doc_conformance.py` and the HTTP step cases (`tests/steps.py` with a `server` step and `tests/http_server.py`).
- `make stdlib-eval` runs the task programs (minutes; not in `make test`).
- `make gc-check`, `make speed-check`, `make baseline` (now spec 006, `1ff166a`).

**Target Platform**: Linux user space, riscv64, static ELF without libc; under `qemu-riscv64` on x86 (sockets work through qemu-user's system-call translation).

**Project Type**: compiler and runtime for a programming language, as a CLI, now with a standard library.

**Performance Goals**: programs without the library within 5% of the spec 006 binary; JSON parsing of a 100 KB document in under 2 s under qemu; `vo doc` of the whole library in under 1 s.

**Constraints**:
- Determinism (Principle III): library functions are pure or fixed by `--seed`/`--clock-ms`; HTTP is recorded and replayed.
- The reference grows by at most 2,000 tokens; library item documentation lives in `vo doc` (spec Assumptions).
- Existing programs unchanged (FR-020): only `std` and manifest names become reserved import roots; `now_ms` keeps its documented meaning (real time) and tests and examples use `--clock-ms`.

**Scale/Scope**: about 1,300–1,600 lines of vo in `std/`; about 3,000–3,500 lines of assembly (`src/sha256.s` ~300, `src/packages.s` ~1,000 for the manifest, hashing and `vo package`, `src/doc.s` ~700, `src/net.s` ~800 for sockets, DNS and the primitive, plus loader, checker, effects record, replay and main.s changes); about 80 golden and step cases, 200 JSON documents, 10 task programs.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Assessment |
|---|---|
| I. Agent-first design | **PASS.** No new syntax: library items are imported with the existing `import` form, one item per line. One form per command (`vo doc [NAME]`, with packages found from the working directory instead of an extra ENTRY argument, R7). No implicit conversions: JSON integers and floats are separate cases. |
| II. Machine-readable feedback | **PASS.** New codes E0701–E0705 and T0010 (contracts/codes.md); `doc` and `package` JSON records; existing records unchanged (schema 1). Package errors name both hashes. |
| III. Determinism | **PASS.** The library is pure or fixed by `--seed`/`--clock-ms`; HTTP responses are recorded and replayed. The one change of observable behavior is `now_ms` reading the real-time clock, which is what the reference already documents ("wall-clock"); every golden case and example uses `--clock-ms` or differences of `now_ms`. |
| IV. Test-first through the CLI | **PASS.** Each story starts with golden or step cases; every new code has a case; `vo doc` and the library are covered by the conformance check; reference examples run. |
| V. Single source of truth, minimal surface | **PASS.** The library documents itself (`vo doc` reads its source comments and the checker's declarations), so there is no second description to drift; the reference adds one short section. Two internal primitives instead of a public socket API. `ponytail:` markers: one request per connection, IPv4 and one DNS server, 512-deep JSON, the 16 MiB response cap, linear JSON parsing over `chars`. |
| Implementation constraints | **PASS.** Toolchain code stays assembly with prototype comments and SESI lint; the library is vo, which the toolchain itself checks; Python only in tests. |

No violations. Complexity Tracking records the one structural addition.

**Post-design re-check:** still passes. Embedding vo source keeps the library under the same checker, formatter and tests as user code; the effect rule (R6) reuses the builtin and anonymous-function mechanisms instead of adding a new kind of effect.

## Project Structure

### Documentation (this feature)

```text
specs/007-stdlib-packages/
├── plan.md, research.md (R1–R13), data-model.md, quickstart.md
├── contracts/
│   ├── library.md    # every module's items and behavior
│   ├── cli.md        # vo doc, vo package
│   ├── packages.md   # vo.packages, the hash, imports, effects record
│   └── codes.md      # E0701–E0705, T0010
├── checklists/requirements.md
└── tasks.md          # Phase 2
```

### Source Code (repository root)

```text
std/                  # NEW: the library, in vo (embedded)
├── json.vo, time.vo, text.vo, random.vo, http.vo
src/
├── data.s            # .incbin the std files; std_files table
├── loader.s          # std/ and package import roots; FILE_STD / FILE_PKG flags
├── packages.s        # NEW: vo.packages, package hashing and verification, vo package
├── sha256.s          # NEW: SHA-256
├── doc.s             # NEW: vo doc
├── net.s             # NEW: sockets, /etc/hosts, DNS, __http_exchange
├── check_expr.s      # __ primitives (std only); library calls count like builtins for effects
├── check_effects.s   # E0503 relaxed for std callees; std functions not in the record
├── effects_record.s  # @NAME/path for package functions; std skipped
├── rt_call.s         # btab entries for the primitives
├── rt_effects.s      # real-time now_ms
├── num.s             # 17-digit float text
├── replay.s          # EVK_HTTP events
├── compile.s / interp.s  # clear std functions' declared effects before running (inherit grants)
└── main.s            # doc and package commands
tests/
├── cases/{doc,json,time,text,random,packages}/, cases/http/ (step cases)
├── json/valid/*.json, json/invalid/*.json, json_corpus.py
├── time_check.py, doc_conformance.py, http_server.py
└── stdlib_eval/tNN-*/ + stdlib_eval.py
docs/reference.md, docs/errors.md
```

**Structure Decision**: the library is a new top-level `std/` directory of vo files (embedded at build time); four new assembly files for the four new mechanisms with no existing home. The Makefile embeds `std/*.vo` as dependencies of `data.o`.

## Complexity Tracking

| Addition | Why needed | Simpler alternative rejected because |
|---|---|---|
| Library functions run with their caller's effect grant (R6) | `std.http.get` must be limited by the user's `net("host")` scope, not allowed every host because the library declares bare `net` | Scoped declarations in the library cannot know the user's hosts; an effect parameter on every library function would put `E: effects` in signatures agents must read and pass through. |
