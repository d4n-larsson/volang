# vo roadmap

vo's goal is to be the language AI coding agents do best in, and that humans can safely review. This roadmap is about what mainstream languages do poorly for agent-written code, and the specs that address it, in order. Each spec goes through the Spec Kit flow in `specs/NNN-*/` (spec, plan, tasks, implementation, `results.md`).

## Status

| Spec | Content | Status |
|---|---|---|
| [001](../specs/001-vo-lang-core/) | Core language and toolchain: check, run, test, fmt, explain, reference; JSON diagnostics with fixes; deterministic runs; multi-file programs | **Done.** All criteria met except "vo beats Python" on benchmark v1, which tied at the ceiling |
| [002](../specs/002-effects-functions-generics/) | Checked effects with scopes and a reviewable effects record; functions as values; user-defined generics; benchmark v2 | **Implemented, every gate passing.** The benchmark v2 run is not complete ([results](../specs/002-effects-functions-generics/results.md)) |
| [003](../specs/003-garbage-collector/) | Garbage collector and memory model; `--timeout` inside builtins; memoized generic resolution | **Done** ([results](../specs/003-garbage-collector/results.md)) |
| [004](../specs/004-bytecode-compiler/) | Bytecode compiler: programs run from compact, typed bytecode instead of walking the syntax tree | **Done** ([results](../specs/004-bytecode-compiler/results.md)) |
| [005](../specs/005-holes-props-contracts/) | Typed holes, property tests and contracts | **Done** ([results](../specs/005-holes-props-contracts/results.md)) |
| [006](../specs/006-query-edit-trace/) | Query API, structural editing, trace and replay | **Done** ([results](../specs/006-query-edit-trace/results.md)) |
| [007](../specs/007-stdlib-packages/) | Standard library and content-pinned packages | **Done** ([results](../specs/007-stdlib-packages/results.md)). Follow-ups: the agent run with and without `vo doc`, HTTPS, downloading packages |
| [008](../specs/008-c-rewrite/) | The toolchain in C: native builds on Linux x86-64, ARM64 and RISC-V 64, byte-identical behavior | **Done** ([results](../specs/008-c-rewrite/results.md)) |
| [009](../specs/009-small-fixes/) | Small fixes from writing the examples: `get_or`, replay matching the request line, R0010 naming the granting function, `case ok()`, `align_left`/`align_right` | **Done** ([results](../specs/009-small-fixes/results.md)) |
| [010](../specs/010-export-visibility/) | `export`: items are private to their file unless exported; the standard library exports only its interface | **Done** ([results](../specs/010-export-visibility/results.md)) |
| [011](../specs/011-http-server/) | `std.http.serve`: answering HTTP requests under a scoped `serve` effect, with record and replay | **Done** ([results](../specs/011-http-server/results.md)) |
| [012](../specs/012-spec-annotations/) | `@spec` annotations: tests name the requirements they prove; `vo query specs` | **Done** ([results](../specs/012-spec-annotations/results.md)) |
| Review | Security review against the threat model in [security.md](security.md) (effects as a security boundary) | **Review 1 done** ([findings](security-review-1.md): 5 Critical, 11 High, 5 Medium, 11 Low) |
| [013](../specs/013-security-fixes/) | The fixes from review 1: the root grant, opening files without following swapped links, one HTTP framing, safe links in imports and packages, replay and server hardening | **Done** ([results](../specs/013-security-fixes/results.md)): all 32 findings fixed, each with a case |
| [014](../specs/014-complex-data-structures/) | Built-in `ordered_map`, `ordered_set`, `priority_queue`, `deque` (created with `new`, like `map`) and `std.collections`; records of ordered fields are ordered; inferred type arguments for `new` and cases | **Done** ([results](../specs/014-complex-data-structures/results.md)) |
| [015](../specs/015-docs-site/) | Documentation site on GitHub Pages: guides with checked examples, generated reference, catalog and library pages, `llms.txt` | **Done** ([results](../specs/015-docs-site/results.md)) |
| [016](../specs/016-concurrency/) | `concurrent_map(xs, limit, f)`: tasks in input order, sharing nothing, failures and output as `map` gives them, record/replay; `serve` reads many connections at once | **Done** ([results](../specs/016-concurrency/results.md)) |
| [017](../specs/017-filesystem-processes/) | Directories and file changes (list, information, append, create directories; delete and rename under a new `file.delete` effect) and running other programs under a scoped `process` effect | **Done** ([results](../specs/017-filesystem-processes/results.md)) |
| [018](../specs/018-agent-testing/) | Agent-friendly testing: list and select tests by identifier, `@spec` ID or diff; explained failures (comparison operands, the path to the first difference, a cause per failure, captured output); coverage; warnings for tests that cannot fail; mutation testing | **Done** ([results](../specs/018-agent-testing/results.md)) |
| [019](../specs/019-https/) | HTTPS for `std.http.get`/`post`: TLS 1.3 and 1.2 from the vendored Mbed TLS 4.1 LTS, certificates always verified against the machine's trust store (plus `--ca-file`), nine fixed `err` messages, recordings with the scheme | **Done** ([results](../specs/019-https/results.md)) |
| [020](../specs/020-math/) | `std.math`: `sqrt`, `pow`, `exp`, `log`, trigonometry, `pi`, `floor`/`ceil`/`round` to `int`; correctly rounded from the vendored CORE-MATH, bit-identical on three platforms; out of domain is R0007 | **Done** ([results](../specs/020-math/results.md)) |
| [021](../specs/021-bytes/) | `bytes`: binary data in files, standard input and output, other programs' output and HTTP bodies (`std.bytes`, `read_bytes`, `run_bytes`, `get_bytes`, `serve_bytes`), hex and base64; every `str` is valid UTF-8, checked where text is read | **Done** ([results](../specs/021-bytes/results.md)) |
| [022](../specs/022-sleep/) | `std.time.sleep(ms)` under `clock`: real waits with the real clock, bounded by `--timeout`; with a fixed clock (`vo test`, `--clock-ms`) and in replay no wait, the clock moves on | **Done** ([results](../specs/022-sleep/results.md)) |
| [023](../specs/023-hover-docs/) | Documentation on hover: `vo query type-at` adds the declared signature and the doc comment of the item at a position (`doc` in JSON, as `vo doc --json` prints it), for functions, records, variants, fields, cases, library items and builtins; `vo doc ENTRY NAME` for the program's own items | **Done** ([results](../specs/023-hover-docs/results.md)) |
| [024](../specs/024-string-formatting/) | String formatting: the `format` builtin with checked `{}` placeholders (alignment, width, `+`, zero padding, thousands, fixed decimals, hex and binary), the same digits as Python; template mistakes and habits from C, Python and Go (`%d`, `{0}`, `{:.2}`, `f"..."`) are check errors with fixes | **Done** ([results](../specs/024-string-formatting/results.md)) |
| [025](../specs/025-regular-expressions/) | Regular expressions: `std.regex` with compiled patterns (`regex("...")` checked by `vo check`, `parse_regex` for data), test, first and all matches with groups, replace with `$1`, split; linear-time matching (a Pike VM), Python's results for the supported syntax; pattern mistakes and Python habits are check errors with fixes, and `"\d"` (E0103) gets a fix | **Done** ([results](../specs/025-regular-expressions/results.md)) |
| [026](../specs/026-sqlite/) | SQLite databases: `std.sqlite` with checked statements, `?` parameters, transactions, effect-typed connections | **Done** ([results](../specs/026-sqlite/results.md)) |
| [027](../specs/027-structure-standards/) | Structure standards: a thin shell of effects over a pure core, small files and functions, no copies, tests next to the code (W0701–W0706, `@allow`) | **Done** ([results](../specs/027-structure-standards/results.md)) |
| [028](../specs/028-concurrent-serve/) | Concurrent request handling: `std.http.serve_workers` with worker processes, a setup per worker, `--workers`, replay one request at a time | **Done** ([results](../specs/028-concurrent-serve/results.md)) |
| [029](../specs/029-web-package/) | `std.web` for APIs, after Spring: route tables, typed input from path, query, headers and JSON body checked by field annotations, answers and RFC 9457 problems, errors mapped in one place, steps, settings, health, tests without a network; derived JSON conversion (`to_json`, `from_json`) for every program | **Done** ([results](../specs/029-web-package/results.md)) |
| Ongoing | Benchmarks that can tell languages apart | Continuous |

