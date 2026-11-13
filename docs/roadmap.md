# vo roadmap

vo's goal is to be the language AI coding agents do best in, and that humans can safely review. This roadmap is about what mainstream languages do poorly for agent-written code, and the specs that address it, in order. Each spec goes through the Spec Kit flow in `specs/NNN-*/` (spec, plan, tasks, implementation, `results.md`).

## Status

| Spec | Content | Status |
|---|---|---|
| [001](../specs/001-vo-lang-core/) | Core language and toolchain: check, run, test, fmt, explain, reference; JSON diagnostics with fixes; deterministic runs; multi-file programs | **Done.** All criteria met except "vo beats Python" on benchmark v1, which tied at the ceiling |
| [002](../specs/002-effects-functions-generics/) | Checked effects with scopes and a reviewable effects record; functions as values; user-defined generics; benchmark v2 | **Implemented, every gate passing.** The benchmark v2 run is not complete ([results](../specs/002-effects-functions-generics/results.md)) |
| [003](../specs/003-garbage-collector/) | Garbage collector and memory model; `--timeout` inside builtins; memoized generic resolution | **Done** ([results](../specs/003-garbage-collector/results.md)) |
| 004 | Typed holes, property tests and contracts | Planned |
| 005 | Query API, structural editing, trace and replay | Planned |
| 006 | Standard library and content-pinned packages | Planned |
| Ongoing | Benchmarks that can tell languages apart | Continuous |

## What mainstream languages get wrong for agent-written code

1. **Nobody can see what changed in what the code is allowed to do.** An agent fixes a bug and also adds a network call, a file delete or an environment read. In Python, TypeScript or Go that is invisible in the diff unless someone reads every line. **Addressed by 002:** effects in every function type, scopes, run-time enforcement, and `vo.effects`, so a reviewer approves "save_report gains file.write" instead of reading the diff.
2. **The agent's tests share the agent's misunderstanding.** Code that compiles and passes its own tests is weak evidence. **Planned in 004:** property tests with inputs generated from types and shrunk counterexamples; contracts checked in tests.
3. **Unfinished code does not compile at all.** Agents work step by step, but big languages reject the whole program until it is complete. **Planned in 004:** typed holes.
4. **Agents invent APIs that do not exist.** The model's memory drifts from the installed versions. **Partly addressed by 001** (the reference is embedded in the binary). **Planned in 006:** exact signatures for the standard library and dependencies, and packages pinned by content hash.
5. **Agents cannot see the whole codebase.** They grep, guess, and break code they never saw. **Planned in 005:** a query API and toolchain-performed refactorings.
6. **Debugging is mostly print statements.** **Planned in 005:** call traces as JSON, local values in runtime errors, and exact replay of deterministic runs.

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

### 004: typed holes, property tests and contracts

- **Typed holes.** `todo("parse the header")` type-checks; the checker reports the expected type and the names in scope there. Running stops at the hole with a structured R-code. The agent's loop becomes "fill one gap at a time" instead of "fix everything first".
- **Property tests.** `prop "sort keeps length" (xs: list[int]) { ... }` generates inputs from the declared types (generics from 002 make this general), with shrunk counterexamples as structured records.
- **Contracts.** `requires` and `ensures` on functions, checked at run time in tests.
- **Success:** on benchmark tasks with hidden properties, agents using properties catch more of their own mistakes before the hidden checks do.

### 005: query, structural editing, trace and replay

- **Queries:** `vo query callers parse_line`, `vo query type-at main.vo:12:5`, and `vo impact --diff` (which functions and tests a change affects).
- **Structural edits:** `vo edit rename` and `vo edit add-field`, verified by the checker instead of done by fragile text replacement.
- **Traces and replay:** `vo run --trace` writes calls with arguments and results as JSON lines; runtime errors include local values; failing runs can be replayed exactly.
- **Success:** better results on the refactor category of the benchmark.

### 006: standard library and packages

- JSON, time, random, text, and HTTP behind the `net` effect (002 already defines and checks it; no operation uses it yet).
- `vo doc --json`: exact signatures for the installed version.
- Packages pinned by content hash and resolved at check time, never at run time; a package's effects appear in `vo.effects`, so a new dependency that wants the network is visible in review.

## Benchmarks (ongoing)

On the v1 benchmark both languages were at the ceiling, so it could not show whether vo helps agents. Benchmark v2 (40 tasks in multi-file, refactor, hidden-property and capability categories, plus a 20-task feature set) is built and validated. Its run is the next step:

1. **Run v2 to completion** with the corrected harness: no tools, no user settings, an empty working directory. The first run was invalid because the agent could read the answers.
2. **Re-run v1** the same way: its results were collected with the flawed harness.
3. **Decide what comes next from the results.** If both languages are still at the ceiling, the next benchmark needs longer, unsupervised, multi-step work (where effects, review and refactoring matter), not just harder single tasks.

Known gaps in v2 to fix along the way:
- Per-case environments: tasks c03 and c08 never test their environment variable being set.
- Task c05 depends on its cases running in order.
- The Python sandbox cannot see effects done through C extensions; seccomp or strace would close that.

## Out of scope for now

- Concurrency.
- Compiling to machine code: vo stays a checked, tree-walking interpreter until speed becomes the bottleneck for agent-sized programs.
- User-defined interfaces or traits: generics with `ordered` and effect parameters cover the cases seen so far.
