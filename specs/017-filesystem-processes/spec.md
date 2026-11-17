# Feature Specification: Filesystem and Processes

**Feature Branch**: `017-filesystem-processes`

**Created**: 2026-11-16

**Status**: Implemented

**Input**: User description: "Filesystem and process support and add the other features to the roadmap"
(the gaps listed after spec 016: whole-file `read_file`/`write_file` are the only file operations, and a
program cannot run another program).

Background: a vo program can read a file and replace a file, and nothing else. It cannot list a
directory, tell whether a path exists, append to a log, create a directory, delete or rename a file, or
run `git`, a compiler or a formatter. These are the first things a build script, a file organizer, a
report generator or an agent's own tooling needs, so today such programs cannot be written in vo at all.
What this spec must keep: effects as a security boundary (security.md S1-S3: a program does nothing
outside its grant, and a reviewer reads the grant from `main`'s `uses` clause and `vo.effects`);
determinism (Principle III: the same program and inputs give the same bytes, and `--record`/`vo replay`
reproduce a run); and the reference budget (Principle V).

## Clarifications

### Session 2026-11-16

- Q: Once a granted program is started, is it limited to the vo program's grant? → A: No: the scope says
  which programs may start and they run with the user's full rights, stated plainly; confining children
  to the grant is planned as a later opt-in.
- Q: Do deleting and renaming need their own effect? → A: Yes, a new `file.delete` effect with directory
  scopes; rename needs it for the old path.
- Q: How does running a program work? → A: Run to completion only: one call waits and returns the exit
  code and captured output.

## User Scenarios & Testing *(mandatory)*

The users are the same as in specs 001–016: AI coding agents that write vo programs from the reference and
`vo doc`, and the people who review and run those programs.

### User Story 1 - Work with the files in a directory (Priority: P1)

An agent writes a program that goes through a directory of notes (or logs, or CSV exports), reads each
file, and writes a summary. It lists the directory, tells files from subdirectories, skips what is not a
file, and walks subdirectories too. The listing comes back in the same order on every run and every
machine.

**Why this priority**: the most common missing piece; almost every file-processing program starts with
"for each file in this directory", and today the program has to be given every file name by hand.

**Independent Test**: golden cases over a fixture directory with files, subdirectories and a symbolic
link: the listing, kinds and sizes are printed identically on every run; a path outside the
`file.read` scope is refused.

**Acceptance Scenarios**:

1. **Given** a directory `data/` with files `b.txt`, `a.txt` and a subdirectory `old/`, **When** a
   program declared `uses file.read("data/")` lists `data/`, **Then** it gets `a.txt`, `b.txt` and `old`
   in byte order of their names, each with its kind (file or directory), without `.` and `..`.
2. **Given** the same program, **When** it asks whether `data/a.txt`, `data/old` and `data/none` exist
   and what they are, **Then** it learns "file" (with its size in bytes), "directory" and "missing",
   without a runtime error for the missing one.
3. **Given** a program declared `uses file.read("data/")`, **When** it lists `secrets/` or `data/../`,
   **Then** it stops with R0010 exactly as `read_file` would; a literal path outside the scope is E0503
   at check time.
4. **Given** a directory that cannot be read (permissions) or a path that is a file, **When** the
   program lists it, **Then** it gets an `err` result with a stable message, not a runtime error.
5. **Given** a symbolic link inside the scope that points inside it, **When** the directory is listed,
   **Then** the link is reported with the kind of what it points to; a link leading outside the scope is
   listed but cannot be read through (as today for `read_file`).

---

### User Story 2 - Change files: append, create directories, delete, rename (Priority: P1)

An agent writes a program that appends a line to a log on each run, creates `out/2026-11/` before
writing a report into it, moves processed files from `inbox/` to `done/`, and deletes temporary files it
created. Each operation that can fail (missing file, directory not empty, name already taken) returns an
`err` the program can match on.

**Why this priority**: with only "replace the whole file", a program cannot keep a log, cannot organize
output into directories, and leaves its temporary files behind.

**Independent Test**: golden cases in a scratch directory: after each operation the case prints the
directory's listing and the file's contents; every refusal (outside the scope, wrong kind, already
exists) is checked separately.

**Acceptance Scenarios**:

1. **Given** `log.txt` with one line, **When** the program appends a second line, **Then** the file has
   both lines; appending to a missing file creates it.
2. **Given** no `out/` directory, **When** the program creates `out/2026-11/` with its parents, **Then**
   both directories exist; creating one that already exists as a directory succeeds; where a file is in
   the way, it is an `err`.
3. **Given** `inbox/a.txt`, **When** the program renames it to `done/a.txt`, **Then** the file is in
   `done/` and gone from `inbox/`; renaming onto an existing name is an `err` and changes nothing.
4. **Given** a file and an empty directory, **When** the program deletes them, **Then** they are gone;
   deleting a directory that is not empty, or a missing path, is an `err` and changes nothing.
5. **Given** a program declared `uses file.write("out/"), file.delete("out/")`, **When** it deletes,
   renames or appends to a path outside `out/` (for a rename: either end), **Then** it stops with R0010,
   and a literal path outside the scope is E0503 at check time; a function that deletes or renames with
   only `file.write` is E0501.
6. **Given** a local process that swaps a directory for a symbolic link while the program works (A4),
   **When** the program deletes, renames or appends, **Then** the operation never reaches outside the
   scope (as spec 013 guarantees for `write_file` today).
7. **Given** an existing program, **When** it changes so that a function now deletes or renames files,
   **Then** `vo check` reports E0504: "gains file.delete", because deleting and renaming need the
   `file.delete` effect (clarified 2026-11-16), not only `file.write`.

---

### User Story 3 - Run another program (Priority: P1)

An agent writes a build or release script: it runs `git status --porcelain`, a test runner and a
formatter, reads their output and exit codes, and decides what to do next. The program names which
programs it may run in its `uses` clause, so a reviewer sees "this program runs git and gcc" before
running it.

**Why this priority**: scripts that drive other tools are among the most common programs agents write,
and today vo cannot write any of them.

**Independent Test**: golden and step cases with small test programs (one that echoes its arguments and
stdin, one that exits with a given code, one that sleeps, one that prints a lot): output, exit code and
refusals are checked; replay runs without starting any program.

**Acceptance Scenarios**:

1. **Given** a program declared `uses process("git")`, **When** it runs `git` with the arguments
   `["status", "--porcelain"]`, **Then** it gets the exit code, the standard output and the standard
   error as values, after `git` has finished.
2. **Given** an argument containing spaces, quotes, `;` or `$(...)`, **When** the program runs a
   command with it, **Then** the argument reaches the program as one unchanged argument: there is no
   shell, so nothing in an argument is interpreted.
3. **Given** a program declared `uses process("git")`, **When** it tries to run `rm` (or `/bin/sh`, or
   `git` by a path that is a different file), **Then** it stops with R0010; a literal program name
   outside the scope is E0503 at check time, and a function that runs programs without declaring it is
   E0501.
4. **Given** text to send to the program's standard input, **When** the program runs, **Then** the
   child reads exactly that text, then end of input.
5. **Given** a program that cannot be found, or that exits with a non-zero code, or that is killed by a
   signal, **When** it is run, **Then** "not found" is an `err` result; a non-zero exit or a signal is a
   normal result carrying the code, so the program can match on it.
6. **Given** a child that runs longer than `--timeout`, **When** the deadline passes, **Then** the child
   is stopped and the vo program stops with R0009, as for any other timeout; no child process outlives
   the vo program.
7. **Given** a run recorded with `vo run --record`, **When** it is replayed, **Then** it gives the same
   output and exit status without starting any program.
8. **Given** a child that writes more than the output limit, **When** it runs, **Then** the vo program
   gets an `err` (and the child is stopped) instead of running out of memory.
9. **Given** a grant `process("git")`, **When** a reviewer reads it, **Then** the reference and
   security.md state that it allows starting `git` and that `git` then runs with the user's full rights,
   outside the vo program's other scopes (clarified 2026-11-16: confinement is a planned opt-in).

---

### User Story 4 - Review sees what a program may touch and run (Priority: P2)

A reviewer gets a change from an agent. In `vo.effects` they see "release gains process(\"git\")" or a
file scope widening, and they see it without reading the code; `vo effects main.vo` lists which programs
`main` may run.

**Why this priority**: the reason vo's effects exist; the new operations are useful only if they keep
that review working. P2 because it falls out of stories 1-3 when effects are designed right, and is
tested separately.

**Independent Test**: step cases that update `vo.effects`, change a program to run a new command or
delete files, and check that `vo check` reports exactly that change (E0504 with the call path).

**Acceptance Scenarios**:

1. **Given** a program and its `vo.effects`, **When** a function starts running `make` as well as
   `git`, **Then** `vo check` reports E0504 naming the function, the new program and the call that
   brought it in.
2. **Given** `vo effects main.vo`, **When** `main` may run `git` and `make`, **Then** the output lists
   both under `main`'s capabilities.
3. **Given** a program that only lists and reads directories, **When** its effects are printed,
   **Then** no write or process capability appears.

---

### User Story 5 - Everything stays deterministic and testable (Priority: P2)

An agent writes tests for its file and process code with `vo test`, using a fixtures directory and a
scratch directory, and the same tests give the same output on every run and on all three platforms.
Traces show each file operation and each program run; a recorded run replays exactly.

**Why this priority**: Principle III is non-negotiable for the toolchain, and agents rely on
reproducible tests; P2 because the mechanisms exist and must be extended, not invented.

**Independent Test**: the new golden and step cases run 20 times with identical results, on x86-64,
ARM64 and RISC-V; a recording made on one platform replays identically on the others.

**Acceptance Scenarios**:

1. **Given** a test declared `uses file.read("fixtures/"), process("./tests/echo")`, **When** `vo test`
   runs it twice, **Then** the results are byte-identical.
2. **Given** `vo run --trace`, **When** the program lists a directory and runs a program, **Then** the
   trace has a record for each, with the path or the program and arguments, in the order they happened.
3. **Given** a recording that includes listings, file information and program runs, **When** `vo replay`
   runs a program that does the same operations, **Then** it reads every answer from the recording and
   touches no file and starts no program; an operation that differs from the recording is reported as
   today.

---

### Edge Cases

- **Names that are not UTF-8**: a directory entry whose name is not valid UTF-8 cannot be a `str`; it is
  reported in a way that is stable and cannot be confused with another name (decided in planning), and
  the rest of the listing is still returned.
- **Program output that is not UTF-8**: binary output cannot be a `str`; the run returns an `err` saying
  so (until a bytes type exists), not a corrupted string.
- **Huge directories**: a directory with 100,000 entries lists within the time and memory limits.
- **Renaming a directory into itself**, renaming across file systems, or renaming over a directory: an
  `err`, nothing changed.
- **Deleting the scope directory itself** (`out/` with `uses file.delete("out/")`): refused (R0010 or an
  `err`), because a scope that disappears invites a link swapped in its place.
- **A child that inherits nothing it was not given**: no open files of the vo program, no environment
  variables except those the call passes explicitly, and the directory the vo program started in as its
  working directory.
- **A child that starts its own children** and exits: those grandchildren must not keep the vo program
  waiting forever, and are covered by `--timeout`.
- **A child that waits for input** that never comes: it gets end of input at once (no stdin unless
  given).
- **Running programs inside `concurrent_map` tasks**: works, with output and results in input order as
  spec 016 guarantees; the per-task limit and the timeout apply.
- **A program name with a slash** (`./tools/gen`, `/usr/bin/git`) versus a bare name (`git`): the scope
  says which one is granted, and a bare name is found the same way on every run (decided in planning).
- **Paths with a NUL byte**, empty paths and empty program names: refused, as for `read_file` today.
- **`vo check`, `fmt`, `doc`, `query`, `edit` on hostile source** still never list, change or run
  anything (S6).

## Requirements *(mandatory)*

### Functional Requirements

**Reading the file system**

- **FR-001**: A program MUST be able to list a directory: the names of its entries (without `.` and
  `..`), each with its kind (file, directory, other), sorted by the bytes of the name. The operation
  needs `file.read` with a scope covering the directory.
- **FR-002**: A program MUST be able to ask what is at a path: missing, a file (with its size in bytes),
  a directory, or something else, as a value rather than an error. It needs `file.read` covering the
  path. Modification times are not part of this spec.
- **FR-003**: Walking a directory tree MUST be possible with FR-001 in a few lines of vo; a standard
  library function for it is added only if it fits the reference budget (FR-020).

**Changing the file system**

- **FR-004**: A program MUST be able to append text to a file, creating it if missing.
- **FR-005**: A program MUST be able to create a directory together with any missing parents; an
  existing directory is success, anything else in the way is an `err`.
- **FR-006**: A program MUST be able to delete a file or an empty directory. Deleting a directory with
  contents is an `err`; there is no recursive delete in this spec.
- **FR-007**: A program MUST be able to rename (move) a file or directory; both the old and the new path
  must be inside the write scope; an existing target is an `err` and nothing changes.
- **FR-008**: Appending and creating directories need `file.write` with a scope covering every path they
  change. Deleting needs a new effect, `file.delete`, covering the path; renaming needs `file.write`
  covering the new path and `file.delete` covering the old one, since it removes a name (clarified
  2026-11-16). `file.delete` has directory scopes like `file.write`; `file.write` alone never allows
  deleting or renaming.
- **FR-009**: Every failure that depends on the state of the file system (missing, exists, not empty,
  wrong kind, permission denied, no space) MUST be an `err` result with a stable, documented message;
  runtime errors are reserved for scope violations (R0010) and invalid arguments (R0007).

**Running programs**

- **FR-010**: A program MUST be able to run another program with a list of arguments and optional text
  for its standard input, wait for it to finish, and get its exit code, standard output and standard
  error. No shell is involved, so no argument is interpreted. A program killed by a signal is reported
  with a code that says so.
- **FR-011**: Running a program MUST need a new effect, `process`, whose scope names the programs that
  may be run (a bare name, a path relative to the starting directory, or an absolute path: amended in
  planning, research R3); an unscoped `process` is not allowed (E0512). Literal names outside the scope are E0503 at check time; every run is checked again at run
  time against the running function's and the root grant (R0010), after resolving the name to the file
  that will be executed.
- **FR-012**: A started child is not confined: it runs with the rights of the user running vo, outside
  the vo program's file, network and environment scopes. The `process` scope controls only which programs
  may be started. The reference and security.md MUST say so plainly (granting a program grants everything
  it can do), and the roadmap MUST list confining children to the grant as a planned opt-in (clarified
  2026-11-16).
- **FR-013**: A child MUST get only what the call gives it: its arguments, its standard input text (or
  none), the environment variables the call passes explicitly (no inheritance), and the starting
  directory as its working directory. No file descriptor of the vo program other than the three pipes
  reaches it.
- **FR-014**: `--timeout` MUST cover child programs: at the deadline the child and anything it started
  are stopped and the vo program reports R0009. No child outlives the vo program, even when the vo
  program is killed.
- **FR-015**: Each of standard output and standard error MUST be limited (16 MiB, as HTTP responses);
  over the limit the child is stopped and the call returns an `err`.
- **FR-016**: Running a program is one call that waits for the child to finish and returns its exit
  code and captured output. There is no live passthrough to the terminal and no handle to a running
  child (clarified 2026-11-16).

**Effects, review and determinism**

- **FR-017**: `vo.effects`, `vo effects FILE`, E0501/E0503/E0504 and the canonical effect order MUST
  cover the new operations and the new effects exactly as they cover the existing ones; the canonical
  order places `file.delete` after `file.write` and `process` last.
- **FR-018**: Directory listings and file information MUST be identical on every run and platform for
  the same file system contents; their answers, program outputs and exit codes are inputs, recorded by
  `--record` and fed back by `vo replay`, which touches no file and starts no program (S7). The
  recording's format version changes only if an existing reader cannot read the new events.
- **FR-019**: `--trace` MUST write one record per file-system operation and program run, in the order
  they happened.
- **FR-020**: The language reference grows by at most 500 tokens, every new builtin, effect and error
  code has a checked example, and every new code has a catalog entry with an example and a fix
  (Principle II).
- **FR-021**: security.md MUST be updated: the new effect in the boundary table, S2 and S3 covering
  directory operations and process creation, and what a granted program may do.
- **FR-022**: Existing programs MUST behave exactly as before: every existing golden and step case
  unchanged, on all three platforms.

**Roadmap**

- **FR-023**: `docs/roadmap.md` MUST list the other gaps found after spec 016 as planned work: HTTPS,
  math builtins, binary data, sleep, regular expressions, string formatting, TCP/UDP sockets, macOS,
  downloading packages, a language server, a debugger, tuples and more map key types, `old()` in
  `ensures`, the benchmark v2 run, and confining child programs to the grant (an opt-in).

### Key Entities

- **Directory entry**: a name and a kind (file, directory, other).
- **Path information**: missing, or a kind, and for a file its size in bytes.
- **Program run**: a program (as granted), arguments, standard input text, explicit environment
  variables; its result is an exit code (or signal), standard output and standard error.
- **`process` effect**: a capability with a scope listing the programs that may be run.
- **`file.delete` effect**: a capability with directory scopes for deleting and for removing a name by
  renaming.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: An agent-sized program that walks a directory tree of 1,000 files, reads them and writes a
  summary works in vo, and its output is byte-identical over 20 runs and on all three platforms.
- **SC-002**: A build script that runs three external programs, inspects their exit codes and output,
  and writes a report, is written in vo in at most 1.5 times the lines of the equivalent Python script.
- **SC-003**: Every new operation refuses every path or program outside its grant, at check time for
  literals and at run time otherwise: 100% of the scope cases in the test suite, including link-swap
  (A4) cases for every operation that changes files.
- **SC-004**: Listing a directory of 100,000 entries takes under 1 second, and running a trivial
  program 1,000 times takes under 5 seconds, with no leaked processes or open files afterwards.
- **SC-005**: A run that lists, changes files and runs programs replays from its recording with
  byte-identical output and starts no program and touches no file, on every platform pair.
- **SC-006**: A reviewer sees every new capability a change grants in the `vo.effects` diff: 100% of the
  step cases that add a program, widen a scope or add `file.delete` fail `vo check` with E0504
  naming the change.
- **SC-007**: The reference grows by at most 500 tokens; the existing suite passes unchanged.

## Assumptions

- **Children are not confined (clarified 2026-11-16)**: the `process` scope says which programs may
  start, and a started program can do anything the user can. Kernel confinement (Landlock) was
  considered: it needs a recent kernel, is uncertain under qemu-user, breaks many tools under a narrow
  file grant, and limits the network only on newer kernels. It is planned as an opt-in instead.
- **Deleting is its own effect (clarified 2026-11-16)**: destructive changes are what a reviewer most
  needs to see, so "gains file.delete" appears in `vo.effects` even for a function that could already
  write. Rename needs it because it removes the old name.
- **Run to completion (clarified 2026-11-16)**: one call, captured output; it fits build scripts,
  records and replays exactly, and keeps spec 016's structured lifetimes. Live passthrough and handles
  to running children were left out.

- **Strings, not bytes**: file contents, names and program output stay `str` (UTF-8). Binary data waits
  for a bytes type (roadmap); until then non-UTF-8 output is an `err`.
- **No shell**: running "a command line" is deliberately not offered; a program needing a pipeline runs
  the programs one after another and passes output along. `sh -c` is possible only by granting `sh`,
  which a reviewer then sees.
- **Bare program names** are found through a fixed search, resolved the same way at check and run time;
  the exact rule (PATH at start, or a fixed list) is decided in planning.
- **Children's environment is explicit**: passing variables to a child does not need the `env` effect
  (they are values the program already has); reading a variable to pass it still does.
- **No permissions, owners, modification times, links or copying**: changing modes and owners,
  creating links, and copying (read plus write covers it) are left out; modification times are a
  candidate follow-up for build tools.
- **No recursive delete**: an agent's mistaken `rm -rf` is the failure most worth preventing; a program
  that must clear a tree walks it.
- **Working directory**: children run in the directory the vo program started in; a per-call working
  directory is left out (most tools take a directory argument).
- **Tasks**: file operations and program runs work inside `concurrent_map` with spec 016's guarantees.
- **HTTPS, math, sleep and the other gaps** are not part of this spec; FR-023 only records them on the
  roadmap.
- **The previous release is the oracle** for "behaves exactly as before".