## What mainstream languages get wrong for agent-written code

1. **Nobody can see what changed in what the code is allowed to do.** An agent fixes a bug and also adds a network call, a file delete or an environment read. In Python, TypeScript or Go that is invisible in the diff unless someone reads every line. **Addressed by 002:** effects in every function type, scopes, run-time enforcement, and `vo.effects`, so a reviewer approves "save_report gains file.write" instead of reading the diff.
2. **The agent's tests share the agent's misunderstanding.** Code that compiles and passes its own tests is weak evidence. **Addressed by 005:** property tests with inputs generated from types and shrunk counterexamples, and contracts checked on every call and return.
3. **Unfinished code does not compile at all.** Agents work step by step, but big languages reject the whole program until it is complete. **Addressed by 005:** typed holes.
4. **Agents invent APIs that do not exist.** The model's memory drifts from the installed versions. **Partly addressed by 001** (the reference is embedded in the binary). **Addressed by 007:** `vo doc` prints exact signatures for the standard library and packages, and packages are pinned by content hash.
5. **Agents cannot see the whole codebase.** They grep, guess, and break code they never saw. **Addressed by 006:** `vo query`, `vo impact` and checked `vo edit` refactorings.
6. **Debugging is mostly print statements.** **Addressed by 006:** call traces as JSON, local values in runtime errors, and replay of recorded runs.

