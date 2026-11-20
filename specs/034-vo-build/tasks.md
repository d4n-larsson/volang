---

description: "Task list for vo-lang spec 034: One Executable per Program (vo build)"
---

# Tasks: One Executable per Program (`vo build`)

**Input**: Design documents from `specs/034-vo-build/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R11), data-model.md, contracts/cli.md,
contracts/codes.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each task's step cases land first and fail before its code.

**Organization**:
- Setup: test wiring (the `app` step command), the results file.
- Foundational: syscalls, the payload format (writer and reader), the trailer check at start.
- US1 (P1) build and run; US2 (P1) `--vo-effects`; US3 (P2) `--vo-` options, record and replay;
  US4 (P2) other architectures; US5 (P3) documentation.
- Polish: every example, speed, three platforms, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Step cases**: `tests/cases/build/<name>/` with `steps` and `expected/`; accept with
  `python3 tests/steps.py --accept PATH` after reading the output.
- **Running an app in a step**: `app PATH ARGS... [< FILE]` (T001).
- **Texts**: messages, outputs and exit statuses from contracts/cli.md; the layout from data-model.md.
- **Unchanged programs**: no existing golden case may change.
- **Reference budget**: `python3 tests/ref_examples.py` after the reference change; at most +120
  tokens (from about 14,622).
- **Between edits**: `make quick AREA=build` (T002); `make test` once, through
  `tools/progress.py run test`; the gate only when asked.

---

## Phase 1: Setup

- [X] T001 `tests/steps.py`: a step command `app PATH ARGS... [< FILE]` that runs the executable at
  PATH (relative to the case's directory) natively, or under the same emulator `$VO_BIN` runs under,
  with stdout, stderr and exit status reported like a `vo` step; documented in the module docstring.
- [X] T002 [P] `tools/quick.sh`: a new area `build` (cases `build`, `cli`, `run`, `replay`, `packages`;
  scripts `catalog.py`, `ref_examples.py`, `build_examples.py`) listed among the known areas;
  `src/build.c` and `tests/build_examples.py` map to it.
- [X] T003 [P] `specs/034-vo-build/results.md`: SC-001–SC-006 pending, Verification, Deviations (empty).

---

## Phase 2: Foundational (blocking)

- [X] T004 `src/platform/x86_64.h`, `src/platform/aarch64.h`, `src/platform/riscv64.h`, `src/sys.c`,
  `src/vo.h`: `SYS_lseek` and `SYS_fchmod` for the three architectures; `sys_lseek(fd, off, whence)`,
  `sys_fchmod(fd, mode)` and `sys_pread(fd, buf, n, off)` (the `SYS_pread64` number exists).
- [X] T005 `src/build.c` (new), `src/vo.h`: the format of data-model.md in one place: the trailer
  ("magic `VO-PROGRAM-v1\0\0\0`", "payload offset (= the base's length)", "payload length", "SHA-256 of
  the payload", 64 bytes, little-endian) and the payload records (`u64 kind`, `u64 length`, bytes;
  kinds 1 version text, 2 entry path, 3 file path, 4 file bytes, 5 manifest path); `build_payload(...)`
  producing the bytes and `app_open()` that opens `/proc/self/exe`, reads the last 64 bytes and returns
  0 (no magic: a toolchain), or the parsed payload after checking "magic, offset + length + 64 = file
  size, hash". The version text is one constant shared with `vo version` (`src/main.c`).
- [X] T006 `src/main.c`: `vo_main` calls `app_open()` first; with no magic nothing changes (every
  existing case passes: `make quick AREA=cli`).

**Checkpoint**: the toolchain looks for a trailer at start and finds none.

---

## Phase 3: User Story 1 - Build a program into one file and run it anywhere (P1) 🎯 MVP

**Goal**: `vo build main.vo -o app`; `./app ARGS` behaves as `vo run main.vo -- ARGS`.

**Independent Test**: a multi-file program built, moved to an empty directory and run with arguments
and stdin gives `vo run`'s output and exit status.

- [X] T007 [P] [US1] Step cases first, `tests/cases/build/`: `basic` (`vo build main.vo -o out/app`,
  `rm main.vo`, `app out/app a b < in.txt` printing `args()` and stdin; the `built ...` line; `vo build
  --json`), `multi` (an entry importing its own files in a subdirectory, a package via `vo package`,
  and `std`; sources and `vo.packages` removed before the run), `errors` (a check error: diagnostics as
  `vo check`, exit 65, no file; a program without `main`), `record_mismatch` (`vo.effects` out of date:
  E0504, no file), `runtime_error` (an index error in the app: R0003 with the source position, exit
  70; `main` returning `err`: exit 1), `output` (default OUTPUT name; an existing OUTPUT replaced; `-o`
  naming a source file refused T0001; an OUTPUT in a missing directory T0009 exit 74, nothing left
  behind), `same_bytes` (two builds to two names, compared by a small vo program that reads both
  files as bytes and prints whether they are equal), `moved` (the app renamed and run from another directory),
  `damaged` (the app's last bytes rewritten with `write`: T0014, exit 66, nothing runs).
- [X] T008 [US1] `src/build.c`, `src/main.c`: the `build` command (`cmd_names`, the command lists in
  the two T0001 messages, options `-o OUTPUT` and `--base FILE`): `check_program` in run mode plus the
  effects-record review as `vo check` does; on errors `finish_check`; collect `ld_files()` that are not
  `std` (paths and bytes as loaded; package files from `pkg_cached`) and the manifest; refuse an OUTPUT
  that is a loaded source file or the toolchain; write base + payload + trailer to `tmp_path`, `fchmod
  0755`, rename; T0009 and unlink on any write failure; the `built OUTPUT: N files, SIZE bytes` line or
  the `build` JSON record.
- [X] T009 [US1] `src/build.c`, `src/loader.c`, `src/packages.c`: app start (R4): register every
  embedded file with `file_add`, set `ld_app`; `load_file` never reads the disk in app mode;
  `inside_tree` answers 1; `pkg_load` uses the embedded manifest; `pkg_verify` accepts; the E0705
  directory probe is skipped; `ld_app` cleared before the program runs. T0014 `damaged_executable`
  (exit 66) from `app_open()`'s checks, with its `docs/errors.md` entry and `034-vo-build` in
  `tests/catalog.py`'s spec list.
- [X] T010 [US1] `src/build.c`, `src/main.c`: the default app command (R5): arguments not starting with
  `--vo-` become the program's arguments in order; the run is `vo run --timeout 0 ENTRY -- ARGS`
  through the ordinary option handling (reports on stderr as `vo run`).
- [X] T011 [US1] Accept T007 after reading each output; `make quick AREA=build`.

**Checkpoint**: a built program runs anywhere on its architecture.

---

## Phase 4: User Story 2 - The executable states what it may touch (P1)

**Goal**: `./app --vo-effects` prints what `main` declares; the grant is enforced.

**Independent Test**: a program that `uses console, file.read("data")`: `--vo-effects` prints exactly
that line and runs nothing.

- [X] T012 [P] [US2] Step case first, `tests/cases/build/effects`: `app out/app --vo-effects` prints
  `main uses console, file.read("data")` and nothing else (the program's own output absent);
  `--vo-effects --vo-json` prints the `capabilities` record of `main`; a pure `main` prints `main pure`;
  a second program that reads a file outside its scope is refused at run time exactly as `vo run`
  (same report).
- [X] T013 [US2] `src/build.c`, `src/effects_record.c` (or `src/main.c`): `--vo-effects` checks the
  embedded program and prints the `main` line of `vo effects` (text or JSON), exit 0; accept T012.

---

## Phase 5: User Story 3 - Toolchain options behind a prefix (P2)

**Goal**: `--vo-NAME` for every `vo run` option; `--vo-record`/`--vo-replay`; `--vo-version`.

**Independent Test**: `./app --vo-record r.jsonl --seed 3 x` then `./app --vo-replay r.jsonl` give the
same output; the program saw `--seed 3 x`.

- [X] T014 [P] [US3] Step cases first, `tests/cases/build/`: `options` (a program printing `args()`,
  `random_int` and the clock: `--vo-seed 3 --vo-clock-ms 5 --seed x -- --help` reaches the program as
  `--seed x -- --help`; `--vo-timeout 1` on a looping program exits 124; `--vo-timeout 0` accepted (the
  missing default timeout is not tested by waiting: too slow); `--vo-max-memory 1` gives R0006; `--vo-mem-stats`; `--vo-trace t.jsonl`
  kept; `--vo-json` on a runtime error; `--vo-bogus` and `--vo-seed x`: T0001 listing the app options,
  the program not run; `--vo-version`), `record_replay` (`--vo-record r.jsonl` with stdin, arguments
  and a file read; data removed; `--vo-replay r.jsonl` gives the same output and status; a recording
  made by `vo run --record` of the same sources replays with the app).
- [X] T015 [US3] `src/build.c`: the option table (`seed`, `clock-ms`, `timeout`, `max-memory`,
  `mem-stats`, `trace`, `trace-limit`, `record`, `ca-file`, `workers`, `json`) rewritten to the
  toolchain's options with their values; `--timeout 0` added unless `--vo-timeout` is given; unknown
  `--vo-` options T0001 with the list; `--vo-version` as `vo version`.
- [X] T016 [US3] `src/build.c`, `src/replay.c`: `--vo-replay FILE` runs `vo replay` with the embedded
  entry in place of the recording's program path (R7); accept T014.

---

## Phase 6: User Story 4 - Builds for the other architectures (P2)

**Goal**: `vo build --base OTHER_TOOLCHAIN` gives an app for that architecture.

**Independent Test**: `--base build-aarch64/vo` on x86-64 gives an app that runs under `qemu-aarch64`
with the same output.

- [X] T017 [P] [US4] Step case first, `tests/cases/build/base`: `--base` naming a text file (not an
  ELF executable), a missing file, and a built app (already a built program): each T0001 naming the
  problem, nothing written; `--base` naming a copy of the toolchain itself builds an app that runs.
- [X] T018 [US4] `src/build.c`: `--base FILE` checks (ELF magic, the version text present, no trailer)
  per R8; accept T017.

---

## Phase 7: User Story 5 - Documented where agents and users read (P3)

- [X] T019 [P] [US5] `docs/reference.md`, Command line: the line `vo build     [--json] [-o OUTPUT]
  [--base TOOLCHAIN] FILE` and one bullet (one executable holding the program; `./app ARGS`; options as
  `--vo-` plus the `vo run` name, `--vo-effects`, `--vo-replay`, no default timeout);
  `python3 tests/ref_examples.py` (≤ +120 tokens).
- [X] T020 [P] [US5] `site/docs/guide/shipping.md` (new, linked wherever the guide lists its pages):
  build, run, `--vo-effects`, recording and replay, `--base` for ARM64 and RISC-V, a `FROM scratch`
  Dockerfile (`COPY app /app`, `ENTRYPOINT ["/app"]`; note that `/proc` is needed, which containers
  mount); shell blocks only where `tests/site_examples.py` cannot run them.
- [X] T021 [P] [US5] `docs/roadmap.md` (row and section for 034), `README.md` (a `vo build` line),
  `docs/review-2026-11/README.md` ("Fixed since the review" row); T0009's catalog entry gains the build
  case.

---

## Phase 8: Polish & cross-cutting

- [X] T022 `tests/build_examples.py` (R11) and its line in the Makefile's `test` script list: every
  `examples/*/main.vo` built into a temporary directory; each `recordings/*.jsonl` replayed with
  `vo replay` and with `app --vo-replay` from an empty directory: stdout, stderr and status equal
  (SC-001); one run recorded with `--vo-record` and replayed by the app (SC-002); two builds
  byte-identical (SC-003).
- [X] T023 SC-004: start time of the largest example's app against `vo run` of it (paired, nine
  rounds, on a replay so no network is used); figures in results.md.
- [X] T024 `make test` (via `tools/progress.py run test`); then `make ARCH=aarch64` and
  `make ARCH=riscv64`, `tests/cases/build` under each (`VO_BIN`), and SC-005: on x86-64
  `vo build --base build-ARCH/vo` of an example, the app run under qemu, output compared; not the gate
  unless asked.
- [X] T025 `specs/034-vo-build/results.md`: SC-001–SC-006 with measurements, Verification, Deviations;
  spec status.

---

## Dependencies & Execution Order

- Setup (T001–T003) → Foundational (T004–T006) → US1 (T007–T011).
- US2 (T012–T013), US3 (T014–T016) and US4 (T017–T018) each need US1 only. US5 (T019–T021) needs
  US1–US4 for accurate text.
- Polish needs everything; T022 needs US3 (replay).

## Parallel opportunities

- T002 and T003 with T001; the step-case tasks marked [P] (T007, T012, T014, T017) touch only new files
  and can be written ahead of their code.
- T019, T020 and T021 are documentation in different files.

## Implementation Strategy

- **MVP**: Phases 1–3 (T001–T011): build and run, with refusals and the damage check.
- **Then** US2 (the stated grant), US3 (options, record and replay), US4 (other architectures), US5
  (docs), each testable alone, and the Polish phase.
- `make quick AREA=build` between edits; `make test` once at T024.
