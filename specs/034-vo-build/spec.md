# Feature Specification: One Executable per Program (`vo build`)

**Feature Branch**: `034-vo-build`

**Created**: 2026-11-20

**Status**: Implemented (gate not run)

**Input**: User description: "`vo build main.vo -o app`: a vo program as one executable. Today a vo
program is run as `vo run main.vo` from its source tree, so shipping one means shipping the toolchain,
the sources, `vo.effects` and `vo.packages`. The binary already embeds the standard library; let it
embed a checked program too and write a copy of itself with the program attached. The result is one
static executable for each of the three architectures. The grant is baked in and can be printed:
`./app --vo-effects` shows what `main` declares. Flags like `--seed`, `--record` and `--max-memory` stay
available behind a `--vo-` prefix. Source: docs/review-2026-11/ecosystem.md, Distribution (review item
8). Must keep: determinism, record and replay, effects enforced as under `vo run`, diagnostics with
fixes, no libc, the reference under 15,000 tokens (about 14,622 now)."

Background: a vo program runs as `vo run main.vo` inside its source tree. To give it to someone, you
give them the toolchain, the source files, the packages it imports and its effects record, and they need
to know the command line. Every mainstream compiled language ships one file instead. vo's binary is
already static, has no libc and carries the standard library, so a program can travel the same way: one
file that runs on any Linux machine of its architecture. vo can also do something those languages
cannot: the executable states, and enforces, what it may touch.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Build a program into one file and run it anywhere (Priority: P1)

A developer runs `vo build main.vo -o app` and copies `app` to another machine with no vo toolchain and
no source tree. `./app input.csv` runs the program with those arguments and behaves exactly as
`vo run main.vo -- input.csv` does.

**Why this priority**: it is the feature.

**Independent Test**: build `examples/url-check` (or a multi-file example) into one file, move it to an
empty directory, run it with arguments and stdin; its output and exit status equal those of `vo run`
on the same arguments and input.

**Acceptance Scenarios**:

1. **Given** a program that checks clean, **When** `vo build main.vo -o app` runs, **Then** it writes one
   executable file `app` and reports its size; nothing else is written.
2. **Given** `app` in a directory with no `.vo` files, no `vo.effects` and no `vo.packages`, **When** run
   with arguments, **Then** the program gets them through `args()` exactly as after `vo run ... --`, and
   its stdout, stderr and exit status equal `vo run`'s.
3. **Given** a program with imports of its own files, of packages and of the standard library, **When**
   built, **Then** everything it imports is inside `app`; deleting the source tree changes nothing.
4. **Given** a program with check errors, **When** `vo build` runs, **Then** it reports the diagnostics as
   `vo check` does (with fixes) and writes no file.
5. **Given** a program whose effects differ from its `vo.effects` record, **When** built, **Then** the
   build refuses as `vo check` reports it (E0504), so what was reviewed is what ships.
6. **Given** a runtime error in `app`, **When** it happens, **Then** the report names the program's own
   source positions, as under `vo run`.
7. **Given** the same sources and the same toolchain, **When** built twice, **Then** the two files are
   byte-identical.

---

### User Story 2 - The executable states what it may touch (Priority: P1)

Whoever receives `app` can ask it what it may do before running it: `./app --vo-effects` prints the
effects `main` declares, with their scopes, and the program cannot do anything else.

**Why this priority**: it is what makes a vo executable different from any other; without it the
feature is only packaging.

**Independent Test**: a program that `uses console, file.read("data")` built into `app`;
`./app --vo-effects` prints exactly those effects; the program runs with them and nothing more.

**Acceptance Scenarios**:

1. **Given** a built `app`, **When** run with `--vo-effects`, **Then** it prints the effects and scopes
   `main` declares, in the form `vo effects` uses, and runs nothing.
2. **Given** `--vo-effects --vo-json`, **When** run, **Then** the same as one JSON record.
3. **Given** a running `app`, **When** the program attempts an effect outside its grant, **Then** it is
   refused exactly as under `vo run` (the grant cannot be widened from the command line).