## Next specs

### 003: garbage collector and memory model (done)

Memory used to be a bump arena that was never freed. Now:

- **The collector:** non-moving mark-sweep with conservative roots, over 64 KiB pages at fixed addresses. Collection points depend only on how much is allocated, so output, errors and memory reports are byte-identical from run to run. The language did not change.
- **Out of memory (R0006)** now means live data over a limit: 512 MiB by default, set with `--max-memory`. `--mem-stats` reports usage.
- **Also:** `--timeout` now stops long work inside builtins and the collector, and generic code no longer re-resolves its types on every call.
- **Not done:**
  - a moving or generational collector;
  - precise roots;
  - a remembered set for the permanent arena, which is still scanned at every collection.

### 004: bytecode compiler (done)

vo programs used to run by walking the checked syntax tree. Now:

- **Compiled once, after checking:** every function, lambda and test becomes typed bytecode, with 32-byte instructions over numbered frame slots, and a direct-threaded dispatch loop in assembly runs it. Calls stay inside the loop; frames live on the value stack.
- **Nothing visible changed:** output, exit codes, runtime errors and their spans, effect checks and `--timeout` are the same. The old interpreter is gone. Two intended differences:
  - memory figures moved;
  - recursion goes about 418,000 calls deep, up from between 100,000 and 200,000.
- **Speed:** 4.6× faster on average on six compute-heavy programs (2.3× to 8.6×). The 10-million-iteration loop takes 0.69 s, faster than Python's 0.99 s.
- **Not done:** native code or a JIT. Builtin-heavy programs gain least, since most builtins still go through the general builtin path.

### 005: typed holes, property tests and contracts (done)

