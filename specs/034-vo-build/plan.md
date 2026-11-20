# Implementation Plan: One Executable per Program (`vo build`)

**Branch**: `034-vo-build` (work on `main`, as for specs 001–033) | **Date**: 2026-11-20 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/034-vo-build/spec.md`

## Summary

`vo build` checks the program as `vo check` does, then writes a copy of a toolchain binary (itself, or
`--base FILE`) with the program's loaded source files appended and a fixed-size trailer at the end (R1,
R2). Every `vo` start reads the last 64 bytes of its own executable; with the trailer present it is an
app: it registers the embedded files as the loader's files (R4), turns its command line into the
toolchain command it stands for (`vo run ENTRY -- ARGS`, `vo replay`, `vo effects`, `vo version`; R5)
and goes on as that command. Nothing is compiled ahead; the app checks its program at start as `vo run`
does (spec assumption).

| Part | File | What |
|---|---|---|
| Build | `src/build.c` (new), `src/main.c` | `vo build`: check, collect files, write base + payload + trailer |
| App start | `src/build.c`, `src/main.c` | trailer read, payload check (T0014), argument translation |
| Loader in app mode | `src/loader.c`, `src/packages.c`, `src/replay.c` | embedded files only; manifest from the payload; no package re-hash; replay uses the embedded entry |
| Syscalls | `src/sys.c`, `src/platform/*.h`, `src/vo.h` | `lseek`, `fchmod` (three architectures) |
| Docs | `docs/reference.md`, `docs/errors.md`, `site/docs/guide/shipping.md` (new), `docs/roadmap.md`, `README.md` | R10 |
| Tests | `tests/cases/build/`, `tests/steps.py` (`app` command), `tests/build_examples.py` (new) | R11 |

## Technical Context

**Language/Version**: C (C11, freestanding, no libc) for the toolchain.

**Primary Dependencies**: none (SHA-256 is `src/sha256.c`).

**Storage**: the executable file; recordings unchanged.

**Testing**: step cases first; `make quick` between edits, `make test` once per change; the gate only
when asked.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64; apps byte-identical per base.

**Project Type**: compiler toolchain (CLI).

**Performance Goals**: an app starts no slower than `vo run` (SC-004); the trailer read adds one `open`,
one `lseek` and one 64-byte read to every `vo` start.

**Constraints**: no libc; deterministic output (FR-002); nothing written on failure (FR-003); the grant
never widened (FR-010); reference ≤ 15,000 tokens (+≤ 120 here).

**Scale/Scope**: about 400 lines of C, one new code (T0014), one site page, one test script.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One command (`vo build`), one prefix rule for app options (`--vo-` + the `vo run` name); no new language surface. | Pass |
| II. Machine-readable feedback | `vo build --json` reports diagnostics as `vo check --json` and a `build` record; a damaged app is T0014; a bad base is T0001 naming the problem. | Pass |
| III. Determinism | The output is the base's bytes, the files in load order and a hash: same inputs, same bytes (FR-002); an app's runs and recordings are `vo run`'s. | Pass |
| IV. Test-first | Step cases and `tests/build_examples.py` before the code; every example compared with `vo run` and replayed (SC-001, SC-002). | Pass |
| V. Single source of truth, minimal surface | Reference command line gains `vo build` and one bullet (+≤ 120 tokens); catalog gains T0014; contracts/cli.md is the source for the options. No compiled form, no new runtime (YAGNI). | Pass |
| Platforms | The payload holds sources only, so one format serves all three architectures; cross builds use another architecture's toolchain as the base. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/034-vo-build/
├── plan.md
├── research.md          # R1–R11
├── data-model.md        # payload and trailer layout
├── quickstart.md
├── contracts/
│   ├── cli.md           # vo build, app options, outputs, exit statuses
│   └── codes.md         # T0014
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/build.c (new), src/main.c, src/loader.c, src/packages.c, src/replay.c, src/sys.c,
src/platform/{x86_64,aarch64,riscv64}.h, src/vo.h, Makefile (C_SRCS)
docs/reference.md, docs/errors.md, site/docs/guide/shipping.md, docs/roadmap.md, README.md
tests/cases/build/, tests/steps.py, tests/build_examples.py
```

**Structure Decision**: one new C file, `src/build.c`, holding both sides of the format (writing and
reading the payload), so the layout lives in one place.

## Order of work

1. Syscalls and the payload format with its reader and writer (R1–R3).
2. `vo build` (check, collect, write; refusals).
3. App start: trailer, T0014, argument translation (R5), loader in app mode (R4).
4. `--vo-effects`, `--vo-replay`, `--vo-version` (R6, R7).
5. Cross-architecture bases (R8); `tests/build_examples.py` (R11).
6. Reference, catalog, site page, roadmap, README; `make test`; ARM64 and RISC-V; results.

## Complexity Tracking

No constitution violations.