---

### User Story 3 - Toolchain options behind a prefix (Priority: P2)

The options `vo run` takes (`--seed`, `--clock-ms`, `--timeout`, `--max-memory`, `--mem-stats`,
`--trace`, `--record`, `--ca-file`, `--workers`, `--json`) are available to an `app` as `--vo-seed`,
`--vo-record` and so on; every other argument goes to the program unchanged. A recording made by the
app replays with the app.

**Why this priority**: determinism and record/replay are vo's guarantees; a shipped program must keep
them, but must also be able to take its own `--help` or `--seed`.

**Independent Test**: `./app --vo-record r.jsonl --seed 3 x` passes `--seed 3 x` to the program and
records the run; `./app --vo-replay r.jsonl` reproduces the output and exit status.

**Acceptance Scenarios**:

1. **Given** arguments without the `--vo-` prefix (including `--help`, `--seed`, `--`), **When** `app`
   runs, **Then** all of them reach the program in order.
2. **Given** `--vo-seed N`, `--vo-clock-ms N`, `--vo-timeout SEC`, `--vo-max-memory MiB`,
   `--vo-mem-stats`, `--vo-trace FILE`, `--vo-ca-file FILE`, `--vo-workers N`, `--vo-json`, **When**
   given, **Then** each behaves as the same option of `vo run`.
3. **Given** `--vo-record FILE`, **When** the app runs, **Then** it records as `vo run --record` does; and
   `./app --vo-replay FILE` replays it as `vo replay` does, with no files, network or input.
4. **Given** an unknown `--vo-` option or a bad value, **When** given, **Then** a usage error (T0001)
   lists the `--vo-` options, and the program does not run.
5. **Given** `./app --vo-version`, **When** run, **Then** it prints the toolchain version it was built
   with.

---

### User Story 4 - Builds for the other architectures (Priority: P2)

A developer on x86-64 builds the same program for ARM64 and RISC-V without those machines.

**Why this priority**: vo supports three architectures; a program should ship to all three from one
machine.

**Independent Test**: on x86-64, build `app-arm64` from the ARM64 toolchain binary; under an ARM64
emulator it gives the same output as the x86-64 `app`.

**Acceptance Scenarios**:

1. **Given** the toolchain binary for another architecture (a file), **When** `vo build` is told to use
   it as the base, **Then** the output runs on that architecture with the same behavior.
2. **Given** a base that is not a vo toolchain binary, or one of another version than the building
   toolchain, **When** used, **Then** the build refuses with a usage error naming the problem.

---

### User Story 5 - Documented where agents and users read (Priority: P3)

The reference's command-line section documents `vo build` and the `--vo-` options in a few lines; the
site documents shipping a program, including a container image with nothing but the executable.

**Why this priority**: an agent that does not know `vo build` exists will ship sources.

**Independent Test**: the reference lists `vo build` and the app options and stays under 15,000
tokens; the site page's container example builds an image that runs the program.

**Acceptance Scenarios**:

1. **Given** the reference, **When** read, **Then** `vo build` and the `--vo-` options are in the command
   line section.
2. **Given** the site, **When** read, **Then** a page shows building, running, `--vo-effects` and a
   `FROM scratch` container.

---

### Edge Cases

- `-o` names an existing file: it is replaced, as compilers do; `-o` naming a source file of the program
  or the toolchain itself is refused.
- The output's directory does not exist or is not writable: a usage error, nothing written.
- `-o` left out: the output is the entry file's name without `.vo`, in the current directory.
- A program that runs tests only (no `main`): E-code as for `vo run` (no `main`), nothing written.
- The executable is renamed or moved: it still runs (it does not depend on its name or location).
- The executable is truncated or its program part damaged: it refuses to start with a clear message
  and a non-zero status, never runs a partial program.
- A program that reads its own source files at run time by path: it reads the disk, as `vo run` would;
  the embedded copies are only for loading the program.
- Relative paths the program opens (`file.read("data")`): relative to the current directory, as under
  `vo run`.