- **Typed holes.** `todo("parse the header")` checks as whatever type its place expects. W0601 reports that type and the names in scope; a statement `todo(...);` ends its path; running into a hole is R0011. Unfinished programs check, run and test.
- **Property tests.** `prop "name" (xs: list[int]) { ... }` runs 100 generated inputs (every data type, records and recursive variants included), deterministic for a `--seed`, with `assume` and greedy type-directed shrinking. Failures report minimal counterexamples in text and JSON.
- **Contracts.** `requires` and `ensures` (with `ret`) on functions, pure, checked on every call and return in `run` and `test` (R0012, R0013), through function values and builtins too. Functions without contracts compile to the same bytecode as before.
- **Evaluation (stand-in):** on mutants of the ten hidden-property benchmark tasks, which all pass the visible cases, properties catch 61 of 62 (98%). The one miss is a correct but slow mutant.
- **Not done:** the benchmark run with agents writing properties themselves (see Benchmarks); a case-count option; strict-hole mode; `old()` in `ensures`.

### 006: query, structural editing, trace and replay (done)

- **Queries:** `vo query callers|refs|type-at ENTRY TARGET` answer from the checker's own resolution; `vo impact ENTRY --diff FILE` lists the functions, tests and properties a unified diff reaches through calls.
- **Structural edits:** `vo edit rename` and `vo edit add-field` change every reference (and `vo.effects`), check the edited program again in the same process, and write all files or none; a clash or a changed meaning is refused (T0006).
- **Traces and replay:** `vo run --trace` writes calls and returns as JSON lines with no cost when off; runtime errors show each frame's parameters and locals (not for timeouts); `vo run --record` / `vo replay` capture and feed back every input.
- **Evaluation (stand-in):** on the refactor tasks' starters, 53 of 53 scripted `vo edit`s keep every visible and hidden output, while the same renames by text replacement break 12 of 41; 95 of 95 hand-written reference lists match `vo query` exactly.
- **Not done:** the agent benchmark run on the refactor category (see Benchmarks); other edits (extract function, move an item); impact through types; a per-function trace filter.

### 007: standard library and packages (done)

- `std.json`, `std.time`, `std.text`, `std.random` and plain-HTTP `std.http` behind the `net` effect, written in vo and built into `vo`; HTTP exchanges are recorded and replayed.
- `vo doc [--json]`: exact signatures, effects and examples for the installed version, from the checker's own declarations; package modules too.
- Packages pinned by content hash in `vo.packages` and resolved at check time, never at run time; a package's effects appear in `vo.effects`, so a new dependency that wants the network is visible in review.
- Follow-ups: the agent run with and without `vo doc`, HTTPS, downloading packages.

### 014: complex data structures (done)

- **Built-in collections,** written and created like `list` and `map`: `new ordered_map[str, int]`,
  `ordered_set[T]`, `priority_queue[T]` (equal items leave in push order), `deque[T]`. AVL trees, a binary
  heap and a ring buffer in the runtime; mutable; equality, printing and generated test inputs by
  contents. The existing builtins work on them; `std.collections` adds neighbors (`key_at_or_above`,
  `after`, ...), ranges, set algebra, deque fronts and `_by(cmp)` orders.
- **Language:** a record whose fields are all ordered is ordered, field by field; type arguments of
  `new` on generic records and of cases like `Tree.leaf` are inferred from the fields and the context.
- **Evaluation:** eight benchmark tasks with a solution using the collections, a hand-written efficient
  one and a naive one; the agent run with and without the module is still to do (see Benchmarks).

### 016: concurrency (done)

A vo program used to do one thing at a time: a server answered one request at a time (a slow client held
it for up to 10 s), and fetching ten URLs waited for each in turn. Now:

- **`concurrent_map(xs, limit, f)`**, shaped like `map`: each element is a task, at most `limit` (1 to 64)
  at a time, and the results come back in the order of `xs`. Ten 1-second requests take about 1 s.
- **Tasks share nothing.** Each task is a child process (a fork), so it works on copies of everything it
  gets or captures and only its result comes back; the VM and the collector stay single-threaded and
  unchanged. Results that cannot come back are refused: functions (E0314), a collection with its own
  order (R0015); stdin cannot be read in a task (E0511).
