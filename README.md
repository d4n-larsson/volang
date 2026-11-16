<img src="docs/vo.svg" alt="vo logo" width="96" height="96">

# vo

vo is a small, statically typed programming language designed for AI coding agents, and for the people who review what those agents write. The whole toolchain is freestanding C: lexer, parser, type checker, formatter, test runner, bytecode compiler, virtual machine and garbage collector, about 23,000 lines with no libc and no dependencies, plus under 200 lines of assembly per platform. It builds natively on Linux x86-64, ARM64 and RISC-V 64, with byte-identical behavior on all three.

```vo
record Person {
    name: str;
    age: int;
}

fn oldest_first(people: list[Person]) {
    sort_by(people, fn(p: Person) -> int {
        return 0 - p.age;
    });
}

fn save(people: list[Person]) -> result[void] uses file.write("out/") {
    return write_file("out/people.txt", join(map(people, fn(p: Person) -> str {
        return p.name;
    }), "\n"));
}

fn main() -> result[void] uses console, file.write("out/") {
    let people = [new Person { name: "al", age: 25 }, new Person { name: "cy", age: 30 }];
    oldest_first(people);
    print(people[0].name);
    try save(people);
    return ok();
}
```

**At a glance:** statically and strongly typed, with effects in every function type; imperative at its core, with functions as values, closures, generics, records and variants with `match`; no classes, exceptions or `null`. Programs are checked, compiled once to typed bytecode and run on a virtual machine (no JIT), with a deterministic garbage collector. C-style braces and semicolons, one canonical format. A small standard library built into the binary, local packages pinned by content hash, and no concurrency yet.

## Why another language

Mainstream languages were designed for people at keyboards. Agents work differently: they write code in one shot, read the compiler's output, and fix what it reports, often with nobody watching. vo is built for that loop.

- **Every diagnostic is machine-readable and comes with a fix.** `vo check --json` emits one JSON record per problem, with a stable code, the exact span, and (wherever one correction is unambiguous) a ready-to-apply edit. `vo explain E0501` prints the catalog entry with a broken and a fixed example.
- **One way to write each thing.** Every statement starts with a keyword or is a call, indentation means nothing, and `vo fmt` has one canonical output. There are no implicit conversions, no overloading, no exceptions and no `null`.
- **Effects are part of every function's type.** A function says what it may touch (`uses console, file.read("data/"), env("HOME")`), with directory and name scopes. A function without `uses` is pure. The checker demands that a function declares everything it and its callees do, and reports exactly what is missing. At run time, every path, host, variable name and address is checked again, against both the running function's declaration and the root grant (`main`'s or the test's), after resolving `..`, absolute paths and symbolic links; a link swapped in after the check cannot redirect the open.
- **Reviewable capability changes.** `vo.effects`, next to the code, records the effects of every function. When an edit makes a function do more, such as writing files, `vo check` fails (E0504) with the call path that introduced it. `vo effects --update` rewrites the record, and its diff shows a reviewer exactly what new capabilities the change grants, without reading every line.
- **Effects are a security boundary.** A program can do nothing outside its grant, whatever the rest of its code does, and the commands that do not run programs (`check`, `fmt`, `doc`, `edit`, ...) are safe on a hostile source tree. The threat model is [`docs/security.md`](docs/security.md); the first review's 32 findings are all fixed, each with a case that reproduces it ([`docs/security-review-1.md`](docs/security-review-1.md)).
- **Deterministic by default.** The same program and input produce the same bytes. The clock and randomness are effects, fixed with `--clock-ms` and `--seed` (and fixed by default in tests).
- **Memory and time are bounded and reported.** Unreachable values are reclaimed by a deterministic collector, so collection never changes output. Running out of memory means live data over a limit you set (`--max-memory`, R0006), `--mem-stats` reports usage, and `--timeout` stops even a single long builtin call.
- **Compiled to bytecode.** After checking, programs are compiled to typed bytecode for a virtual machine, about 4.5× faster than the tree-walking interpreter it replaced on compute-heavy code, with byte-identical results.
- **Unfinished code still checks and runs.** `todo("...")` is a typed hole: the checker reports the type it needs and the names in scope, and the rest of the program checks, runs and tests until the run reaches it.
- **Tests that look for counterexamples.** `prop "name" (xs: list[int]) { ... }` runs a property on 100 generated inputs and shrinks a failure to a minimal counterexample, deterministically. `requires`/`ensures` contracts are checked on every call and return.
- **Exact library signatures on demand.** `vo doc std.json.parse_json` prints the installed signature, effects and an example, from the same declarations the checker uses, so an agent never has to remember an API. Packages are pinned by content hash in `vo.packages`: a changed byte stops the check, and a package that starts using the network shows up in `vo.effects`.
- **Ask the toolchain, not grep.** `vo query callers|refs|type-at` answers from the checker (strings, comments and look-alike names never match), `vo impact --diff` lists the functions and tests a diff affects, and `vo edit rename` / `add-field` change every reference across files, check the result first, and write all files or none.
- **Debugging without print statements.** `vo run --trace` writes every call and return as JSON lines, runtime errors show each frame's parameters and locals, and `vo run --record` saves everything a run read so `vo replay` can run the fixed program on exactly that input.
- **The whole language fits in a prompt.** `vo reference` prints the complete reference, about 10,000 tokens with a runnable example in every section. That is all an agent needs to know.

