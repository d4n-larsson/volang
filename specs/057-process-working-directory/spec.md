# Feature Specification: Process Working Directory

**Feature Branch**: `057-process-working-directory`

**Created**: 2026-11-25

**Status**: Implemented (2026-11-25, vo 0.3.0)

**Input**: User description: "std.process: run a program in a given working directory. Today std.process
(run, run_with, run_lines and the _bytes forms) always starts the program in vo's start directory and has
no way to choose another, so a vo tool that drives other programs (for example tools/vo_fix.vo running
`vo check` and `vo effects --update` on an entry, or a build tool running `make` or `git` in a checkout)
cannot run them where they expect to be run; it works only when the program happens to accept absolute
paths. Add a way to choose the working directory of a started program. Settle: the API shape under 'one
way to write each thing' (a new parameter on the existing functions vs. a new function vs. an options
record, and how existing callers migrate, e.g. through the library change table and E0214 fixes); how the
directory is checked against the caller's grant (file scopes are relative to the start directory; the
process scope names the program as written, and a relative program path like 'tools/gen' must resolve the
same way it does today, from the start directory, not the new one); errors for a missing or non-directory
path; and that recording and replay include the directory. Per-call timeouts are out of scope:
std.workers.with_deadline already stops the call and kills the child."

Background: porting `tools/vo_fix.py` to vo (2026-11-25) showed the gap. The Python tool ran
`vo check --json main.vo` with the entry's directory as the working directory, so the files in the
diagnostics were named relative to the entry. The vo port can only pass absolute paths, which works for
`vo` but not for programs that act on "the current directory" (`make`, `git`, most build and conversion
tools). `tools/progress.vo` works around it today by running from the repository root only.

Decided with the user on 2026-11-25:
1. **API shape:** a directory argument on `run_with`, `run_with_bytes` and `run_lines`; `run` and
   `run_bytes` unchanged. Existing calls of the three get E0214 with a fix that passes `"./"`.
2. **Grant:** the directory must lie inside a `file.read` scope of the caller, so a reviewer sees in the
   effects where programs run.

What this spec must keep:
- **Stability (`docs/stability.md`):** a breaking library change comes with E0214 and, wherever one
  correction exists, a mechanical fix; existing programs migrate by applying fixes alone.
- **Principle III:** the same program and inputs give the same output on x86-64, ARM64 and RISC-V; the
  directory is part of what a recording holds and replay matches.
- **Principle V:** the reference stays within 15,000 tokens; the cost of this change is stated.
- **Principle I:** one way to write each thing: no second function that does what an existing one does.

## User Scenarios & Testing *(mandatory)*

The users are AI coding agents writing vo programs that start other programs (tools, workers that call
converters or version control, build helpers), the people who review those programs, and the owners of
existing programs that use `std.process`.

### User Story 1 - Run a program where it expects to run (Priority: P1)

An agent writes a worker that checks out a repository into `jobs/42/` and runs `git status` and `make`
there, or a tool that runs `vo check main.vo` in an entry's directory. It names the directory in the call;
the program starts there and sees relative paths from there. Today it can only start in vo's own start
directory.

**Why this priority**: it is the gap. Without it, a whole class of programs (anything that acts on the
current directory) cannot be driven from vo at all.

**Independent Test**: a golden case runs a program that prints its working directory and lists a relative
file, once in a named subdirectory and once in the start directory, and shows both outputs.

**Acceptance Scenarios**:

1. **Given** a directory `work/` holding `a.txt`, **When** a program runs `ls` with `work/` as its
   directory, **Then** the output lists `a.txt`.
2. **Given** the start directory as the directory, **When** a program runs, **Then** it behaves exactly as
   a run today does.
3. **Given** a program path written relative (`tools/gen`) and the directory `work/`, **When** it runs,
   **Then** `tools/gen` is the file under the start directory (as the `process` scope names it), not
   `work/tools/gen`, and it starts in `work/`.
