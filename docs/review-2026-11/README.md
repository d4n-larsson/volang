# vo review, November 2026

A review of vo at commit `7e681b9` (spec 030), with suggestions for making it faster and for what it
would take to compete with mainstream languages.

| File | Contents |
|---|---|
| [optimizations.md](optimizations.md) | Performance of what exists: measurements, the defects found, and fixes in order |
| [language.md](language.md) | Language features that are missing, with sketches in vo's style |
| [ecosystem.md](ecosystem.md) | Standard library, tooling, platforms, distribution and project basics |

## How it was done

- Read `README.md`, `docs/reference.md`, `docs/roadmap.md`, the `ponytail:` notes in `src/` and `std/`,
  and the core of `vm.c`, `gc.c`, `compile.c`, `check.c`, `rt_map.c` and `std/json.vo`.
- Ran micro-benchmarks with the native x86-64 `build/vo` against Python 3.12 and Node on this machine.
  Every figure in `optimizations.md` is from those runs unless it says "from the code".
- Not done: the server was not load-tested, ARM64 and RISC-V were not measured, and the test suite was
  not run (nothing in the toolchain was changed).

## Where vo stands

vo is already ahead of mainstream languages on what it set out to do. Checked and enforced effects with
a reviewable record, diagnostics with fixes, deterministic runs with record and replay, built-in
property, coverage and mutation testing, and `vo query`/`vo edit` exist in one 10 MB static binary that
starts in about 2.5 ms. No big language has that set.

On raw speed the VM is in the right class for an interpreter: about 2× CPython on calls and 3× to 16×
on loops and collections, and about 4× slower than Node's JIT on call-heavy code.

What keeps it from competing falls into three groups:

1. **A few performance cliffs** that an ordinary program walks off: removing from a map, building a
   string in a loop, parsing JSON of a few megabytes, and checking programs with thousands of items.
   Each is a local fix.
2. **Missing language pieces** that force workarounds in almost every real program: errors are only
   strings, there are no constants, tuples, bitwise operations or user-defined map keys, and no way to
   read a string without turning it into a list.
3. **Reach**: the HTTP client cannot send a header, nothing streams, there is no TCP, no macOS, no way
   to ship a program as one file, no license file and no CI for the test suite.

## The first ten things to do

In order of value for effort. Details are in the linked files.

| # | What | Why | Size |
|---|---|---|---|
| 1 | Fix `remove` on maps ([optimizations](optimizations.md#1-remove-on-a-map-is-linear-in-the-maps-capacity)) | 200,000 removes take 14 s; the fix is a few lines | hours |
| 2 | Add a `LICENSE` and run `make gate` in CI ([ecosystem](ecosystem.md#project-basics)) | nobody can legally use vo today; the suite only runs on your machine | hours |
| 3 | HTTP client: headers, methods, timeouts ([ecosystem](ecosystem.md#standard-library)) | `get(url)` and `post(url, type, body)` cannot call any API that needs a token | days |
| 4 | Hash table for top-level names in the checker ([optimizations](optimizations.md#3-checking-is-quadratic-in-the-number-of-top-level-items)) | check time quadruples when the program doubles | a day |
| 5 | `byte_at`, interned one-character strings, native JSON parsing ([optimizations](optimizations.md#4-text-is-processed-through-chars-at-40-bytes-per-character)) | a 13 MB JSON document runs out of the 512 MiB limit | days |
| 6 | String building that is not quadratic ([optimizations](optimizations.md#2-building-a-string-with--in-a-loop-is-quadratic)) | 200,000 appends take 4.9 s and allocate 24 GiB | days |
| 7 | Typed errors: `result[T, E]` ([language](language.md#1-typed-errors)) | `std.web` already parses error kinds out of message prefixes | a spec |
| 8 | `vo build`: a program as one executable ([ecosystem](ecosystem.md#distribution)) | today a user of a vo program needs the toolchain and the source tree | a spec |
| 9 | Finish the benchmark v2 run ([ecosystem](ecosystem.md#evidence)) | the claim that vo helps agents is still unmeasured, and it is the whole pitch | days |
| 10 | Constants, tuples, structural map keys ([language](language.md)) | the most common workarounds in the examples | a spec each |

## A note on scope

Some suggestions here go against decisions in the roadmap (interfaces and native code are "out of scope
for now") or against the constitution's "one way to write each thing" and the 15,000-token reference
budget. Those are marked where they come up. vo's small surface is the reason agents can hold all of it,
so the recommendation throughout is to add what removes a workaround, and to say no to what only adds a
second spelling.