- **Deterministic by ordering.** Output is task 0's, then task 1's, whatever order they finish in; the
  failure reported is the first in input order, with the report `map` would give; per-task random
  sequences come from `--seed`; traces get a `task` record per task; recordings keep their format and
  replay exactly, on every platform.
- **Effects unchanged.** Starting tasks is not an effect; each task has its caller's grant, and
  `vo.effects` is the same for a loop and its concurrent version.
- **`serve`** reads up to 64 connections at once (each with its 10 s limit) and calls the handler for one
  request at a time, in the order requests arrive complete, so state kept between requests stays safe.
- **Not done (on purpose):** parallel computation as a goal; concurrency inside tasks (nested calls run
  one task at a time); handlers that overlap; closures in results.

### 017: filesystem and processes (done)

A program could only read and replace whole files. Spec 017 adds directory listings and path
information under `file.read`; appending and creating directories under `file.write`; deleting files and
empty directories, and renaming, under a new `file.delete` effect, so destructive changes stand out in
review; and running a named program (no shell) to completion under a new `process` effect scoped to the
programs it may run, with captured output, `--timeout`, record and replay. A started program runs with the
user's full rights, and security.md says so.

- **Not done (on purpose):** confining started programs to the grant (planned below), streaming or
  interactive children, a shell, modification times, permissions, links, copying, recursive delete;
  names and output that are not UTF-8 (they wait for a bytes type).

### 018: agent-friendly testing (done)

From the proposal in [agent_friendly_test_framework.md](agent_friendly_test_framework.md), fitted to vo:
one language, so the CLI with `--json` is the agent interface. `vo test` now lists tests without running
them (`--list`, identifiers `FILE::NAME`) and selects exactly (`--test`, `--spec`, `--diff`); a failing
`expect` shows its operands, `expect_eq` the first difference, every failure carries a `cause`, and a
test's output stays in its record; `--coverage` lists what no test ran; `--mutate` changes the code one
step at a time and lists the changes no test catches; W0602 and W0603 flag tests that cannot fail.

- **Not done (on purpose):** recorded tests (planned below), isolating tests that write files, retries
  and flaky-test detection (runs are deterministic), skips and expected failures, mock objects, parallel
  mutant runs.

### 019: HTTPS (done)

`std.http.get` and `post` accept `https://` URLs. TLS comes from Mbed TLS 4.1.2 (the LTS branch,
supported until March 2029), vendored unmodified under `third_party/` and built into the one static
binary under constitution 1.4.0, which added the rules for vendored code. A request is sent only after
the server's certificate is verified against the machine's trust store and any `--ca-file` the runner
gives; every failure is an `err` with a fixed message, and nothing turns verification off.

- **Not done (on purpose):** serving over HTTPS (planned below), session resumption and connection
  reuse, revocation checks (OCSP, CRL), client certificates, HTTP/2.

### 020: math functions (done)

`std.math` has `sqrt`, `pow`, `exp`, `log`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `pi`,
and `floor`, `ceil` and `round` to `int`. The transcendental functions are CORE-MATH's correctly rounded
ones, vendored at a commit under constitution 1.4.1 (CORE-MATH publishes no releases), so every result is
the nearest double and the same on all three platforms; 109,517 reference values, the library's
published hardest cases among them, match bit for bit on each. Out-of-domain arguments are R0007 at the
call; NaN passes through; overflow is `inf`.

- **Not done (on purpose):** `log2`, `log10`, `hypot`, `cbrt`, hyperbolic functions (each comes from the
  same library when a program needs it); hardware FMA on x86-64 (a software `fma` keeps one binary
  for every x86-64 processor; one million calls take about 0.4 s).

### 021: binary data (done)

`bytes` is a built-in type: an immutable sequence of integers 0 to 255, with no literal. `std.bytes`
converts it to and from text, integers, hex and base64, and reads and writes standard input and output;
`std.fs`, `std.process` and `std.http` gained bytes twins of their text functions. Every `str` is now valid
UTF-8: text read from files, input, arguments, the environment, programs and HTTP is checked when it is
read, and the UTF-8 check follows the standard exactly (overlong forms and surrogates are refused, in
source files too).

