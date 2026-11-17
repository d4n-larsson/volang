# Research: Filesystem and Processes

Decisions for spec 017, from reading the current code: `src/effects.c` (effect bits and scopes),
`src/check_effects.c` (builtin effects, literal targets), `src/rt_effects.c` (`rt_check_path`,
`rt_resolve`, `rt_open_file`), `src/replay.c` (events), `src/vm.c` (traces), `src/serve.c` (signalfd),
`src/task.c` (fork), `std/http.vo` (a module over `__` primitives).

## R1. Surface: two standard-library modules over two primitives

- **Decision**: add `std.fs` and `std.process`, written in vo like `std.http`, over two internal
  primitives callable only from `std/`: `__fs(op, a, b) -> result[str]` and
  `__run(program, args, input, env) -> result[str]`. The modules turn the primitives' text into records
  and variants.
- **Rationale**: the reference does not list library items (`vo doc` does), so a module costs one row
  in the modules table, not one row per function (Principle V, FR-020). Library functions can return
  records and variants, which builtins cannot. Library calls already get everything this spec needs:
  their effects count like a builtin's for the caller (E0501), they run with the caller's grant
  (R0010 in the primitive), and `--trace` already writes their `call` and `return` records with
  arguments and results (checked: `pad_left` and its helpers appear in a trace), which covers FR-019
  with no new trace kind. A primitive that returns one `str` makes recording and replay one string per
  event.
- **Alternatives**: builtins next to `read_file` (about 25 reference tokens each, no records); one
  module `std.os` (mixes two effects agents should keep apart).

## R2. The API

- **Decision** (exact text in contracts/language.md):
  - `std.fs`: `variant Info { missing; file(int); directory; other; }`,
    `record Entry { name: str; info: Info; link: bool; }`, `info(path) -> result[Info]`,
    `list_dir(path) -> result[list[Entry]]`, `walk(dir) -> result[list[str]]`,
    `append_file(path, text) -> result[void]`, `make_dir(path) -> result[void]`,
    `delete(path) -> result[void]`, `rename(from, to) -> result[void]`.
  - `std.process`: `record Output { code: int; stdout: str; stderr: str; }`,
    `run(program, args) -> result[Output]` and
    `run_with(program, args, input, env) -> result[Output]` (env: `list[str]` of `NAME=value`).
- **Rationale**: `info` returns `missing` as a value (FR-002), so "does it exist" needs no error
  handling. `Entry.link` lets `walk` (and programs) avoid descending through links to directories,
  which would loop. `walk` is a few lines of vo over `list_dir` and costs no reference tokens (FR-003).
  `run` covers the common case in two arguments; `[]` is E0306, so a single four-argument function
  would make every call pass `"", new list[str]`.
- **Alternatives**: one `run` with four arguments (noisy at every call); a `map[str, str]` for the
  environment (`new map[str, str]` is longer than `new list[str]`, and `NAME=value` is what `env(1)`
  and execve use).

## R3. Effects: `file.delete` and `process`

