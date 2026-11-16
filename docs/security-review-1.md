# Security Review 1 (2026-11-16)

The first review against [security.md](security.md), with effects as a security boundary. Seven reviewers
covered: static effect checking (S1), run-time enforcement (S2, S3), the VM and memory safety (S4, S5, two
passes), networking (S4, S5, S9), replay and packages (S7, S8), and the commands that do not run programs
(S6). Every finding below was reproduced; every Critical, and the High findings H1, H7 and H10, were
re-run independently when the reports were merged. Version reviewed: `6958225`
(spec 012) plus the security documents.

**Result: the boundary does not hold yet.** Five Critical findings each break it on their own: a program
with no grant at all can read any file (C2), a program granted one directory can read and write any file
(C1, C3), the toolchain's memory can be corrupted from vo code (C4), and a hostile recording can make
`vo replay` read any file (C5).

## Root causes

Most findings share one of five causes; fixing the cause fixes the group.

| Cause | Findings | Fix direction |
|---|---|---|
| **R1. A function's run-time grant is its own declaration**, never limited by its caller's. Only the static check keeps callees inside `main`'s grant, so any static gap is a full escape. | C2, C3, H1 | Check every effect at run time against the root grant (`main`'s or the test's clause) as well as the running frame's |
| **R2. Paths are checked as text and opened separately**: the check resolves the counted string, the open uses it as a C string (NUL), relative to the real working directory, following links again. | C1, H2, H3, H4 | Refuse NUL; resolve and open through directory descriptors from one start-directory descriptor, with no symlink following after the check |
| **R3. Library parsers are not total on hostile bytes**: input strings may hold invalid UTF-8, `slice` then raises R0007, and std.http's parsers run outside the handler guard and with no deadline. | H7, H8, M2 | Parse requests and responses so that every input gives a value or `err`; run request parsing under the guard and deadline; one framing parser |
| **R4. The toolchain follows symbolic links everywhere** (no `O_NOFOLLOW`/`O_EXCL` in `src/`). | H5, M3, M4, (H4) | Refuse or contain links in imports, packages and temporary files |
| **R5. Recovering from a runtime error leaves process state behind** (collector off, error extras), which a long-running server then inherits. | H10, L11 | Restore all runtime state after a guarded failure; never raise inside the collector |

## Findings

### Critical

**C1. A NUL byte in a path escapes every file scope** (S2; A1, and A3 when paths come from input).
`rt_check_path` resolves the whole counted string (`src/rt_effects.c:142-226`), while the open uses
`path->data` as a C string (`src/rt_io.c:87,94`, `src/sys.c:186`), so `"/etc/passwd\0/../../proc/self/cwd/data/x"`
is checked as `data/x` and opened as `/etc/passwd`.
```vo
fn main() uses console, file.read("data/") {
    match read_file("/etc/passwd" + chr(0) + "/../../proc/self/cwd/data/x") {
        case ok(s) { print(slice(s, 0, 32)); }
        case err(e) { print(e); }
    }
}
```
Prints `root:x:0:0:root:/root:/bin/bash`. `write_file` the same way creates files anywhere.
Fix: refuse any path containing NUL before checking (R0010); see R2.

**C2. Function types nested in generic types lose their effects** (S1; A1). `ty_unify`'s function case
never compares effects (`src/types.c:295-307`), and after inference only top-level function arguments are
re-checked (`src/check_generic.c:132-136`; generic variant cases not at all, `:168`). A function value
inside `list[...]`, `map[...]`, `result[...]`, a generic record or a generic variant payload carries any
effect into a pure context.
```vo
fn each[T](x: T, fs: list[fn(T)]) {
    for f in fs { f(x); }
}
fn leak(name: str) uses console, file.read {
    match read_file(name) { case ok(s) { print("read: " + s); } case err(e) { print(e); } }
}
fn main() {
    each("/etc/hostname", [leak]);
}
```
Checks clean; `vo effects` says `main pure`; the run prints the file. Also through an already-bound
effect parameter, a generic variant (`Action.run(leak)`), `result`, `map`, a generic record, and inside a
contract (which E0603 then accepts as pure).
Fix: after binding, check every argument (and variant payload) against the instantiated parameter type;
compare effects in `ty_unify`.