4. **Given** streaming (`run_lines`) and byte (`_bytes`) forms, **When** they are given a directory,
   **Then** each starts the program there in the same way.

---

### User Story 2 - A wrong directory is a value, not a surprise (Priority: P1)

The directory is missing, is a file, or cannot be entered. The call returns an `err` with a short reason
the program can match, and nothing is started. A directory outside what the function is allowed to name
is refused like a file outside its scopes.

**Why this priority**: the program must never run somewhere other than where it was told, and an agent
needs a reason it can act on.

**Independent Test**: golden cases for a missing directory, a file given as the directory, and a directory
outside the grant, each with its result or runtime error.

**Acceptance Scenarios**:

1. **Given** a directory that does not exist, **When** a program is run there, **Then** the result is
   `err("directory not found")` and no program starts.
2. **Given** a regular file as the directory, **When** a program is run there, **Then** the result is
   `err("not a directory")`.
3. **Given** a directory outside the caller's grant, **When** a program is run there, **Then** it is
   refused (R0010 at run time; E0503 at check time when the directory is a literal).

---

### User Story 3 - Existing programs keep working through a fix (Priority: P2)

The owner of a program written for vo 0.2 upgrades. Each call that the change affects reports E0214 with a
fix that keeps its behavior (it runs in the start directory, as before). After applying the fixes the
program checks clean and gives the same output.

**Why this priority**: required by the stability rules for any breaking change; the number of affected
calls is small (the repository has 26, mostly in tests).

**Independent Test**: the repository's own callers (tests, tools, std) migrate by applying fixes alone,
with unchanged golden output.

**Acceptance Scenarios**:

1. **Given** `run_with("vo", args, "", vars)` written for 0.2, **When** it is checked, **Then** it reports
   E0214 with one fix giving `run_with("vo", args, "", vars, "./")`, and after the fix it checks clean and
   runs as before.
2. **Given** a 0.2 caller that declares no `file.read` scope covering the start directory, **When** the fix
   is applied, **Then** the checker reports what the call now needs (E0501 with its fix adding
   `file.read("./")`), so fixes alone still migrate it.

---

### User Story 4 - Recordings hold the directory (Priority: P2)

A program that runs `git` in `jobs/42/` is recorded and replayed. Replay gives the recorded output. A
changed program that runs the same command in another directory stops with R0014, as one with other
arguments does.

**Why this priority**: the directory changes what the started program does, so a replay that ignored it
would accept a different program as the same.

**Independent Test**: a golden record/replay case with a directory, and one where the replayed program
names another directory and gets R0014.

**Acceptance Scenarios**:

1. **Given** a recording of a run in `work/`, **When** the same program is replayed, **Then** the output
   and exit status are identical, and nothing is started.
2. **Given** the same recording, **When** a program that runs the same command in `other/` is replayed,
   **Then** it stops with R0014 naming the run.

### Edge Cases

- The directory is `"./"`: the start directory, as in scopes.
- The directory is an empty string: refused the same way an empty path is elsewhere in the library.
- The directory contains `..`, is absolute, or is a symbolic link: resolved (links followed) and checked
  against the grant as file paths are; the program starts in the resolved directory.
- The directory exists but cannot be entered (no search permission): `err` with the system's reason;
  nothing is started.
- A bare program name (`git`) is found through `PATH` as today; the directory does not change the lookup.
- The program changes or removes its own directory while running: its affair; vo does not watch it.
- The directory is removed between the check and the start: the call returns `err`, never starts the
  program elsewhere.