- `app` started with no arguments: the program runs with no arguments.
- A built `app` used as a toolchain (`./app check x.vo`): the arguments go to the program; an app is
  never a toolchain.
- `vo build` of an app's own output as the base (an app as base): refused; the base must be a toolchain.
- Very large programs: the build and the start stay proportional to the program's size.

## Requirements *(mandatory)*

### Functional Requirements

**Building**

- **FR-001**: `vo build ENTRY [-o OUTPUT]` MUST check the program as `vo check` does (including the
  effects record) and, only when it checks clean, write one executable file holding the toolchain's
  runtime and everything the program loads (its files, package files, nothing from outside).
- **FR-002**: The same sources and toolchain MUST give a byte-identical executable.
- **FR-003**: A build MUST write nothing but the output file, and nothing at all when it fails.
- **FR-004**: `vo build` MUST accept a base toolchain file for another architecture, and refuse one that
  is not a vo toolchain binary of the same version.

**Running**

- **FR-005**: A built executable MUST run the program exactly as `vo run ENTRY -- ARGS` would (same
  output, exit status, runtime reports, effects enforcement), with no source tree, `vo.effects`,
  `vo.packages` or toolchain present.
- **FR-006**: Every argument that does not start with `--vo-` MUST reach the program unchanged and in
  order.
- **FR-007**: The executable MUST not depend on its own file name or directory.
- **FR-008**: The executable MUST refuse to start, with a message and a non-zero status, when its
  program part is missing or damaged.

**Options**

- **FR-009**: `--vo-effects` MUST print the effects and scopes `main` declares and run nothing.
- **FR-010**: Each `vo run` option MUST be available as `--vo-` plus its name, with the same meaning; the
  grant MUST NOT be widened by any option.
- **FR-011**: `--vo-record FILE` and `--vo-replay FILE` MUST record and replay as `vo run --record` and
  `vo replay` do; a recording made by an executable replays with the same executable.
- **FR-012**: `--vo-version` MUST print the toolchain version; an unknown `--vo-` option MUST be a usage
  error (T0001) that lists the options.

**Documentation and tests**

- **FR-013**: The reference MUST document `vo build` and the `--vo-` options and stay under 15,000
  tokens; the site MUST have a page on shipping a program with a container example.
- **FR-014**: Every new mistake MUST have a stable code with a catalog entry, or reuse an existing one.

### Key Entities

- **Executable (app)**: one file, the toolchain's runtime plus the program; behaves as the program.
- **Program part**: the program's checked source files and package files, the entry's name and the
  toolchain version, with a check that it is intact.
- **Base**: the toolchain binary the executable is made from; the building one by default.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every example program, built and run from an empty directory, gives byte-identical output
  and exit status to `vo run` on the same arguments and input.
- **SC-002**: A recording made with an executable replays with it to the same output, on all examples
  that record.
- **SC-003**: Building the same program twice gives byte-identical files.
- **SC-004**: An executable starts no slower than `vo run` of the same program (both check before
  running), measured on the largest example.
- **SC-005**: An executable built on x86-64 with the ARM64 or RISC-V base runs on that architecture with
  the same output.
- **SC-006**: The reference stays under 15,000 tokens with `vo build` documented.

## Assumptions

- The executable is the toolchain binary with the program attached, so it is about the toolchain's size
  (about 10 MB). Stripping unused parts of the runtime is not part of this spec.
- The executable re-checks its embedded program when it starts (a check is milliseconds), rather than
  storing a compiled form; the user-visible guarantee is FR-005 and SC-004.
- No timeout by default: a shipped program is not an agent's test run. `--vo-timeout SEC` sets one. The
  memory limit stays 512 MiB by default (`--vo-max-memory`).
- Cross-architecture builds use the other architecture's toolchain binary as the base (built with
  `make ARCH=...` or downloaded); `vo build` never contains three runtimes.
- Linux only, as the toolchain. Signing, compression, and a smaller runtime are out of scope.
- `vo test`, `vo check` and the other tools are not available inside an executable.
