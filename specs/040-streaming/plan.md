# Implementation Plan: Streaming

**Branch**: `040-streaming` (work on `main`, as for specs 001–039) | **Date**: 2026-11-21 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/040-streaming/spec.md`

## Summary

Four library functions, written in vo, each a loop that asks for the next piece and calls the
program's function. Because the loop is vo code, the function is called like any other, its effects
are an ordinary effect parameter, and the library function's own structure releases what it opened on
every path (R1). What differs is where the pieces come from:

- **Files** need no open file at all: one new primitive reads a block of a file at an offset, opening
  and closing it each time (R2). `each_chunk` asks for blocks of the caller's size; `each_line` asks
  for a block and gets back the whole lines in it, split in C (R3).
- **HTTP** needs no new C: `send_stream` is written on `std.tcp` (spec 039): connect, send the request
  `std.http` already builds, read the head, then the body by length, by chunks or to the end (R4).
- **Child programs** need the one piece of kept state: a started child with its pipes, in a small
  table in `src/process.c`, with "next line" and "end" primitives, and a sweep that ends any child
  still there when a test or the program ends (R5).

| Part | File | What |
|---|---|---|
| Files | `src/fs.c`, `std/fs.vo` | primitive `__fs_block(path, offset, size)`; `each_chunk`, `each_line` |
| HTTP | `std/http.vo` | `send_stream` on `std.tcp`; head parsing shared with `send` |
| Children | `src/process.c`, `std/process.vo` | `__proc_start`, `__proc`; `run_lines`; the sweep |
| Recording | `src/replay.c`, `src/vo.h` | file blocks as `read_file` events with an offset; `proc_line`, `proc_end` events (R6) |
| Wiring | `src/vo.h`, `src/check_expr.c`, `src/rt_call.c`, `src/check_effects.c` | three builtin ids |
| Docs | `docs/reference.md`, `site/docs/guide/files-and-processes.md`, `standard-library.md`, roadmap, changelog, review | R8 |
| Tests | `tests/stream.py` (new, in `make test`), `tests/cases/fs/`, `tests/cases/process/`, `tests/tcp_server.py` (an HTTP body mode) | R9 |

## Technical Context

**Language/Version**: C (C11, freestanding, no libc) and vo.

**Primary Dependencies**: none new; `std.http` now imports `std.tcp`.

**Storage**: none.

**Testing**: `tests/stream.py` generates large inputs in a temporary directory (a 300 MiB file, a
100 MiB body from a local server, a slow child), runs vo programs under a small `--max-memory` and
compares results, hashes, timing and replays; golden cases for the small behaviours (line ends, early
stop, errors, scopes). `make quick AREA=stream` between edits, `make test` once; the gate only when
asked. `VO_STREAM=small` shrinks the large inputs for the other architectures under qemu.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler toolchain with its standard library.

**Performance Goals**: 300 MiB of lines summed in under 10 s (SC-001): lines are split in C, a block
of 256 KiB at a time, so the cost is one string and one call per line.

**Constraints**: memory bounded by one block (SC-005); no static buffer over 64 KiB (blocks are read
into the result's own storage; the child table is 16 small entries); reference ≤ 15,000 tokens (14,898
now: about +40, R8).

**Scale/Scope**: about 120 lines in `src/fs.c`, 250 in `src/process.c`, 40 in `src/replay.c`, 250 of
vo across three modules, 500 of Python tests.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One shape for every stream: pass a function, answer `ok(true)`, `ok(false)` or an error. Nothing to close, so nothing to forget. | Pass |
| II. Machine-readable feedback | Errors are the modules' existing results; scope and effect errors keep their codes. | Pass |
| III. Determinism | Every piece is recorded where it enters the program; replay needs no file, network or child. | Pass |
| IV. Test-first | `tests/stream.py` and the golden cases per story before its code. | Pass |
| V. Single source of truth, minimal surface | No new type, effect or syntax. HTTP streaming reuses `std.tcp`; the whole-value functions stay as they are and are not reimplemented. Four functions, no variants for cases nobody asked for. | Pass |
| Vendored code | None. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/040-streaming/
├── plan.md
├── research.md          # R1–R9
├── data-model.md
├── quickstart.md
├── contracts/
│   └── streaming.md     # the four functions, errors, limits, recording
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
std/fs.vo, std/http.vo, std/process.vo
src/fs.c, src/process.c, src/replay.c, src/vo.h, src/check_expr.c, src/check_effects.c, src/rt_call.c
tests/stream.py (new), tests/tcp_server.py, tests/cases/fs/, tests/cases/process/
docs/reference.md, site/docs/guide/
```

**Structure Decision**: no new C file and no new module: each function lives in the module of its
whole-value counterpart.

## Order of work

1. Files: the block primitive with recording, `each_chunk`, `each_line`; golden cases; the 300 MiB run.
2. HTTP: `send_stream` on `std.tcp`; length, chunked, to-the-end, TLS; the 100 MiB run.
3. Children: start, next line, end, the sweep; `run_lines`; timing and early stop.
4. Replay of all three; docs; three architectures; stress cases; `make test`; results.

## Complexity Tracking

No constitution violations.
