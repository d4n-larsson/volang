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
| [030](../specs/030-readable-signatures/) | readable signatures: `vo fmt` lays out headers over 100 characters (the effect clause on its own line, then parameters one per line); named effect sets (`effects Storage = ...;`, `uses console, Storage`) with one form for effects everywhere | **Done** ([results](../specs/030-readable-signatures/results.md)) |
| [031](../specs/031-constants/) | Constants: `const name = value;` items of type `int`, `float`, `bool` or `str`, worked out by the checker from literals, constants and operators; usable as effect scopes, annotation limits and checked templates; reviews show values | **Implemented** ([results](../specs/031-constants/results.md)) |
| [032](../specs/032-typed-errors/) | Typed errors: `result[T, E]` (`result[T]` is `result[T, str]`), `try` only between the same error types (E0412), `map_err` the one conversion; `std.web`'s `typed(op, answer)` maps a variant to statuses in one checked function; `std.option` | **Implemented** ([results](../specs/032-typed-errors/results.md)) |
| [033](../specs/033-map-keys/) | Structural map keys: `map[K, V]` keyed by `int`, `bool`, `str`, `bytes`, records and variants of those, compared like `==`; `K: key` for generic code; E0309 names the part that is not a key | **Implemented** ([results](../specs/033-map-keys/results.md)) |
| [034](../specs/034-vo-build/) | `vo build`: a program as one static executable (the toolchain with the program's files attached and hashed), run without sources; `--vo-effects` prints its grant; `vo run`'s options as `--vo-NAME`; `--base` for other architectures | **Implemented** ([results](../specs/034-vo-build/results.md)) |
| [035](../specs/035-std-crypto/) | `std.crypto`: `sha256`, `hmac_sha256`, constant-time `same_bytes`, `random_bytes` from the system under a new effect `entropy`, `hash_password` / `verify_password` (PBKDF2-HMAC-SHA256) | **Implemented** ([results](../specs/035-std-crypto/results.md)) |
| [036](../specs/036-vo-mcp/) | `vo mcp`: the toolchain as a Model Context Protocol server on stdin/stdout; ten commands as tools with the commands' own JSON output, the reference as a resource; every call in its own process | **Implemented** ([results](../specs/036-vo-mcp/results.md)) |
| [037](../specs/037-editor-grammars/) | Editor grammars in `editors/`: TextMate (VS Code, and the format GitHub reads), tree-sitter (Neovim, Helix, Zed), Vim; held to the language by a check over every `.vo` file in the repository | **Implemented** ([results](../specs/037-editor-grammars/results.md)) |
| [038](../specs/038-vo-lsp/) | `vo lsp`: the toolchain as a language server; diagnostics on unsaved text, quick fixes, hover, definition, references, rename, format, outline, each answer computed by the command it stands for in its own process | **Implemented** ([results](../specs/038-vo-lsp/results.md)) |
| [039](../specs/039-std-tcp/) | `std.tcp`: TCP connections under the existing `net` and `serve` effects, with ports in `net` scopes (`net("db:5432")`), three reads with time limits, TLS from the first byte or as an upgrade, listen and accept, typed errors, record and replay | **Implemented** ([results](../specs/039-std-tcp/results.md)) |
| [040](../specs/040-streaming/) | Streaming: `std.fs.each_line` and `each_chunk`, `std.http.send_stream`, `std.process.run_lines` hand a program one piece at a time through a function it passes; memory does not grow with the source; no new type | **Implemented** ([results](../specs/040-streaming/results.md)) |
| [041](../specs/041-package-download/) | Downloading packages: `vo package ENTRY NAME URL` fetches a tar or tar.gz archive over HTTPS, unpacks its `.vo` files, pins them by content and prints the effects the package declares; `--restore` on a fresh clone; hostile archives refused whole | **Implemented** ([results](../specs/041-package-download/results.md)) |
| [042](../specs/042-strings/) | Strings: raw strings between backticks, the escapes `\\r` and `\\u{HEX}`, `for c in s`, `upper`/`lower` for Latin, Greek and Cyrillic, exponent float literals, `std.text.char_count` and `equal_fold` | **Implemented** ([results](../specs/042-strings/results.md)) |
| [043](../specs/043-bits/) | Bits: `std.bits` (and, or, xor, not, shifts, rotations, wrapping arithmetic, `parse_hex`, `parse_binary`) and hex and binary integer literals | **Implemented** ([results](../specs/043-bits/results.md)) |
| [044](../specs/044-decimal/) | Decimal: `std.decimal`, exact decimal numbers (units and places) with stated rounding, text, comparison and allocation, written in vo | **Implemented** ([results](../specs/044-decimal/results.md)) |
| [045](../specs/045-security/) | Security: `std.security`, access control for `std.web` applications in the manner of Spring Security: one table of rules, tokens, Basic and keys, the user in operations, and a review of the table; written in vo | **Implemented** ([results](../specs/045-security/results.md)) |
| [046](../specs/046-iteration/) | Iteration: `for k, v in m` and `for i, x in xs`, W0403 with a fix for the loops they replace, `std.lists` | **Implemented** ([results](../specs/046-iteration/results.md)) |
| [047](../specs/047-workers/) | Workers: `std.workers`, processes that run beside the program and exchange typed messages; a time limit for one call; failures as values; deterministic output; replay without workers | **Implemented** ([results](../specs/047-workers/results.md)) |
| [048](../specs/048-timezones/) | Time zones: the tz database built into `vo`, zones in `std.time` (`load_zone`, `in_zone`, `from_local`, `format_in`), the machine's zone under `clock` | **Implemented** ([results](../specs/048-timezones/results.md)) |
| [049](../specs/049-postgres-package/) | The `postgres` package: a PostgreSQL client in vo on `std.tcp` and `std.crypto`, with `std.sqlite`'s names, results and errors; TLS, SCRAM-SHA-256, time limits | **Implemented** ([results](../specs/049-postgres-package/results.md)) |
| [050](../specs/050-scoped-package-effects/) | Scoped effects through functions given their target (helpers, package `connect`): callable under scopes, every target checked against each scoped declaration on the call path | **Implemented** ([results](../specs/050-scoped-package-effects/results.md)) |
| [051](../specs/051-html-templates/) | Web pages: `std.html` templates checked by `vo check`; `html_reply`; HTML forms bound like query parameters | **Removed** by [053](../specs/053-remove-html/) except form binding: vo renders no HTML |
| [052](../specs/052-sessions-sign-in/) | Sessions and sign-in forms: cookies in `std.web`; `std.security`'s `browser` way and routes, sessions kept by the application, cross-site posts refused, pages sent to sign-in | **Implemented**, page parts removed by 053 ([results](../specs/052-sessions-sign-in/results.md)) |
| [053](../specs/053-remove-html/) | Remove HTML rendering from the core: vo targets backend network services; `std.html`, its checker and `html_reply` removed, E0335–E0340 retired; browser sign-in without a page | **Implemented** ([results](../specs/053-remove-html/results.md)) |
| [054](../specs/054-recorded-tests/) | Recorded tests: `vo test --record DIR` saves what each test receives from outside (HTTP with its sent body, TCP both ways, files, variables, programs), `--replay DIR` runs each test on its file offline; mismatches fail that test (R0014); credentials redacted | **Implemented** ([results](../specs/054-recorded-tests/results.md)) |
| [055](../specs/055-service-operations/) | Operating a service: a `log` effect; `std.log` (JSON records with the request's id, secrets redacted by name, W0506), `std.metrics` (counters, gauges, histograms shared by worker processes, `/metrics` in the Prometheus text format), request steps, `/health/live` and `/health/ready`, `--drain` | **Implemented** ([results](../specs/055-service-operations/results.md)) |
| [056](../specs/056-consistency-pass/) | Consistency pass (vo 0.2.0): std names that clashed renamed, lookups give an `Option`, regex takes the text first, durations in ms, one time-limit rule, one parse error format; no local shadows a builtin; `bool` and variants ordered; `[]` from context; one spelling per literal; one message style. E0214 names each library change with its fix | **Implemented** ([results](../specs/056-consistency-pass/results.md)) |
| [058](../specs/058-module-imports/) | Module imports (vo 0.4.0): `import std.json;`, then `json.parse_json`; one import per module, no local named like a module (E0203); the 0.3 form is E0215, whose one-edit fix rewrites the imports and every use; W0203 for an unused import; types in messages and hover as the file writes them; `vo query`/`vo edit` take `module.name`; hover on a module | **Implemented** ([results](../specs/058-module-imports/results.md)) |
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
  (spec 038 added the server; completion is still open).

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

### 030: readable signatures (done)

`vo fmt` lays out a `fn`, `test` or `prop` header over 100 characters: the `uses` clause on its own line,
then, if still too long, the parameters one per line; anonymous functions' parameters break like a call's
arguments, and function types never break. Effect sets name scoped effects a program repeats
(`effects Storage = file.read("data/"), file.write("data/");`), usable wherever effects are written,
exported and imported like items; records, messages and `vo doc` show what they stand for, so reviews
stay exact. Moving `uses` into an annotation was rejected: types keep `uses`, so the concept would get
two spellings.

- **Not done (on purpose):** parameters on sets, set operations other than union, comments between
  parameters (a syntax error, as before).

### 031: constants (implemented)

A value with one meaning used in several places (a limit, a directory, a message) was a repeated literal
or a function with no parameters, and the two string literals of a scope and the path under it had to
agree by hand. `const name = value;` names it once. The checker works out the value from literals, other
constants and operators with the run-time rules, folds every use into that literal, and accepts a
constant wherever a literal is read: effect scopes, field annotations, and the templates of `format`,
`regex`, `sql` and routes. `vo.effects` and every report show values, so replacing a literal with a
constant changes nothing in review, and changing the constant is reported for each function it reaches.

- **Not done (on purpose):** calls in values (even `len` or `str`), lists, maps and records as
  constants (they can be changed in place), constants inside functions (`let` names a value there),
  generic constants.

### 032: typed errors (implemented)

Every `result` used to fail with a `str`, so callers could only compare text, and `std.web` read a status
out of a message prefix. `result[T, E]` fails with a value of `E`, usually a variant of the ways to fail:
callers `match` it and the checker sees every case. `try` passes an error up only between the same error
types (E0412); `map_err(r, f)` is the one conversion; `main` and tests keep `str` errors. `std.web` gains
`typed(op, answer)`, `std.option` gives `Option[T]` for absence.

- **Not done (on purpose):** builtins returning `Option` (`pop`, `env`, `get_or` keep their results),
  any automatic message from an error value, one error type per `std.web` app.
- **Found on the way:** a generic function value two generic calls deep does not get its effect
  arguments inferred (`on_get(p, typed(op, f))` with `op` generic in its effects): the notes API writes
  the operations' types out. Worth a small spec of its own.

### 033: structural map keys (implemented)

Map keys were `int` or `str`, so programs glued compound keys together from text
(`str(x) + "," + str(y)`): slower, untyped, and wrong when a field holds the separator. A key is now any
type made of `int`, `bool`, `str`, `bytes`, records and variants, hashed and compared structurally, so
two keys are one entry exactly when `==` says so. Generic code declares `K: key`; E0309 names the part
that cannot be a key (`value` is `float`) and adds `: key` to a bare parameter. `int` and `str` keys run
the same code as before.

- **Not done (on purpose):** `float` and `result` keys, tuples (a record is the one way to group values),
  detecting a record changed while it is a key (documented, as for `ordered_map`), combining two
  constraints on one parameter.

### 034: one executable per program (implemented)

A vo program used to travel as its source tree plus the toolchain. `vo build main.vo -o app` checks it as
`vo check` does (an effects record that disagrees refuses the build) and writes the toolchain binary with
the program's files, a hash and a 64-byte trailer appended; `./app ARGS` then runs as `vo run main.vo --
ARGS`, from any directory, on any Linux machine of that architecture. `./app --vo-effects` prints what
`main` declares, enforced as always. `vo run`'s options are `--vo-NAME`; recordings replay with
`--vo-replay`. Other architectures: `--base` with that architecture's `vo`.

- **Not done (on purpose):** a compiled form (the program is checked at each start, in milliseconds), a
  smaller runtime (an executable is about 10 MB), signing, `check` and `test` inside an executable.
- **Known limit:** a file cut off at its end has lost its program part and starts as the plain toolchain
  (a usage error); bytes changed in the part are T0014.

### 035: hashing and secrets (implemented)

Nothing in vo could make a value nobody can guess (`random_int` is seeded), store a password or check a
signed webhook. `std.crypto` adds `random_bytes(n)` from the operating system under its own effect,
`entropy`, so `vo.effects` shows who draws secrets; `hash_password` and `verify_password`
(PBKDF2-HMAC-SHA256, 600,000 iterations, a self-describing stored text); `sha256`, `hmac_sha256` and a
constant-time `same_bytes`, all on `bytes`. Secret bytes ignore `--seed`, are reproducible in `vo test`,
and are recorded and replayed like every other input.

- **Not done (on purpose):** Argon2 and scrypt (the stored text names its method, so they can be added),
  SHA-1, MD5, SHA-512, encryption, public-key signatures; `std.web` request IDs stay on `random`.

### 036: the toolchain as an agent server (implemented)

An agent had to be told that vo's commands exist. `vo mcp` serves the Model Context Protocol on standard
input and output: `check`, `run`, `test`, `fmt`, `effects`, `explain`, `doc`, `query`, `impact` and `edit`
are tools whose arguments are the commands' operands and options, and whose result is the command's
`--json` output, its stderr and one `exit` line; the reference is the resource `vo://reference`. A call is
the command itself in a forked copy of the server, so a timeout, memory exhaustion or a crash ends with
that call, and the limits and the effect grant are the command line's. Output is cut at 1 MiB per stream;
a call can be cancelled. `--diff -` now also reads a pipe (it needed a regular file).

- **Not done (on purpose):** the HTTP transport, prompts, sampling, `build`/`package`/`replay` as tools,
  `--trace` and `--record` in calls, a sandbox (a program runs with what `main` declares, as always).

### 037: editor grammars (implemented)

vo was uncoloured everywhere but its own site. `editors/` now has a TextMate grammar in a VS Code
extension folder, a tree-sitter grammar with its generated parser and queries (and a Zed extension around
it), and Vim files. `tests/grammars.py` keeps them right: their word lists equal the reference's and the
parser's, the tree-sitter grammar parses every `.vo` file the toolchain parses, and the TextMate grammar
agrees with it token by token. The toolchain is unchanged; the node tools are needed only to change a
grammar or run those checks.

- **Not done (on purpose):** publishing to any marketplace or registry, the request to GitHub's language
  list (it needs wide use first), semantic highlighting (the language server's job).
- **Not verified by hand:** VS Code, Zed, Neovim and Helix were not available; only Vim was opened.

### 038: the toolchain in the editor (implemented)

`vo lsp` serves the Language Server Protocol. Each answer is computed in a forked copy of the server that
reads the editor's open documents in place of the files on disk, runs the ordinary command (`check`,
`query type-at`, `query refs`, `edit rename --dry-run`, `fmt`) with its output kept in memory and nothing
written, and turns the command's records into the protocol's answer. So the editor shows exactly what the
command line would, on unsaved text, and a text that breaks a check cannot end the session. A file's
program is the nearest `main.vo` above it that imports it. The JSON reader is shared with `vo mcp`
(`src/jsonrpc.c`).

- **Not done (on purpose):** completion and signature help (they need the checker to answer "what can
  stand here"), semantic tokens, inlay hints, test lenses, workspace symbol search, a Zed extension that
  starts the server (Zed needs compiled extension code), publishing the VS Code extension.
- **Not verified by hand:** no editor with a protocol client was available; a scripted editor compares
  every answer with its command.

### 039: TCP connections (implemented)

`std.tcp` hands a program the connection itself, under the effects it already had: `connect` needs `net`,
`listen` needs `serve`, and no effect was added. A `net` scope can name one port (`net("db:5432")`); a
bare host still means any port, and `std.http` obeys the port too. A connection is a number into a table
outside the collected heap, so a value kept after `close` is just "closed", and the effect it was opened
under is checked on every use. The HTTP client's sockets, name lookup and TLS are shared: the TLS exchange
was split into open, read, write and close, and `std.http` runs on the same pieces. A recording holds what
each wait for the peer brought in, so the three reads replay as ordinary code.

- **Not done (on purpose):** UDP, IPv6, TLS for accepted connections, a worker pool for TCP servers,
  trying every address of a name, rebuilding `std.http` on `std.tcp`.
- **Found on the way:** strings have no `\r` escape, so a protocol's line end is `chr(13) + "\n"`.

### 040: streaming (implemented)

Four functions take a function and call it once per piece: lines or chunks of a file, pieces of an HTTP
response body, lines of a child program's output. The function answers `ok(true)`, `ok(false)` (stop) or
an error. Each is a loop written in vo over a primitive, so nothing calls back from C. A file is never
kept open (a block is read at an offset, opened and closed each time); `send_stream` is written on
`std.tcp` with no new C; only a followed child is kept state, and a sweep ends any child a stopped
program or test left behind.

- **Not done (on purpose):** handles or iterators, `defer`, streaming upload, a child's output as bytes,
  streaming JSON or SQLite rows, an incremental hash (so a large file cannot be hashed in vo yet).

### 041: downloading packages (implemented)

`vo package ENTRY NAME URL` puts a fetch step in front of the pin spec 007 already had: the archive is
downloaded with the HTTPS client, inflated by a small gzip reader written for this, read as tar in
memory, and refused as a whole for a link, a device or a path that leaves the package. Its `.vo` files
go to a staging directory that is pinned and swapped in by renames, the manifest is written last, and
its line records the URL. `--restore` fetches missing packages again and accepts each only at its pin.
The command prints the effects the package's exported functions declare, read from the same records
`vo doc` prints, and what an update gains or loses.

- **Not done (on purpose):** git itself, zip, a registry, versions, fetching a package's dependencies,
  credentials, proxies, signatures.

### 042: strings (implemented)

A raw string, between backticks, takes its text as written, so a regular expression, SQL or JSON is no
longer written with doubled backslashes and escaped quotes. It is a second literal form, recorded as a
justified deviation from "one form per construct": the formatter and W0102 keep one spelling per text
(raw only for text with a backslash, a quote or a line break). Quoted strings gain `\r` and `\u{HEX}`,
with one escape per character. `for c in s` walks a string's characters without building a list.
`upper` and `lower` map Latin, Greek and Cyrillic letters from a generated table. Float literals take an
exponent.

- **Not done (on purpose):** interpolation, a character type, full case mapping (`ß`), normalization,
  hex and binary integer literals (with the bitwise operations of the review's item 5).

### 043: bits (implemented)

Checksums, hashes and binary formats need the bits of an integer. They come as functions in `std.bits`,
over one primitive, not as operators: the language keeps its eight precedence levels, and `&`, `|`, `^`,
`~`, `<<` and `>>` are errors that name the module. Shifts and rotations take a count of 0 to 63 (R0007
otherwise); `wrapping_add`, `wrapping_sub` and `wrapping_mul` keep the low 64 bits where the operators
stop with R0001. Integer literals gain `0x` and `0b` forms, each a 64-bit pattern, a second notation
recorded as a justified deviation: the formatter keeps the base a constant was written in. A table-driven
CRC-32 and FNV-1a over 1 MiB take well under a tenth of a second, so no instruction was added.

- **Not done (on purpose):** operators, `_` separators, octal, sized integer types (`u32`), counting
  bits (`popcount`, leading zeros), bit operations on `bytes`.

## Planned after 017

### 044: decimal (implemented)

Money needs numbers that hold 0.10 exactly. `std.decimal` is a library type over `int`: a `Decimal` is
a count of units and a number of places (0 to 18), and the whole module is written in vo, with no new
builtin (100,000 invoice lines take 80 ms). Sums and products are exact; `div` and `round_to` always
take a `Rounding`; a result that does not fit stops with R0001, and only then: division is long
division on the remainder, so no 128-bit value is ever formed. The places are kept (1.50 prints as
1.50), which means `==` tells 1.5 from 1.50: `equal`, `compare` and `normalize` go by value.
`split_evenly` and `allocate` give parts that sum to the amount. No decimal literal and no `+` on
decimals: functions only. Every operation is checked against Python's `decimal` on 10,000 random cases.

### 045: security (implemented)

Access control was the part of a `std.web` service still written by hand: a route added without a
guard was open, and nothing said so. `std.security` borrows Spring Security's model (everything is
refused unless a rule allows it, the rules are one table, ways of signing in plug into it) and adds
what a value-oriented language can: the table is plain data, `decide` is the step's decision as a pure
function, `describe` prints who may call each route, and `review` finds routes without a rule and
rules that never apply. Signing in is stateless: signed tokens (JWT, HMAC-SHA256) with a sign-in
route, HTTP Basic, or a key checked by the application; sessions and cookies wait for a store that
worker processes can share. The module is written in vo and has no effects of its own: a token check
adds about 20 microseconds to a request.

### 046: iteration (implemented)

A `for` had one variable, so a map was looped as `for k in keys(m)` with `m[k]` in the body and a list
with positions as `for i in 0..len(xs)` with `xs[i]`. A `for` now takes two: `for k, v in m` and
`for i, x in xs`. Such a loop visits what the collection had when the loop began (a map: the keys it
had, each visited if it is still there, with its value at that moment; a list: one round per index it
had), which is what the written-out loops did, so warning W0403 can rewrite them with a fix and one
form remains. The review's `for i in 0..n by 2` was left out: no loop in the library or the examples
steps by other than 1. `std.lists` adds the small list functions, written in vo; for its descending
sort the builtin `sort_by` learned to take a key of an `ordered` type parameter.

### 047: workers (implemented)

`concurrent_map` and `serve_workers` start work, wait for it and return. A worker (`std.workers`) is
the same mechanism, a copy of the program in a process of its own, without the waiting:
`start_worker(f)` returns a typed handle, and the two sides exchange messages (`tell` and `receive`,
`next_message` and `emit`). That gives a program work that lasts beside it, a time limit for one call
(`with_deadline`), a wait on several sources (`receive_first`), work that can be stopped
(`stop_worker`) and computation on several cores. A worker's runtime error is a value where the
program receives; no worker outlives the program. A worker's output travels with its messages and is
written by the program when it receives past it, so a program whose receives name their worker prints
the same bytes on every run; time limits, the first of several and a stop are where timing enters, and
a recording holds what the program got from its workers, so a replay starts none. Workers are started
by the program's own process only, not in a task or a serving worker. Protocols on top (server-sent
events, WebSockets) are separate work: a worker reads the connection with `std.http` or `std.tcp`.

### 048: time zones (implemented)

`std.time` was UTC only. The IANA tz database is now built into the binary (compiled once when it is
vendored, 424 KB), so a zone by name is a pure value: no effect, no `/usr/share/zoneinfo`, the same
answer in a test, in a container with nothing but the program, and on three architectures. Reading the
system's files was declined because an effect scope cannot be an absolute directory, so a program
would have needed every file to print a date. The zone code is vo: the reader of compiled zone files,
the rule that holds after the listed changes, and the conversions. `from_local` does not pick when a
local time is skipped or repeated: the error says which, with the instants. `local_zone` is the one
function that reads the machine (`TZ`, `/etc/localtime`), under `clock`, recorded for replay and UTC
with a fixed clock. The data is as old as the toolchain: `vo version` names the release, every release
of vo takes the newest, and `zone_from_bytes` reads a file a program brings itself. Month and weekday
names, locale formats and calendar arithmetic ("a month later") are not part of it.

### 049: a PostgreSQL package (implemented)

`packages/postgres`, a client written in vo on `std.tcp` and `std.crypto`, added with `vo package`; the
toolchain did not change (it needed spec 050 first). It has the names, parameters, results and error
texts of `std.sqlite`, so a program moves between the two by changing its imports, the code that opens
the database, its effects and the SQL the two spell differently; the notes API example exists in both
forms and a test fails on any other difference. `?` parameters on both (the package numbers them),
TLS by default with the server verified, SCRAM-SHA-256 with the server's proof checked, every wait
under a time limit, failures named like `std.sqlite`'s plus `connection`, `timeout`, `tls`, `auth`
and `protocol`. Not done: check-time SQL checking for PostgreSQL (`vo check` knows only `std.sqlite`'s
`sql`), connection pools, `LISTEN`/`NOTIFY`, `COPY`, cancelling a running statement, SASLprep of
non-ASCII passwords. Found on the way: `vo test` keeps a test's connections open until it ends, so a
property that connects per case holds up to 100 connections (the example's test helper ends the
earlier ones).

### 050: scoped effects through package functions (implemented)

Found while planning the PostgreSQL package (spec 049): a function that is given its target (a
package's `connect(host, ...)`, a helper that reads the file it is handed) could only declare the
plain effect, and then every caller up to `main` had to as well, so a program using such a function
lost its scopes. Now an effect declared without scopes by a function other than `main` or a test
means "within my callers' scopes": a scoped caller may call it, and when the program runs a target
is checked against every scoped declaration on the call path, not only the running function's and
the root's. The checker's other reports are unchanged, and it no longer offers "drop your scopes" as
a fix. No syntax, code or reference tokens were added. Not done: comparing literal hosts with `net`
scopes at check time (only file, `env` and `process` literals are), and naming the parameter that is
the target so that literals at a helper's call sites could be checked.

### 051: web pages (removed by 053)

**Removed by spec 053 (2026-11-23)**: vo targets backend network services; pages are the frontend's.
Form-body binding stays. What follows is the record of what was built.

`std.html`: a page is an HTML string literal or constant with `format`'s `{}` holes, checked by `vo
check` (tags nest and close; a hole is text, a quoted attribute or a URL, never in a script, style,
event handler, name, comment or unquoted value) and filled with values escaped for their place; a URL
whose scheme could be chosen by a value is checked when it is inserted. Pages are composed from `Html`
values, which only templates make; there is no template language. `std.web` answers pages with
`html_reply` (with a content security policy) and reads HTML forms into the same input records as JSON.
The new reference text was paid for by shortening the module rows. Chosen against: a template language,
template files, an escape hatch for raw markup. Not done: cookies, sessions and sign-in forms with
cross-site protection (done in spec 052), static files, file uploads, showing form errors
on the page without the operation's help.

### 052: sessions and sign-in forms (implemented; page parts removed by 053)

Since spec 053 the library serves no sign-in page: the sign-in route answers a failed form with 303 to
`SIGN_IN?failed=1&next=...`, and the frontend shows the page. The rest stands as written.

People sign in to a `std.web` application through an HTML form and stay signed in with a cookie:
`std.security`'s `browser(b)` is one more way of signing in in the policy, under the same rules and
with the same `require_user`, and `browser_routes(b, p)` adds the sign-in page, the form and sign-out.
Sessions are kept by the application (functions over its context, as `find_account`), which stores
only the SHA-256 of the cookie's 16 random bytes; sign-out and removed accounts take effect at once.
The cookie is `__Host-`, `HttpOnly`, `Secure`, `SameSite=Lax`; a state-changing request signed in by it
must come from the application's own origin (`Sec-Fetch-Site`, else `Origin`), with nothing put into
forms; a page that needs a user is sent to the sign-in page and back, an API still gets 401. `std.web`
gains `Cookie`, `cookie`, `with_cookie`, `read_cookie`; `Way` gains `sign_in_page` and `origin`. The
toolchain did not change. Decided with the user: sessions stored rather than signed cookies (sign-out
must end them), and recordings kept as they are (they hold sign-in passwords and cookies, and are
documented as secrets). Not done: sliding expiry, rate limiting (events make it possible), sign-in with
other providers, sign-up and password reset, multi-factor sign-in. Found on the way: a type's effect argument
parsed only with effect parameters and sets first, while `vo fmt` writes them last (fixed); the checker
does not infer an effect parameter from a mixed effect argument (`Job[uses clock, D]` passed where
`Job[uses D, clock]` is expected in another generic function): not fixed.

### 053: no HTML rendering in the core (implemented)

Decided with the user: vo is for backend network services written by agents and reviewed by people;
rendering pages is the frontend's job. `std.html` (spec 051), its C template checker, its two builtins
and `std.web`'s `html_reply` are removed, with their cases, the "Web pages" guide and the notes-pages
example; form-body binding stays. E0335–E0340 are the first retired codes: they stay in the catalog,
marked retired, and the catalog check fails if the toolchain names one again. Browser sign-in (spec
052) keeps everything but the page; it moved into the secure-notes example beside tokens. Found on the
way: the 401 challenge offered the session cookie's name as an authentication scheme (fixed). Chosen
against: an HTML package now (later, as a builder API, if a service needs it).

### 054: recorded tests (implemented)

Tests of code that calls other services run once against them with `vo test --record DIR` and
afterwards offline with `vo test --replay DIR`: one readable file per test, written by the recording
code of `vo run --record` around each test (the runner already isolates tests). Test recordings also
keep the headers and body of each HTTP request and the bytes of each TCP send, and replay compares the
body and the bytes, so a changed request fails that test with R0014, naming it and the recorded one;
a missing recording fails the same way without reaching outside. Notes for unused recorded inputs and
a changed program; recordings no test uses are listed. Credentials (`Authorization`,
`Proxy-Authorization`, `Cookie`, `Set-Cookie`, and names given with `--redact`) are written as
`[redacted]`. Decided with the user: plain `vo test` stays live; redaction at record time. New example
`examples/rates` records a call to a public HTTPS API. Not done: secrets in bodies or paths (not
detected), recording a test that serves, expiry.

### 055: operating a service (implemented)

A new effect `log` covers log records and metrics, outputs to the operator. `std.log` writes one JSON
record per line on stderr (time, level, message, the request's id, fields); names that look like secrets
are written `[redacted]` at any depth, and W0506 points at a field whose value is a secret-named variable.
`std.metrics` keeps counters, gauges and histograms in a registry shared by `serve_workers`' processes;
`metrics_route` answers the text format 0.0.4. `request_logs` and `request_metrics` are steps that carry
their own effects (`Step[C, uses log, clock, E]`): the checker now infers `E` as the app's effects without
them. `health_routes` answers liveness and readiness apart; `--drain` bounds the graceful stop, which ends
with a `stopped` record. Decided with the user: a new effect rather than `console`; secrets by name. Not
done: secrets under other names (not detected), per-worker metrics without the shared registry, tracing.

### 056: consistency pass (implemented)

The language's and the library's inconsistencies from the audit of 2026-11-25
([`docs/review-2026-11/inconsistencies.md`](review-2026-11/inconsistencies.md), section 3), fixed before
1.0 as breaking changes of vo 0.2.0, each with a diagnostic and a fix so applying `vo check`'s fixes
migrates a program. One code, E0214 `library_changed`, comes from one table of library changes (renames,
removals, lookups now giving an `Option`, swapped arguments, renamed fields and cases, unit changes with
the factor its fix applies). Decided with the user: rename one side of each clash (no import aliases);
lookups return `Option`; locals may not shadow builtins; `bool` and variants are ordered. Kept apart on
purpose, with the reason in the guide: per-type names in `std.collections`, `decimal.total` and
`absolute`, `with_cookie`'s result. Deferred: `web.bearer_auth`/`api_key_auth` beside the
`std.security` ways (no mechanical fix), explicit type arguments on calls (new syntax).

The open gaps, ordered by the question vo's target sets (decided 2026-11-23, spec 053): **does it make
an agent-written network service easier to write, review or operate?** Each becomes a spec when it is
next; anything that reaches the outside world gets an effect, scopes, and record and replay.

| Gap | What is missing | Notes |
|---|---|---|
| Events | clients and servers for server-sent events and WebSockets | the model is there since spec 047: a worker reads the connection and sends each event on; the protocols are not |
| Schema migrations | a database's tables change by hand | for `std.sqlite` and the `postgres` package alike |
| Serving over HTTPS | `std.http.serve` answers plain HTTP only; a public server needs a proxy in front | spec 019 follow-up: certificate and key under the program's grants |
| Rate limiting | refusals are events (spec 045, 052), nothing slows a caller | sign-in first |
| Benchmark v2 run | whether vo helps agents is still unmeasured | see Benchmarks; service-shaped tasks first |
| Workers in tasks and servers | `start_worker` and `with_deadline` only in the program's own process (spec 047) | a table of workers per process |
| Confined child programs | a program started under `process` runs with the user's full rights | opt-in confinement to the grant (Landlock); spec 017 follow-up |
| UDP, IPv6 | TCP over IPv4 only (spec 039) | when a program needs them; IPv6 for the HTTP client and `std.tcp` together |
| A registry, versions | packages are fetched by URL and pinned by hash (spec 041); nothing resolves versions or dependencies | when there are enough packages to need it |
| Debugger | traces, locals in errors and replay, but no stepping | replay makes a deterministic stepper possible |
| `old()` in `ensures` | contracts cannot compare with values from before the call | spec 005 follow-up |
| Tuples | no tuple type | map keys done in spec 033; tuples declined for now: a record groups values and is a key |
| macOS | Linux only (x86-64, ARM64, RISC-V) | the raw-syscall, no-libc design makes this a real port |

User-defined interfaces, compiling to machine code and rendering pages stay out of scope (below).

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
- Rendering pages (decided 2026-11-23, spec 053): vo is for backend services; templates, client scripts and static files belong to the frontend in front of them.