- **Decision**: insert `file.delete` after `file.write` and add `process` last in `es_names`; the
  `EF_*` bits are renumbered (`EF_NET` 32 ... `EF_SERVE` 512, `EF_PROCESS` 1024, `EF_ALL` 2047), so the
  canonical order (the bit order) is console, input, file.read, file.write, file.delete, net, clock,
  random, env, serve, process (FR-017). `builtin_effects` stays `unsigned short`.
  - `file.delete` has directory scopes, normalized like `file.write`'s.
  - `process` scopes are program names: a bare name (no `/`), a relative path, or an absolute path;
    no `..` component, no trailing `/`, no NUL; relative paths lose a leading `./`. Anything else is
    E0506.
  - `uses process` without a scope is a new check error, **E0512** `process_unscoped`, except in
    `std/` (library declarations are unscoped by design: the caller's scopes apply).
  - E0509's message lists the eleven effects.
- **Rationale**: bit order is the canonical order everywhere (`fx_render`, `vo.effects`, `vo fmt`), so
  renumbering is the smallest change that puts `file.delete` beside the other file effects. Nothing
  stores bit values: `vo.effects`, recordings and JSON use names. An absolute program path is the most
  precise grant a reviewer can read, so it is allowed (amends FR-011, which named only bare and
  relative forms). An unscoped `process` would grant every program on the machine, which makes the
  grant meaningless; E0512 refuses it rather than letting E0506 describe it badly.
- **Alternatives**: new bits at the end with a separate order table (two orders to keep in sync);
  allowing unscoped `process` (honored as written, like unscoped `net`, but `net` cannot run code).
- **Behavior change**: E0509 golden outputs list two more effects; everything else is unchanged.

## R4. Check-time literal targets (E0503)

- **Decision**: `fx_call`'s standard-library branch, which already registers `serve`'s address, gets a
  small table of library functions and the argument that is a target: `info`, `list_dir`, `walk`
  (argument 0, `file.read`); `append_file`, `make_dir` (0, `file.write`); `delete` (0, `file.delete`);
  `rename` (0, `file.delete`; 1, `file.write`); `run`, `run_with` (0, `process`), matched by module
  file and name. `fx_literal` takes the argument index. Program literals are compared with the
  normalized scopes exactly (like env names); paths as today (resolved, under a scope).
- **Rationale**: the spec 013 mechanism (L1) applies unchanged; only the table grows.

## R5. Safe file-system operations under a scope

- **Decision**: split `rt_open_file` into `rt_open_parent(bit, path, &name)`, which returns a
  descriptor of the parent directory of the resolved path, reached from the deepest granted scope
  directory one component at a time with `O_NOFOLLOW` (spec 013's walk), and the last component's name.
  Every operation then uses the `*at` system call on that descriptor:
  - `info`: `statx(parent, name, AT_SYMLINK_NOFOLLOW)` (the resolved path has no links left);
  - `list_dir`: the walk opens the directory itself (`O_DIRECTORY | O_NOFOLLOW`), then `getdents64`;
  - `append_file`: `openat(parent, name, O_WRONLY | O_APPEND | O_CREAT | O_NOFOLLOW)`;
  - `make_dir`: `mkdirat` for each missing component below the scope (and the scope directory itself
    and its parents, created from the start directory when missing); `EEXIST` on a directory is
    success;
  - `delete`: `statx` to learn the kind, then `unlinkat(parent, name, 0 | AT_REMOVEDIR)`; a link is
    deleted as a link (its target is untouched);
  - `rename`: two parents, `renameat2(old_parent, old, new_parent, new, RENAME_NOREPLACE)`.
  `delete` and both ends of `rename` must lie strictly inside a scope: the scope directory itself is
  refused (R0010), so a scope cannot be removed and replaced by a link. An unscoped grant works from the
  start directory's descriptor, as `rt_open_file` does today.
- **Rationale**: spec 013 already proves the walk on every platform and under qemu (no `openat2`), and
  the `*at` calls never re-resolve the path, so a link swapped in after the check cannot redirect any
  operation (A4, SC-003). `RENAME_NOREPLACE` makes "an existing target is an `err` and nothing
  changes" (FR-007) atomic instead of check-then-rename.
- **System calls**: `mkdirat` is new (x86-64 258, generic 34); `getdents64`, `unlinkat`, `renameat2`
  and `statx` are already in the platform headers (used by `src/sys.c`).
- **Alternatives**: path-based calls after `rt_check_path` (the check-then-open race spec 013 fixed);
  `openat2(RESOLVE_BENEATH)` (missing under qemu-user, research R3 of spec 013).

## R6. Listings

- **Decision**: `getdents64` with a 64 KiB buffer until the end; `.` and `..` dropped; names that are
  not valid UTF-8 dropped (the rest is returned); kind from `d_type`, with `statx` for `DT_UNKNOWN` and
  for sizes of files. A symbolic link is `link: true` with the kind of its target when the target
  resolves inside one of the running grant's `file.read` scopes, otherwise `other` (so a listing never
  reveals what lies outside the grant). Sorted by name bytes in C with a merge sort, then encoded as
  entries joined by `/` (which no name contains): a kind letter (`f` with the size, `d`, `o`), `l` when
  a link, a space, the name.
- **Rationale**: dropping non-UTF-8 names is stable and cannot be confused with another name (the
  spec's edge case), until a bytes type exists. `/` and NUL are the only bytes a name cannot hold, and
  `/` survives JSON in recordings as itself. Sorting in C keeps 100,000 entries well under a second
  (SC-004); `statx` per file is the main cost.
- **Alternatives**: escaping invalid bytes (`\xNN` collides with real names); returning the
  directory's own order (differs between file systems and runs).

## R7. Errors as values

- **Decision**: every errno from these operations becomes an `err` with a fixed message: `ENOENT`
  "not found", `EEXIST`/`ENOTEMPTY` on rename "already exists", `ENOTEMPTY` on delete "not empty",
  `ENOTDIR` "not a directory", `EISDIR` "is a directory", `EACCES`/`EPERM` "permission denied",
  `ENOSPC` "no space left", `EXDEV` "on another file system", `EROFS` "read-only file system",
  `EINVAL` from `RENAME_NOREPLACE` "not supported on this file system", anything else "error N".
  `info` maps `ENOENT` and `ENOTDIR` on a component to `ok(missing)`. R0010 stays for scopes; R0007 for
  invalid arguments (empty path, NUL byte, a bad `NAME=value`).
- **Rationale**: FR-009; the messages are part of the reference's contract and checked by golden cases.

## R8. Which program runs

- **Decision**: the program text is checked against the `process` scopes of the running and the root
  grant (R0010), exactly as written after the scope normalization (a leading `./` dropped). Then it is
  resolved: a path with `/` from the start directory (`execve` of the start directory's path joined
  with it, or the absolute path); a bare name through the `PATH` of the `vo` process at the time of the
  call, absolute entries only, the first regular file with an execute bit (`statx`). No match is
  `err("not found")`. Replay resolves nothing.
- **Rationale**: exact text is what a reviewer reads and approves: `process("git")` allows `git`, not
  `/usr/bin/git` or `./git`; `process("/usr/bin/git")` allows only that file. Resolving through `PATH`
  is what every shell and agent expects; `vo` does not change its own environment, so the lookup is
  the same during a run. Relative `PATH` entries are skipped because they would depend on the working
  directory.
- **Known limit**: whoever can replace the granted program's file (A4) controls what runs; this is
  stated in security.md, and it is no different from running that program by hand.

## R9. Starting the child

- **Decision**: `sys_fork` (spec 016's `clone(SIGCHLD)`), then in the child: `setpgid(0, 0)` (its own
  process group), `prctl(PR_SET_PDEATHSIG, SIGKILL)` and the `getppid` race check (as tasks do), an
  empty signal mask (serve blocks SIGINT/SIGTERM, which exec would inherit), `dup3` of three pipes onto
  0/1/2, `fchdir` to the start directory, and `execve(path, argv, envp)` with `argv[0]` the program
  text and `envp` exactly the `NAME=value` strings given (none for `run`). If `execve` fails, the child
  writes the errno to a fourth pipe (close-on-exec, so it closes unread on success) and exits 127; the
  parent reads it: nothing means started, `ENOENT` is "not found", `EACCES` "permission denied".
  All of the toolchain's descriptors are already close-on-exec (`O_CLOEXEC`, `SOCK_CLOEXEC`,
  `pipe2(O_CLOEXEC)`); a golden case lists the child's open descriptors (FR-013).
- **New system calls**: `execve` (x86-64 59, generic 221), `setpgid` (109, 154), `fchdir` (81, 50);
  `clone`, `wait4`, `pipe2`, `dup3`, `prctl`, `kill`, `getppid`, `signalfd4`, `rt_sigprocmask` exist.
- **Rationale**: the error pipe is the standard way to tell "could not start" from "started and exited
  127". A process group lets one `kill(-pgid)` stop everything the child started.
- **Alternatives**: `vfork`/`CLONE_VM` (faster with a large heap, but sharing the stack with freestanding
  C is fragile; marked `ponytail:` with SC-004's measurement as the trigger); `posix_spawn` (libc).

## R10. Waiting, limits and the timeout

- **Decision**: for the run, SIGCHLD and SIGPIPE are added to the blocked set and a `signalfd4` for
  SIGCHLD joins a `ppoll` over the child's stdout and stderr (read), stdin (non-blocking writes of the
  input text, closed when done or on `EPIPE`), bounded by the run's deadline. When SIGCHLD arrives,
  `wait4(pid, WNOHANG)`; once the child is reaped, its process group is killed (`kill(-pgid, SIGKILL)`)
  and the pipes are read until empty, without waiting for writers that escaped the group. Before the
  old mask is restored the signalfd is read empty, so a SIGPIPE from writing to a child that stopped
  reading is consumed, never delivered. Over 16 MiB on either stream: the group is killed and the call
  returns `err("output is larger than 16 MiB")`. At the deadline: the group is killed and reaped, then
  R0009 as for any timeout. The direct child dies with `vo` (PDEATHSIG).
- **Rationale**: serve already uses `signalfd4` with a blocked mask and it works on all three platforms
  under qemu; it wakes the loop exactly when the child exits, with no polling interval (a trivial
  program costs one fork and one exec, SC-004). Killing the group at exit is what makes grandchildren
  unable to hold the run open (edge case).
- **Known limit** (`ponytail:`): a grandchild that leaves the process group (`setsid`) survives the
  child, and if `vo` itself is killed with SIGKILL, grandchildren are not stopped; Linux has no portable
  "kill my descendants on death" short of a PID namespace, which needs privileges.
- **Alternatives**: polling `wait4` on a timer (latency or wasted wake-ups); a pidfd (spec 016 avoided
  it for qemu-user).

## R11. Exit codes and output text

- **Decision**: `code` is the exit status, or 128 plus the signal number when the child was killed by a
  signal (the shell convention, documented). `stdout` and `stderr` must be valid UTF-8; otherwise
  `err("output is not UTF-8")`.
- **Rationale**: one `int` covers both without a variant; agents know `128 + n`. A bytes type is on the
  roadmap and lifts the UTF-8 restriction.

## R12. Recording and replay

- **Decision**: new event kinds in schema 1, one per primitive operation: `info`, `list_dir`
  (`path`, `result`, `value`: the primitive's text), `append_file`, `make_dir`, `delete` (`path`,
  `result`, `value` on `err`), `rename` (`path`, `to`, `result`), `run` (`program`, `args`, `result`,
  `value`: the primitive's text with the code and both outputs, or the error). Replay checks every
  path and program against the grant first (lexically, as spec 013 does for files), takes the next
  event, matches the kind and `path`/`program` (and `args` for `run`; R0014 otherwise), performs
  nothing, and returns the recorded result; `denied` repeats R0010. The input text and environment of
  a run, and the text of an append, are not recorded (like `write_file`'s content).
- **Rationale**: adding event kinds within schema 1 follows specs 007 and 011 (`http`, `listen`,
  `request`, `response`); old recordings stay readable, which is what FR-018 protects. Matching `args`
  catches a replayed program that now runs a different command (the analog of spec 009's request line).
- **Alternatives**: a schema bump (no reader of schema 1 recordings would break otherwise).

## R13. Determinism

- **Decision**: listings are sorted by bytes and drop non-UTF-8 names; sizes, outputs and codes are
  inputs recorded by R12. Tests use fixture directories with fixed contents and helper scripts with
  fixed output, so golden outputs are identical on every platform.
- **Rationale**: Principle III: nondeterminism enters only through these named, effectful operations.

## R14. Test helpers

- **Decision**: small `#!/bin/sh` scripts in `tests/cases/process/bin/` (print arguments, copy stdin,
  exit with a code, kill itself with a signal, sleep, print 17 MiB, print invalid UTF-8, start a
  grandchild that sleeps, list its own descriptors from `/proc/self/fd`), granted by relative path; one
  case uses a bare name from `PATH` (`true`). File cases use fixture trees and a scratch directory
  created by the step runner. Link-swap cases (A4) extend spec 013's step actions.
- **Rationale**: `/bin/sh` exists on every supported host and on the CI runners; under qemu-user a host
  script runs natively, since `execve` goes to the kernel. Relative paths keep cases independent of
  `PATH`.

## R15. Security model

- **Decision**: security.md gains `file.delete` and `process` in the boundary table; S2 covers listing,
  information and every change (each through R5's walk, scope directories themselves protected); S3
  says processes are created only by tasks (spec 016) and by `process`, and that a started program runs
  with the user's rights outside the grant; the known limits add "a granted program can do anything its
  user can", A4 on the program file, and escaped grandchildren (R10). The stale spec 016 lines
  (one request at a time in S9 and the known limits) are corrected in the same change.
- **Rationale**: FR-012, FR-021; every spec that adds an effect is checked against security.md.

## R16. Reference cost

- **Decision**: the effects table gains two rows (`file.delete`; `process` with its scope forms), the
  modules table two rows (`std.fs`, `std.process`), the canonical order line two names, and one
  paragraph with one checked example: not confined, no shell, `PATH` for bare names, explicit
  environment, `128 + signal`, `--timeout`. Estimated 380 tokens (limit 500, budget 15,000; now about
  11,393).