- A 0.2 caller whose `file.read` scopes exist but do not cover the start directory (`file.read("data/")`
  only): after the E0214 fix passes `"./"`, the literal is outside its scopes (E0503). The plan MUST make
  fixes alone still migrate such a caller (for example, E0503's fix widening the scope), or show none
  exists in the repository and state the manual step in the changelog.
- `vo test` with a fixed clock and seed: the directory is part of the run's identity in recordings of
  tests too.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The library MUST let a program name the working directory of every program it starts, in
  each of the run forms (text, bytes, streaming lines).
- **FR-002**: The directory MUST be an argument of the forms that already take input and variables
  (`run_with`, `run_with_bytes`, `run_lines`), after the variables; `run` and `run_bytes` MUST stay as they
  are (start directory). No new function names are added.
- **FR-003**: A relative directory MUST be resolved from the start directory; `"./"` MUST mean the start
  directory.
- **FR-004**: The program path MUST be resolved as it is today (a path with `/` from the start directory,
  a bare name through `PATH`), independent of the directory, and MUST still match the `process` scope
  exactly as written.
- **FR-005**: A missing directory MUST give `err("directory not found")`, a non-directory
  `err("not a directory")`, and one that cannot be entered `err("directory not accessible")`; in each case no
  program starts. The texts differ from a missing program's `err("not found")`, so a program can tell them
  apart.
- **FR-006**: The directory MUST lie inside one of the caller's `file.read` scopes, checked as file paths
  are (after resolving `..`, absolute paths and symbolic links): outside is R0010 at run time, and a literal
  outside is E0503 at check time. The `process` scope still names the program. `"./"`, the start directory
  where `run` starts programs, is always allowed (decided in implementation: results.md "Deviations").
- **FR-007**: A recording MUST hold the directory of each run as written (as it holds the program and
  arguments), and replay MUST treat a run in another directory as a different input (R0014). A run in
  `"./"` MUST record exactly as a run did before this change, so recordings made with vo 0.2 replay against
  migrated programs.
- **FR-008**: Each existing call of the three changed forms MUST report E0214 with a single fix that keeps
  the call's behavior (the start directory), registered in the library change table so the catalog and
  changelog list it.
- **FR-009**: The started program's environment, input, output limits, process group and `--timeout`
  behavior MUST be unchanged by the directory.
- **FR-010**: The reference, `vo doc` for each changed function (with a runnable example), the error
  catalog's E0214 list and the changelog MUST describe the directory in the same change.

### Key Entities

- **Run**: one start of a program: the program as written, its arguments, input, variables, and now its
  working directory; its result is the exit code and output.
- **Working directory**: a directory path, relative to the start directory or absolute, resolved and
  checked before the run; part of a run's identity in recordings.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: `tools/vo_fix.vo` runs `vo check` and `vo effects --update` in each entry's own directory,
  and its "left:" lines name files relative to the entry, as the Python tool did; `tests/migrate_056.py`
  and `tests/libchange_rows.py` give the same results as before.
- **SC-002**: Every existing caller in the repository (26 calls) migrates by applying fixes alone, and every
  golden case's output is unchanged.
- **SC-003**: A program run in a missing, non-directory, or ungranted directory never starts: 0 of the
  edge-case golden cases show the program's output.
- **SC-004**: Replaying a recording made with a directory gives byte-identical output; a replay in another
  directory stops with R0014 in 100% of cases.
- **SC-005**: Output is identical on x86-64, ARM64 and RISC-V for every new golden case.
- **SC-006**: The reference stays within 15,000 tokens (it has about 10 to spare today): the words this
  change adds are paid for by trimming in the same change, and the net change is stated.

## Assumptions

- Per-call time limits are out of scope: `std.workers.with_deadline` already stops a run and kills the
  started program (verified 2026-11-25: a 7-second `sleep` stopped at 0.31 s, no process left).
- The directory is not created by the call: a program that needs a fresh directory makes it with
  `std.fs.make_dir` first.
- The started program stays unconfined (as `docs/reference.md` says today); the directory does not limit
  what it can reach, it only sets where it starts.
- The child's environment stays empty unless variables are given; no `PWD` variable is added.
- Error texts follow the style of the `std.fs` texts, with "directory" where a bare text would be read as
  the program's (`"directory not found"`).
- Version: this is a minor-version change before 1.0 (0.2 → 0.3), allowed by `docs/stability.md` with
  E0214 and fixes.