**C3. An effect parameter bound to an anonymous function's inferred effects drops every scope** (S1/S2;
A1). The binding is a requirement set whose bits are "any scope" (`ES_ANY`); at run time it becomes the
generic function's grant (`src/vm.c:108`), and the scoped checks accept unscoped bits
(`src/rt_effects.c:129,137,145`). With the reference's own `each` pattern:
```vo
fn each[T, E: effects](xs: list[T], f: fn(T) uses E) uses E {
    for x in xs { f(x); }
}
fn main() uses console, file.read("data/") {
    each(["/etc/hostname"], fn(p: str) {
        match read_file(p) { case ok(s) { print("got: " + s); } case err(e) { print(e); } }
    });
}
```
Prints the file; a direct `read_file` in `main` is R0010. The same drops `file.write`, `env`, `net` and
`serve` scopes. Fix: R1 (root-grant check); never use a requirement set as a grant.

**C4. `sort_by` reads and writes outside a list's buffer when its key function shrinks the list** (S4;
A1). `hof_sort_by` reads `n = list->len` once (`src/rt_func.c:86-114`); a key callback that pops the list
and calls `sort` installs a smaller buffer; the gather loop then reads and the permute loop writes `n`
slots. The sanitizer build reports `use-after-poison` (read); the release build overwrites neighbouring
heap object headers and crashes (SIGSEGV), i.e. heap memory corruption from vo code.
Fix: sort a rooted snapshot; raise a runtime error if the list changed during `sort_by`.

**C5. A recording makes `vo replay` load any file as the program** (S7; A3). The recording's `program`
field is used as a path with no check (`src/replay.c:773`): absolute paths, `..`, devices and FIFOs.
`"program":"/etc/passwd"` prints `1 | root:x:0:0:...` in the parse error; `"../elsewhere/.env"` prints a
secret; `/dev/stdin` reads stdin; another `.vo` file is run instead of the recorded program (with only a
"program differs" note). Fix: take the program as an argument, or require a relative `.vo` path under the
recording's directory; open only regular files.

### High

**H1. A callee's narrower scope reaches outside the caller's grant through a symlink** (S1/S2; A1 given a
link under a granted directory). Static coverage compares scope text (`es_pair_covered`,
`src/effects.c:165`): `file.read("data/link/")` is inside `file.read("data/")`. At run time the callee's
grant is its own declaration, resolved through the link (`src/vm.c:99-105`, `src/rt_effects.c:150`), so
with `data/link -> /` a function declaring `file.read("data/link/")` reads `/etc/hostname`; the same read
in `main` is R0010. Fix: R1.

**H2. A failed `getcwd` makes the start directory `/`** (S2). `src/rt_effects.c:232-238`: `start_cwd`
becomes `""`, which every path is under, so `file.read("./")` reads the whole file system when the program
starts in a directory whose path is over 4096 bytes or was deleted. Fix: fail closed.

**H3. Moving the working directory redirects scoped access deterministically** (S2; A4). Relative paths
are checked against the cached start-directory text but opened relative to the real working directory
(`src/rt_effects.c:229-240`, `src/rt_io.c:87,94`): `mv proj proj.real`, a decoy `proj/data/x` and a link
`proj.real/data/x -> ../secret.txt` give the secret on every run. Fix: R2.

**H4. Check-then-open symlink race** (S2; A4). The known open question is confirmed: a helper swapping a
link between the check and the open won 34 of 100 runs. `write_file` truncates and writes through the
link. Fix: R2.

**H5. Package pins skip symlinked and unreadable subdirectories, but imports reach into them** (S8; A1).
`walk_dir` recurses only into `DT_DIR` entries and skips directories it cannot open (`src/main.c:115-123`);
imports load through them (`src/loader.c:46,103`). With `vendor/money/lib -> ../money_extra`, editing the
linked code changes what runs without E0701, and `vo package` reports `unchanged`. Fix: hash fails closed
on unopenable directories; load package files only from the hashed set (and its bytes, see H6).