- **Not done (on purpose):** a bytes literal, `for` over bytes, bytes as map keys, base64url, streaming
  reads, directory names that are not UTF-8.

### 022: sleep (done)

`std.time.sleep(ms)` waits, under `clock`. With the real clock it waits without using the processor and
never past `--timeout`; with a fixed clock (every `vo test`, `--clock-ms`) it returns at once and moves
the clock on, so code that sleeps is tested instantly and exactly; replay does not wait. Doc examples now
run with a fixed clock too.

- **Not done (on purpose):** timers, async waits, sub-millisecond durations, ending a sleeping `serve`
  handler at once on Ctrl-C (it ends when the handler returns, within its time limit).

### 023: documentation on hover (done)

`vo query type-at` answers with the declared signature and the documentation of the item at a position:
the comment lines directly above it (the convention the standard library already used, now stated in the
reference) and its example. JSON gains a `doc` field holding the record `vo doc --json` prints, so an
editor plugin shows it on hover; `vo doc ENTRY NAME` prints it for the program's own items. The doc syntax
did not change.

- **Not done (on purpose):** markup or tags in doc comments, completion, a standing language server
  (the "Language server" row below).

### 024: string formatting (done)

`format("{:<12}|{:>8.2f}", name, price)`: a builtin whose template is a string literal, checked against
its arguments when the program is checked, so a call that checks cannot fail at run time. Placeholders
are positional `{}` with the Python and Rust option syntax (a subset); ints and floats with a precision
print exactly what Python's `format()` prints (100,000 random values, all three platforms). Wrong
counts, unsupported options and options on the wrong type are E0315-E0318; `%d` templates, `{0}`,
`{name}`, `{:.2}`, `{:d}` and `f"..."` (E0120) get fixes to the vo form.

- **Not done (on purpose):** interpolation, scientific notation (`e`), percentages, fill characters, `#`
  prefixes, debug printing of lists and records, locale-aware output, a `printf` that writes directly.

### 025: regular expressions (done)

`std.regex`: `regex("...")` compiles a literal pattern, checked by `vo check` (E0319-E0321, with spans
inside the pattern), and `parse_regex(s)` one from data; `is_match`, `first_match` (groups, named
groups, byte offsets), `all_matches`, `replace_all` (`$1`, `${name}`; E0322) and `split_on`. The engine
is vo's own Pike VM: time linear in the text for every pattern, Python's leftmost-first results (100,000
random patterns and texts, all three platforms), 10 MB of logs searched in half a second. Python's
`(?P<name>`, `\1` in replacements and `"\d"` in any string (E0103) get fixes.