The language also has records, variants with exhaustive `match`, `result[T]` with `try`, lists and maps, built-in tests, multi-file programs, functions as values and closures, and user-defined generics: inferred type arguments, `T: ordered`, and effect parameters for higher-order functions.

## Getting started

Requirements: GCC and GNU make on Linux (x86-64, ARM64 or RISC-V 64). A clean build takes a few seconds.

```sh
make                 # builds build/vo for this machine: one static binary
bin/vo version
```

`make ARCH=riscv64` or `make ARCH=aarch64` cross-builds `build-<arch>/vo` (with `gcc-riscv64-linux-gnu` or
`gcc-aarch64-linux-gnu`); `bin/vo` runs a binary for another architecture through `qemu-user`
(`VO_BIN=build-riscv64/vo bin/vo ...`). The cross checks also need those tools: `make check-tools` lists
what is missing.

```sh
bin/vo check --json main.vo        # diagnostics with fixes (JSON lines)
bin/vo run main.vo                 # check, then run
bin/vo test file.vo                # run the file's tests
bin/vo fmt main.vo                 # rewrite in canonical form (--check to only report)
bin/vo effects --update main.vo    # write vo.effects; prints the capability changes
bin/vo explain E0501               # catalog entry for an error code
bin/vo reference                   # the complete language reference
bin/vo doc std.json                # exact signatures of a library module (or of one item)
bin/vo package main.vo money ../shared/money   # pin a package directory by its content hash
```

Every command takes `--json`. Exit statuses are stable: 0 success, 1 failed tests, 64 usage error, 65 check errors, 70 runtime error.

## Repository layout

| Path | Contents |
|---|---|
| `src/` | The toolchain in C: `lex.c`, `parse.c`, `check*.c` (types, effects, functions as values, generics), `compile.c` and `vm.c` (the bytecode compiler and VM), `gc.c` (the collector), `interp.c` and `rt_*.c` (the runtime), `fmt.c`, `main.c` (CLI); `vo.h` holds every layout and declaration |
| `src/platform/` | Per architecture: process entry, system calls and `setjmp`/`longjmp` in assembly (`x86_64.S`, `aarch64.S`, `riscv64.S`) and the system call numbers; `hosted.c` for the sanitizer build |
| `docs/reference.md` | The language reference (embedded in the binary; `vo reference`) |
| `docs/errors.md` | The error catalog: every code with an example and its fix (`vo explain`) |
| `tests/cases/` | Golden test cases: source plus expected stdout, stderr and exit status |
| `tests/*.py` | Fuzzer, fix applier, catalog and reference checks, formatter corpus, v1→v2 migration, effects-record change sets, scope fuzzer |
| `tests/cases/security/`, `tests/security/` | The security review's reproductions: golden and step cases, and the race, rename, package-swap and server-attack scripts |
| `bench/`, `bench2/` | Agent benchmarks (v1: 50 single-file tasks; v2: multi-file, refactoring, hidden-property and capability tasks, plus a feature set) |
| `specs/` | Specifications, plans, contracts, task lists and results, one directory per spec (`001-vo-lang-core` to `013-security-fixes`) |

## Development

```sh
make test     # the quality gate: golden cases, determinism, fuzzing, fixes, catalog, reference,
              # formatter corpus, migration, effects-record change sets, scope fuzzing
make lint     # strict-warning C for all three architectures, SESI over riscv64.S, platform file sizes
make gate     # lint, test, the UB-trap and collector stress suites, 2,000 programs against the
              # last assembly release, and the speed check
make instrumented   # the suite under ASan and UBSan (x86-64)
make security-check # the security review's races (1,000 runs each) and server attacks
make speed-check    # native speed against the assembly release under qemu (SC-003, SC-004)
sh tests/perf.sh    # check + run of a 1,000-line program in under 1 s; large inputs
```

Conventions are in `.specify/memory/constitution.md`:
- A behaviour change lands first as a golden case that fails.
- Every error code has a catalog entry and a golden case.
- Every reference example is executed by the test suite.
- The reference stays under 15,000 tokens.

Known false positives and negatives of the assembly linter (SESI, for `src/platform/riscv64.S`) are tracked in `sesi-linter-issues.md`.

## Benchmarks

`bench2/run_agent.sh --lang vo|python` runs a headless agent (`claude -p`) on the same tasks in both languages, with three attempts, the same feedback rules and hidden grading. For capability tasks, vo solutions are judged by their declared effects and Python solutions by observing what they do. `python3 bench2/summarize.py` reports the results. On the v1 benchmark both languages were at the ceiling (about 98% solved on the first attempt), which is why v2 exists. The agent runs with no tools, no user settings and an empty working directory, so it sees only the prompt. Results and the status of every success criterion are in `specs/*/results.md`; the v2 run is not complete yet.

## Status

vo 0.1.0, schema 1, built through specs 001-013 ([`docs/roadmap.md`](docs/roadmap.md) has the status of each): the core language and toolchain, checked effects, generics, a garbage collector, a bytecode VM, typed holes, property tests and contracts, query and structural editing, trace and replay, a standard library (`std.json`, `std.time`, `std.text`, `std.random`, plain-HTTP `std.http` with a server) with exact signatures from `vo doc`, content-pinned packages, native builds from C, file-private items with `export`, `@spec` annotations linking tests to requirements, and the fixes from the first security review. Its security model is in [`docs/security.md`](docs/security.md). The benchmark v2 agent run is still to be completed.

Not yet: data structures beyond lists and maps (ordered maps and sets, balanced trees, heaps: the next spec), concurrency, HTTPS and downloading packages.