**H6. Package files are read twice: once to hash, once to load** (S8; A4). `src/packages.c:185` then
`src/loader.c:103`; a replacement between the two runs unpinned code (shown with a delayed `openat`).
Fix: parse the bytes that were hashed.

**H7. One request stops any vo server** (S9; A2). `parse_request` runs outside the handler guard
(`std/http.vo:238`); a request with one byte `\x80` in a header, the query or the body makes a `slice`
raise R0007, which ends the program. Chunked bodies that `sv_framing` (C) accepts but `dechunk` (vo)
parses differently raise R0001/R0003. Fix: R3.

**H8. A 1.9 MB chunked request holds a server for 142 s** (S9/S5; A2). `dechunk` copies the rest of the
body for every chunk (`std/http.vo:156`) and request parsing has no deadline (`src/serve.c:71,326`).
Fix: index-based search; parse under the per-request deadline.

**H9. Chunk extensions and trailers are unbounded** (S5/S9; A2). Extension and trailer bytes count
against no limit (`src/serve.c:188,194-201`) and the 10 s read limit is checked only on `EAGAIN`
(`:224-231`): one connection grew a server to 241 MiB in 40 s, and to R0006 (exit) under
`--max-memory 64`. SIGTERM is not seen meanwhile. Fix: a total request size cap; check the deadline and
the stop signal on every read.

**H10. After a handler fails with R0006 (or R0009 raised inside a builtin or the collector), the server's
collector stays off** (S9/S5; A2). Reporting those errors sets `gc_on = 0` (`src/interp.c:49-71,264`),
assuming the process ends; `rt_serve_call` does not restore it (`src/serve.c:324-331`). One 16 MiB POST to
a `split`-based handler, then ordinary requests: RSS grows ~2 MB per request until `T0003` (exit 125); a
`--max-memory 32` server reaches ~500 MB. R0009 can also fire inside `sweep`, abandoning a collection
half-done. Fix: R5 (no polling inside the collector; restore `gc_on`, the OOM hook and error extras).

**H11. `==` on cyclic or very deep values crashes the toolchain** (S4; A1, and A2 for a handler comparing
nested request data). `values_equal` recurses with no stack check (`src/interp.c:305-357`): two cyclic
records give SIGSEGV; so do two 3,000,000-deep chains. Fix: `check_stack` (R0005), `a == b` shortcut.

### Medium

**M1. Replay accepts a forged `ok` for a file outside the grant** (S7; A3). The path is checked only when
the recorded result is `denied` (`src/replay.c:287-293,314-320`); `env`, `net` and `serve` are checked
always. Fix: check every file event against the grant, lexically (no file-system access in replay, see L2).

**M2. A hostile HTTP server crashes any vo client** (A2). The same non-total parsing as H7 in
`parse_response`/`dechunk` (`std/http.vo:118,120,139,156,171,180`) makes `get`/`post` raise R0007, R0001 or
R0003 instead of returning `err`. Fix: R3.

**M3. `fmt`, `edit`, `effects --update` and `package` write through a symlink at their temporary file**
(S6; A3, a hostile tree). `write_file_safely` (`src/main.c:533-543`) and `ed_write_tmp`
(`src/edit.c:553,618`) open a predictable `<path>.vo-fmt.tmp` with `O_CREAT|O_TRUNC`, following links:
formatting a hostile tree overwrote a file outside it. Fix: `O_EXCL|O_NOFOLLOW` and an unpredictable name.

**M4. An import resolving to a symlink makes `check`, `fmt`, `doc` and `query` print a file outside the
tree** (S6; A3). `repo/leak.vo -> ../secret.txt` and `import leak.thing;` print the secret's first line in
the parse error. Fix: refuse imports (and the missing-import scan) whose resolved file is outside the
program's tree, unless under a listed package.