- **Not done (on purpose):** lookaround and backreferences (they need backtracking), Unicode classes
  and case folding (vo's text functions are ASCII), replacement by a function, matching over streams, a
  regex literal syntax.

### 026: SQLite databases (done)

`std.sqlite`: SQLite 3.54.1 (public domain, allowed by constitution 1.5.0) vendored unmodified and
built into the static binary with vo's own OS layer, so every file it opens goes through the program's
grant. `open` (file.read, file.write), `open_readonly` (file.read) and `open_memory` (no effect: real
SQL in `vo test`, closed when each test ends). Statements are literals, `sql("...")`, parsed by
`vo check` (E0323-E0326, with spans inside the SQL); values are only `?` parameters, so SQL injection
cannot be written by accident; `parse_sql(s)` for SQL built at run time. A database's type names its
effects, `Database[uses file.read, file.write]`, the first effect arguments in types (E0327), so a
function that queries one needs them wherever the handle came from. Results are the same on all three
platforms (1,000+ statements, floats included); clock and randomness in SQL are refused; record and
replay cover every call.

- **Not done (on purpose):** PostgreSQL and other servers (a later spec, over TCP), streaming rows (a
  query returns every row), named parameters, scripts of several statements, WAL mode, an ORM or query
  builder, user-defined SQL functions.
- **Standing obligation:** as for Mbed TLS, SQLite security releases
  ([sqlite.org/cves.html](https://www.sqlite.org/cves.html)) are applied in their spec cycle with
  `tools/vendor_sqlite.sh`.

### 027: structure standards (done)

How a vo program is structured, written down and checked: a thin shell of effects over a pure core,
one concept per file, small functions, one function instead of copies, tests next to the code. `vo
check` warns (W0701–W0705) about a function with effects holding 15 or more statements that use no
effect, a file over 400 code lines, a function over 50 statements, 6 or more statements repeating
earlier ones (with the effect-parameter advice when copies differ only in effects), and a test that
calls only another file's functions; `@allow("W07NN", "reason")` accepts one warning where the
structure is deliberate (W0706 when it silences nothing). `vo test main.vo` now runs the tests of every
file the program loads. The limits come from measuring the examples, std, the golden cases and two
programs agents wrote in one file (739 and 1,350 lines).

- **Not done (on purpose):** an extract-function edit that would make the W0701/W0704 advice a fix,
  per-project limits, rules about naming or comments (`vo fmt` covers layout).

### 028: concurrent request handling (done)

`std.http.serve_workers(address, workers, setup, handle)` and `serve_bytes_workers` answer up to
`workers` requests at once, so a slow request no longer delays the others. The server process keeps the
connections and their limits; worker processes, forked at start, run `setup` once (a database
connection per worker) and then one request at a time, with nothing shared between requests. A failing
handler answers 500, a dead worker is replaced, SIGTERM answers the requests already read; `vo run
--workers N` overrides the count. Recordings keep each request's inputs as one block in arrival order,
so replay runs the requests one at a time with the same responses. `std.sqlite`'s `transaction` now
takes the write lock at once on a writable database, so read-then-write transactions in several
processes wait for each other instead of failing with `busy`. The notes-api example keeps its notes in
SQLite.

- **Not done (on purpose):** automatic scaling of the worker count, a cache shared between workers,
  serving over HTTPS (run a proxy in front).

### 029: a web package for APIs (done)

`std.web` gives APIs what Spring gives Java ones, as values instead of annotations and reflection: a
route table (`on_get("/notes/{id}", show)`) checked by `vo check` (E0330, E0331), operations
`fn(ctx, input) -> result[Response]` whose input record is bound from the path, query, headers and JSON
body and checked by field annotations (`@size`, `@range`, `@pattern`, `@default`, `@header`; E0329)
before they run, answers and RFC 9457 problems, errors `err("not_found: ...")` mapped in one place,
steps around requests (request ids, logging, CORS, bearer tokens, API keys), typed settings, health and
info routes, `serve_app` over `serve_workers`, and `handle_request` for tests. Underneath, every program
gains derived JSON conversion (`to_json`, `from_json` for any `T: json`, E0328) and better inference
(generic functions as values, a call's type from the other side of `==`, `try` and the expected type).
The notes-api example is rewritten with it.

- **Not done (on purpose):** sessions and cookies, HTML views, OpenAPI generation, rate limiting, HTTPS
  serving, form and multipart bodies.

## Planned after 017

The gaps found after spec 016, roughly in order of how many programs they block. Each becomes a spec when
it is next; anything that reaches the outside world gets an effect, scopes, and record and replay.

| Gap | What is missing | Notes |
|---|---|---|
| Benchmark v2 run | whether vo helps agents is still unmeasured | see Benchmarks; decides what matters next |
| TCP and UDP | only HTTP reaches the network | scoped like `net` and `serve` |
| macOS | Linux only (x86-64, ARM64, RISC-V) | the raw-syscall, no-libc design makes this a real port |
| Downloading packages | packages are local and pinned by hash, never fetched | spec 007 follow-up; HTTPS exists since spec 019 |
| Language server | no completion, hover or diagnostics in editors | `vo query` and `vo check --json` already have the data |
| Debugger | traces, locals in errors and replay, but no stepping | replay makes a deterministic stepper possible |
| Tuples, more map keys | no tuple type; map keys are `int` or `str` only | records and ordered maps cover some cases |
| `old()` in `ensures` | contracts cannot compare with values from before the call | spec 005 follow-up |
| Effects as an annotation | the `uses ...` clause sits between the signature and the body (`fn f(x: int) -> int uses file.read("data/") {`), which makes long signatures hard to read | move it to an annotation on the line above, as `@spec`/`@allow` are (`@uses(file.read("data/"))`); one form only, so `vo fmt` and a `vo edit` migration rewrite every program; function types (`fn(int) -> int uses E`) and effect arguments in types need their own spelling |
| Confined child programs | a program started under `process` runs with the user's full rights | opt-in confinement to the grant (Landlock); spec 017 follow-up |
| Serving over HTTPS | `std.http.serve` answers plain HTTP only; a public server needs a proxy in front | spec 019 follow-up: certificate and key under the program's grants |
| Recorded tests | a test that uses HTTP, files, programs or the environment depends on the machine and the network | spec 018 follow-up, decided: `vo test --record DIR` saves one recording per test, `vo test --replay DIR` runs offline on them; replaces mock objects |
| Events | investigate if we should support events | |

User-defined interfaces and compiling to machine code stay out of scope (below).

## Benchmarks (ongoing)

On the v1 benchmark both languages were at the ceiling, so it could not show whether vo helps agents. Benchmark v2 (40 tasks in multi-file, refactor, hidden-property and capability categories, plus a 24-task feature set; f21–f24 are formatting tasks from spec 024) is built and validated. Its run is the next step:

1. **Run v2 to completion** with the corrected harness: no tools, no user settings, an empty working directory. The first run was invalid because the agent could read the answers.
2. **Re-run v1** the same way: its results were collected with the flawed harness.
3. **Agents using properties (spec 005 follow-up):** run the hidden-property tasks with agents told about `prop`, and compare the hidden-check pass rate with and without it. Spec 005 measured only the stand-in (mutants of the reference solutions).
4. **Agents using queries and edits (spec 006 follow-up):** run the refactor tasks with agents told about `vo query`/`vo edit`, and compare the pass rate with and without them. Spec 006 measured only the stand-in (scripted edits on the starters).
5. **Decide what comes next from the results.** If both languages are still at the ceiling, the next benchmark needs longer, unsupervised, multi-step work (where effects, review and refactoring matter), not just harder single tasks.

Known gaps in v2 to fix along the way:
- Per-case environments: tasks c03 and c08 never test their environment variable being set.
- Task c05 depends on its cases running in order.
- The Python sandbox cannot see effects done through C extensions; seccomp or strace would close that.

## Running natively (spec 008, done)

The toolchain was RISC-V assembly, so on ordinary machines it ran through `qemu-riscv64`. Spec 008 ported it
file by file to freestanding C, every step gated against the last assembly release
([results](../specs/008-c-rewrite/results.md)):

- `make` builds a static native binary on Linux x86-64, ARM64 and RISC-V 64; the only assembly left is a
  platform layer of under 200 lines per architecture (entry, system calls, `setjmp`/`longjmp`).
- Behavior is byte-identical on all three and to the assembly release: the suite, 90,000 generated programs,
  the 703 benchmark reference runs and replays recorded on one platform and replayed on another.
- Natively on x86-64 the speed benchmark runs about 7 times faster than the assembly toolchain under qemu.
- The collector's roots are precise (no native stack scanning), and the suite also runs under ASan and UBSan.

The next specs start from C. The qemu-only effects (the pinned page offsets) are gone with the assembly.

## Out of scope for now

- Compiling to machine code: vo runs bytecode (004), and native code waits until that becomes the bottleneck for agent-sized programs.
- User-defined interfaces or traits: generics with `ordered` and effect parameters cover the cases seen so far.
