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
| Next | Complex data structures: balanced trees (AVL), ordered maps and sets, heaps (see below) | **Next spec, 014** |
| Planned | Concurrency (see below) | **After data structures** |
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

## Next: complex data structures

The next spec (014). vo has `list` and `map` (keys `int` or `str`, insertion-ordered), records and variants. Programs that need
an ordered map, a set, a priority queue or a balanced tree must build it themselves. That is possible
today: a generic AVL tree checks and runs, as a `variant Tree[T: ordered]` whose `node` case holds a
`record Node[T]`. Writing it showed where it costs more than it should, and where an agent is likely to
get it wrong:

- **Generic construction is verbose:** `new Node[T] { ... }` needs its type argument written out, and an
  empty `Tree.leaf` passed as an argument cannot be inferred (E0310) without a typed `let` first.
- **A variant case carries one value,** so every tree or list node needs a separate record.
- **`ordered` covers only `int`, `float` and `str`:** a tree keyed by a record or a tuple of fields
  needs a comparison function passed in by hand.
- **Rebalancing is easy to get subtly wrong** (rotations, height updates), and a wrong tree still
  returns plausible answers. This is where properties and contracts (005) should carry the weight.

What the spec should decide:

- **A standard-library module** (for example `std.collections`) written in vo: an ordered map and set on
  an AVL tree, a binary heap (priority queue), a deque. Each comes with properties for its invariants
  (balance, ordering, size) in its own tests, and `vo doc` examples.
- **Persistent or mutable:** persistent trees fit vo's value semantics and make replay and future
  concurrency simpler; mutable ones are faster. Probably persistent by default, measured against
  `list` and `map` on the speed benchmark.
- **Language changes the module needs, kept small:** inference for type arguments of `new` and of cases
  without a value; ordering for user types (a `compare` function parameter, or records of ordered fields
  ordered field by field).
- **Agent evaluation:** benchmark tasks that need one of these structures, with and without the module,
  to see whether a checked, documented library beats hand-written trees.

## Planned: concurrency

Today a vo program does one thing at a time: a server answers one request at a time (a slow client holds
it for up to 10 s), and a program that fetches ten URLs waits for each in turn. Concurrency comes
after the data structures spec. What it has to keep:

- **Determinism (Principle III).** The same program and inputs give the same output; record and replay
  keep working. A concurrent program's interleaving must be reproducible, or recorded and replayed.
- **Effects as a boundary.** Every concurrent task runs within its spawner's grant (as anonymous
  functions do today), and `vo.effects` still shows what each function may do.
- **No data races by construction.** Agents get shared mutable state wrong; the design should make it
  impossible rather than detected (for example tasks that share nothing and communicate by messages, or
  structured `spawn`/`join` over immutable inputs).
- **Structured lifetimes.** A task cannot outlive the block that started it, so errors, timeouts and
  `--timeout` have one place to go.

Open questions for the spec: concurrency on one thread (an event loop over the existing `ppoll`, enough for
I/O-bound servers and clients) or parallelism on several cores (the collector and the VM are single-threaded
today); the form in the language (a `spawn` builtin, a block, channels); how `serve` uses it to answer
requests concurrently.

## Benchmarks (ongoing)

On the v1 benchmark both languages were at the ceiling, so it could not show whether vo helps agents. Benchmark v2 (40 tasks in multi-file, refactor, hidden-property and capability categories, plus a 20-task feature set) is built and validated. Its run is the next step:

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