**M5. Response header injection** (A2 via a server echoing request data). `response_text` writes header
names and values verbatim (`std/http.vo:313`) and `parse_request` keeps bare `\n` inside values: an echoed
header becomes a `Set-Cookie` header for the client. Fix: refuse CR, LF and NUL in header names and values.

### Low

| ID | Finding | Location | Fix |
|---|---|---|---|
| L1 | A literal path, host or variable outside the grant passes `vo check` (stopped at run time) | `src/check_effects.c:146,160` | Check literal targets statically (E0503) |
| L2 | Replaying a `denied` file event resolves links on the real file system (probing) | `src/replay.c:290,317` | Lexical check in replay |
| L3 | A NUL in a manifest DIRECTORY loops the hash walk to out-of-memory (T0003) | `src/packages.c:113` | Refuse control bytes in manifest tokens (E0703) |
| L4 | A FIFO named `*.vo` (in a package, or a recording's `program`) hangs `check`, `package`, `replay` | `src/sys.c:185` | `O_NONBLOCK` and regular files only |
| L5 | E0701's suggested `vo package` command can name a different directory | `src/packages.c:234-240` | Print the directory relative to the working directory |
| L6 | The recording reader accepts bad literals and overflowing integers, and is not bounded by `--max-memory` | `src/replay.c:553-623` | Exact literals, overflow checks, a size cap (T0008) |
| L7 | Diagnostics print raw terminal escape bytes from hostile source | `src/diag.c:330,356` | Escape control bytes in human output |
| L8 | DNS uses a fixed transaction ID and does not check the question | `src/net.c:152,179,187` | Random ID; match the question |
| L9 | The server's header-end search and framing rescan from the start on every read (quadratic) | `src/serve.c:238,253` | Resume where the last scan stopped |
| L10 | Rendering a 1,000,000-deep value in `expect_eq` stops `vo test` with T0003, not R0005 | `src/rt_render.c:35-41`, `src/gc.c:609` | Stack check in `render_value`; root-stack overflow as R0005 while running |
| L11 | A server's later JSON error reports carry stale `in_use`/`limit` fields | `src/interp.c:64` | Clear error extras after a guarded failure |

### Notes (design limits to decide on, not bugs)

- `net("host")` scopes have no port: `net("localhost")` reaches every local port.
- DNS answers are not checked: a granted domain can resolve to `127.0.0.1` or internal addresses (DNS
  rebinding against local services).
- Strings read from input may hold invalid UTF-8 (only source is validated); `std.text` and `std.json`
  then misbehave (`lines` raises R0007; `write_json` emits JSON that `parse_json` rejects). This is the
  general form of R3.
- Polymorphic recursion is quadratic and grows the permanent arena outside `--max-memory` (modest).

## Found sound

The reviewers verified, among others: effect-set operations and scope normalization; the requirement
fixpoint across files; higher-order builtins, calls through values and record fields, generic functions as
values; `__` builtins unreachable from user code; the builtin effect table complete; no flag, variable or
file widens a grant; `rt_resolve` on `.`, `..`, absolute paths, link loops, long paths; exact-match env,
net and serve checks; serve binding loopback by default; every effect builtin going through an `io_*`
wrapper with the same checks in record mode; the collector's size checks, value-stack bounds, overflow-
checked arithmetic, map probing and list builtins; number formatting and parsing edge cases; string
builtins on boundary code points; trace rendering of cyclic values; `--max-memory` holding for ordinary
allocation; handler failures by exception, stack exhaustion and timeout in bytecode; the recording and
manifest parsers under the sanitizer; `sha256` against an independent implementation; DNS answer parsing
bounds; server framing memory safety under 3,000 fuzzed requests; the client's response cap and deadlines;
front-end fuzzing (thousands of runs on the sanitizer build) with no crash or hang; nothing executing at
check time; deep nesting stopped at E0115.

## Next

The fixes are one spec (`013-security-fixes`): each finding a requirement, its reproduction the first
failing golden case. Suggested order: R1 and R2 (C1-C3, H1-H4) first, then C4, C5 and R3 (H7, H8, M2),
then the rest.
