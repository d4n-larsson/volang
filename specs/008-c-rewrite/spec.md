# Feature Specification: The Toolchain in C (Native Builds)

**Feature Branch**: `008-c-rewrite`

**Created**: 2026-11-14

**Status**: Draft

**Input**: User description: "c rewrite"

## Context

`vo` is written in RISC-V assembly: about 53,000 lines in 45 files. On the machines people actually use (x86-64 and ARM64), it runs only through an emulator (`qemu-riscv64`). That costs speed. It also causes emulator-only effects, such as the page-placement workarounds in the VM, the map and the list code. And every change pays the price of hand-written assembly: register allocation and calling conventions are done by hand and checked by an external linter (SESI). A whole class of bugs (overwritten save slots, clobbered registers) is found only by that linter or by crashes. During spec 007, two such bugs reached a running program because the linter did not check those functions.

This feature rewrites the toolchain in C, so it builds natively on ordinary machines with ordinary tools. The language, the command line, every output byte and every error code stay exactly as they are. The rewrite is invisible to the agents that use `vo`. The existing test suite (golden and step cases, fuzzers, engine comparisons, the agent benchmark's reference runs, the speed benchmark) is the specification: the C toolchain is correct when it passes all of it.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - vo runs natively on an ordinary machine (Priority: P1)

A developer or an agent on an x86-64 Linux machine builds `vo` from a fresh checkout with standard tools and runs it directly, without an emulator. Every command behaves exactly as the assembly toolchain did.

**Why this priority**: this is the point of the feature. Without it, nothing else in this spec matters.

**Independent Test**: on an x86-64 Linux machine with no emulator installed, build from a clean checkout, then run `make test`. Every golden case, step case, reference example and catalog example passes byte for byte.

**Acceptance Scenarios**:

1. **Given** a clean checkout on x86-64 Linux, **When** the developer runs the documented build command, **Then** a native `vo` is produced and `bin/vo` runs it without the emulator.
2. **Given** the native build, **When** the full test suite runs, **Then** every case passes with byte-identical stdout, stderr and exit status.
3. **Given** a program with a runtime error, a timeout, an out-of-memory condition or a recording, **When** it runs natively, **Then** the report, exit status and recording are exactly those of the assembly toolchain.

---

### User Story 2 - Same results on every platform and against the assembly version (Priority: P1)

The same program, input, seed and clock give byte-identical output on every supported platform, and the same output the last assembly release gave. This includes float formatting, integer edge cases, memory limits, diagnostics, JSON, recordings, effects records and package hashes. Agents can rely on what they saw once.

**Why this priority**: determinism is a constitution principle (III). A port that changes even one byte of output silently breaks agents' expectations, recorded runs and golden files.

**Independent Test**: run a differential check of thousands of generated programs through the last assembly release (under emulation) and the C build on each platform, plus the agent benchmark's reference runs. There must be zero disagreements.

**Acceptance Scenarios**:

1. **Given** 10,000 generated programs, **When** each runs on the last assembly release and on the C build, **Then** stdout, stderr and exit status agree for every one.
2. **Given** a recording made on RISC-V, **When** it is replayed on x86-64 (and the reverse), **Then** the replay output is byte-identical.
3. **Given** a float computation that rounds differently with fused multiply-add or extended precision, **When** it runs on each platform, **Then** the result bits are identical.
4. **Given** integer division of the minimum integer by -1, or by zero, **When** it runs on a platform whose hardware traps on these, **Then** vo reports the same documented runtime error as on RISC-V, not a crash.

---

### User Story 3 - The migration never breaks the tree (Priority: P2)

The maintainer ports the toolchain in steps. After every step, the whole suite passes, so the work can stop, pause for other features, or be reviewed at any point without a half-working toolchain.

**Why this priority**: a 53,000-line rewrite done in one piece risks a long period where nothing works and bugs cannot be located. Stepwise passing builds keep each bug local to the step that introduced it.

**Independent Test**: for each recorded migration step, check out that state and run the full quality gate. Every step passes.

**Acceptance Scenarios**:

1. **Given** any recorded migration step, **When** the full gate runs (golden and step cases, fuzzers, determinism, collector stress, engine comparison, speed check), **Then** it passes.
2. **Given** a migration step that changes output, **When** the gate runs, **Then** the failing case points at the behavior that step touched.

The port goes file by file. C compiled for RISC-V links with the remaining assembly, since both use the same calling convention and data layouts. The mixed toolchain runs under emulation and passes the full gate after every file. The build switches to native once no assembly is left. Leaf code goes first; the VM and the collector go last.

---

### User Story 4 - Faster native runs (Priority: P2)

Checks, test runs and program runs finish sooner on ordinary machines, because no instruction is emulated.

**Why this priority**: speed is a direct benefit to every agent loop, but correctness (US1, US2) comes first.

**Independent Test**: run the project's speed benchmark with the native build and with the assembly build under emulation on the same machine, and compare.

**Acceptance Scenarios**:

1. **Given** the speed benchmark, **When** it runs natively on x86-64, **Then** it is at least 3 times faster (geometric mean) than the assembly toolchain under emulation on the same machine, and no program is slower.
2. **Given** the C toolchain built for RISC-V, **When** it runs under emulation, **Then** it is no more than 10% slower (geometric mean) than the assembly build was, so the RISC-V target does not regress.

---

### User Story 5 - Cheaper and safer to change (Priority: P3)

A contributor changes the toolchain with ordinary C tools: a debugger, memory-error and undefined-behavior detectors, compiler warnings. The per-edit checks that SESI gave for assembly are replaced by checks that cover the C code.

**Why this priority**: it pays off on every later spec, but it does not change what users see.

**Independent Test**: build the instrumented variant (memory-error and undefined-behavior detection) and run the full suite under it. There must be zero reports, and the strict-warning build must have zero warnings.

**Acceptance Scenarios**:

1. **Given** the instrumented build, **When** the full suite runs, **Then** no memory error, leak of a kind the collector should prevent, or undefined behavior is reported.
2. **Given** a change that overwrites memory out of bounds, **When** the instrumented suite runs, **Then** the report names the faulty function and line.

---

### Edge Cases

- **Deep nesting and recursion**: documented limits (parser nesting, call depth, stack exhaustion reports such as E0115) must trigger at the same points and with the same reports, although native stack frames differ in size.
- **Hardware traps**: x86-64 traps on integer division by zero and on the minimum integer divided by -1, while RISC-V does not. Both must stay defined runtime errors.
- **NaN and signed zero**: platforms produce NaNs with different signs and payloads. No output may depend on them, for example `str(0.0 / 0.0)` and JSON refusals.
- **Float contraction**: a compiler may fuse multiply-add or keep extended precision. Results must be bit-identical to the assembly toolchain's IEEE double operations.
- **Memory figures**: `--max-memory` limits and `--mem-stats` reports count objects of the same sizes as today, so a figure differs from the assembly release only where the old toolchain kept unreachable objects alive by accident (the FR-001 exception). Every figure is identical across platforms, and every difference from the assembly release is recorded.
- **Collector stress mode**: the build that collects every N allocations must still exist and give byte-identical output.
- **System calls**: system call numbers, structures and missing calls differ per architecture (for example, no `open` on RISC-V or ARM64). Every file, clock, random, network and process behavior must stay identical.
- **Timeouts and signals**: `--timeout`, interrupted runs and child processes must end with the same statuses and reports. The toolchain installs no signal handlers (timeouts are deadline checks), and the C toolchain keeps it that way.
- **Emulator-only workarounds**: the page-placement pins exist only for the emulator. They must not carry over, and their removal must not change output.
- **Embedded files**: the language reference, the error catalog and the standard library must still be built into the single executable.
- **Mixed states** during the file-by-file port: code in both languages must agree on data layouts and calling conventions at every boundary.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Every observable behavior of `vo` MUST be identical to the last assembly release on every supported platform. That covers each command and option, stdout and stderr bytes, JSON schemas, exit statuses, diagnostics and fixes, runtime errors, recordings, traces, effects records, package manifests and hashes, and `vo doc` output. The single exception is memory figures (`--mem-stats`, and where `--max-memory` is reached): the C toolchain may retain less unreachable memory than the assembly release did, so a figure may be smaller. Such a figure MUST still be identical on every supported platform, and every difference from the assembly release MUST be recorded.
- **FR-002**: The toolchain MUST be written in C and build from source with a standard C compiler, `make` and Python 3 (for build-time generation of embedded data and headers), with no other build dependency. Python is never part of the shipped executable.
- **FR-003**: The shipped toolchain MUST remain one static, self-contained executable per platform, with the language reference, error catalog and standard library embedded. It MUST link no C library: it uses only the Linux system call interface, through the platform layer (FR-005), as today.
- **FR-004**: The toolchain MUST build and pass the full suite on Linux on x86-64, ARM64 and RISC-V 64 (RISC-V and ARM64 may run under emulation on an x86-64 development machine).
- **FR-005**: After the port, no hand-written assembly MAY remain, except a small per-platform layer for system calls and context switching of at most 300 lines per platform.
- **FR-006**: The existing test suite MUST run unchanged against the C build: golden and step cases, the reference and catalog example checks, the fuzzers, the determinism check, the collector stress mode, the agent benchmark's reference runs and the speed benchmark. Only how the harness finds and launches the binary may change.
- **FR-007**: A differential check MUST compare the C build against the last assembly release (run under emulation) on generated programs, and report every disagreement with the program that caused it.
- **FR-008**: Floating-point results MUST be bit-identical across platforms and to the assembly toolchain. Integer edge cases (division by zero, minimum integer divided by -1, overflow) MUST keep their documented runtime errors on every platform.
- **FR-009**: Documented limits MUST trigger at the same points with the same reports: nesting depth, call depth (including the depth of vo code re-entered from each builtin that calls back, such as `map`, `filter` and `sort_by`, measured per builtin on the assembly release), memory limit, timeouts and response sizes.
- **FR-010**: Recordings, effects records and package manifests MUST be interchangeable between platforms. A file written on one platform gives identical results on another.
- **FR-011**: An instrumented build variant (memory-error and undefined-behavior detection) MUST exist, and the full suite MUST pass under it with no reports.
- **FR-012**: The C sources MUST compile with the strict warning set the plan chooses, with zero warnings. Per-edit checks MUST replace the SESI assembly lint for C code.
- **FR-013**: Workarounds that exist only for the emulator (the pinned page offsets) MUST be removed.
- **FR-014**: The constitution MUST be amended before implementation starts. Its implementation constraints must name C, the supported platforms and the replacement per-edit checks. Assembly-specific rules no longer apply to toolchain code.
- **FR-015**: `bin/vo` MUST run the native build on supported hosts, and keep running a RISC-V build through the emulator where one is used (the oracle for FR-007).
- **FR-016**: The documentation MUST be updated: README build instructions, the roadmap (native running done), and any text that tells users to use the emulator. The language reference and error catalog MUST NOT change otherwise.
- **FR-017**: No language, command-line or output change MAY be made as part of this feature. A behavior change found to be desirable during the port is recorded as a follow-up, not made.
- **FR-018**: The port MUST proceed file by file. C code and the remaining assembly link into one toolchain at every step, and each step MUST pass the full quality gate before the next begins (US3).

### Key Entities

- **Last assembly release**: the final commit of the assembly toolchain (after spec 007). It is the behavioral oracle for the differential check and the speed baseline.
- **C toolchain**: the C sources that replace `src/*.s`, organized by the same responsibilities (lexer, parser, checker, compiler, VM, collector, runtime, commands).
- **Platform layer**: the small per-platform part (system calls, register and stack access for the collector, non-local exits).
- **Migration step**: one recorded unit of the port, with its gate result (US3).
- **Quality gate**: the existing `make test` set plus the differential check, the instrumented build and the speed checks.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On each supported platform, 100% of the golden cases (over 400), the step cases (over 90) and every example in the language reference and error catalog pass with byte-identical output.
- **SC-002**: 10,000 generated programs give 0 disagreements between the last assembly release and the C build on each supported platform. All 703 reference runs of the agent benchmark are byte-identical.
- **SC-003**: On an x86-64 machine, the speed benchmark runs natively at least 3 times faster (geometric mean) than the assembly toolchain under emulation on the same machine, and no program is slower.
- **SC-004**: The C toolchain built for RISC-V and run under emulation is at most 10% slower (geometric mean) than the assembly build on the speed benchmark.
- **SC-005**: The full test suite passes under memory-error and undefined-behavior detection with 0 reports, and the strict-warning build has 0 warnings.
- **SC-006**: A new contributor builds the native toolchain from a clean checkout with one documented command, in under 1 minute on a typical laptop.
- **SC-007**: Every replay case recorded on one platform replays byte-identically on every other supported platform.
- **SC-008**: Hand-written assembly is at most 300 lines per supported platform (down from about 53,000).
- **SC-009**: The language reference and error catalog are unchanged apart from instructions on how to build and run `vo`.

## Assumptions

- Spec 007 (standard library and packages) is merged first, with parts deferred (HTTP networking, some tests and polish; its results.md "Deferred"). The port starts from that tree, and the merge commit is the "last assembly release". The deferred 007 work is finished after the port, in C, so the behavior freeze (FR-017) holds during the port.
- The behavior is frozen during the port: no feature work lands in the toolchain until the port is complete (FR-017), so the oracle stays valid.
- Data layouts that affect observable output (object sizes counted by the memory limit and memory reports) are kept, so memory figures match.
- The test suite is the specification. A difference not covered by a test, and not documented, is still treated as a bug when found.
- The developer scripts (Python, shell) stay as they are; only how they locate and launch the binary may change.
- The agent benchmark's new-agent runs are not repeated: the language is unchanged, and SC-002's reference runs cover the behavior.
- Linux only: macOS and Windows are out of scope (both would need a C library, which FR-003 excludes).
- During the port, the mixed toolchain is a RISC-V build run under emulation; native x86-64 and ARM64 builds exist only once the last assembly file is ported.
