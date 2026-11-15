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
| [007](../specs/007-stdlib-packages/) | Standard library and content-pinned packages | **Partly done, merged.** `vo doc`, `std.json`, `std.text`, `std.time`, `std.random` and packages work and are tested; HTTP networking, some tests, the task-program evaluation and the reference section are deferred until after 008 ([results](../specs/007-stdlib-packages/results.md)) |
| [008](../specs/008-c-rewrite/) | The toolchain in C: native builds on Linux x86-64, ARM64 and RISC-V 64, byte-identical behavior | **Done** ([results](../specs/008-c-rewrite/results.md)) |
| Ongoing | Benchmarks that can tell languages apart | Continuous |

## What mainstream languages get wrong for agent-written code

1. **Nobody can see what changed in what the code is allowed to do.** An agent fixes a bug and also adds a network call, a file delete or an environment read. In Python, TypeScript or Go that is invisible in the diff unless someone reads every line. **Addressed by 002:** effects in every function type, scopes, run-time enforcement, and `vo.effects`, so a reviewer approves "save_report gains file.write" instead of reading the diff.
2. **The agent's tests share the agent's misunderstanding.** Code that compiles and passes its own tests is weak evidence. **Addressed by 005:** property tests with inputs generated from types and shrunk counterexamples, and contracts checked on every call and return.
3. **Unfinished code does not compile at all.** Agents work step by step, but big languages reject the whole program until it is complete. **Addressed by 005:** typed holes.
4. **Agents invent APIs that do not exist.** The model's memory drifts from the installed versions. **Partly addressed by 001** (the reference is embedded in the binary). **Planned in 007:** exact signatures for the standard library and dependencies, and packages pinned by content hash.
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

### 007: standard library and packages

- JSON, time, random, text, and HTTP behind the `net` effect (002 already defines and checks it; no operation uses it yet).
- `vo doc --json`: exact signatures for the installed version.
- Packages pinned by content hash and resolved at check time, never at run time; a package's effects appear in `vo.effects`, so a new dependency that wants the network is visible in review.

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

- Concurrency.
- Compiling to machine code: vo runs bytecode (004), and native code waits until that becomes the bottleneck for agent-sized programs.
- User-defined interfaces or traits: generics with `ordered` and effect parameters cover the cases seen so far.
