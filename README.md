# vo

vo is a small, statically typed programming language designed for AI coding agents, and for the people who review what those agents write. The whole toolchain is written in RISC-V assembly (RV64): lexer, parser, type checker, formatter, test runner, bytecode compiler and virtual machine, about 49,300 lines with no libc and no dependencies.

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

## Why another language

Mainstream languages were designed for people at keyboards. Agents work differently: they write code in one shot, read the compiler's output, and fix what it reports, often with nobody watching. vo is built for that loop.

- **Every diagnostic is machine-readable and comes with a fix.** `vo check --json` emits one JSON record per problem, with a stable code, the exact span, and (wherever one correction is unambiguous) a ready-to-apply edit. `vo explain E0501` prints the catalog entry with a broken and a fixed example.
- **One way to write each thing.** Every statement starts with a keyword or is a call, indentation means nothing, and `vo fmt` has one canonical output. There are no implicit conversions, no overloading, no exceptions and no `null`.
- **Effects are part of every function's type.** A function says what it may touch (`uses console, file.read("data/"), env("HOME")`), with directory and name scopes. A function without `uses` is pure. The checker demands that a function declares everything it and its callees do, and reports exactly what is missing. At run time, paths are resolved (`..`, absolute paths, symbolic links) and checked against the running function's declaration.
- **Reviewable capability changes.** `vo.effects`, next to the code, records the effects of every function. When an edit makes a function do more, such as writing files, `vo check` fails (E0504) with the call path that introduced it. `vo effects --update` rewrites the record, and its diff shows a reviewer exactly what new capabilities the change grants, without reading every line.
- **Deterministic by default.** The same program and input produce the same bytes. The clock and randomness are effects, fixed with `--clock-ms` and `--seed` (and fixed by default in tests).
- **Memory and time are bounded and reported.** Unreachable values are reclaimed by a deterministic collector, so collection never changes output. Running out of memory means live data over a limit you set (`--max-memory`, R0006), `--mem-stats` reports usage, and `--timeout` stops even a single long builtin call.
- **Compiled to bytecode.** After checking, programs are compiled to typed bytecode for a virtual machine, about 4.5× faster than the tree-walking interpreter it replaced on compute-heavy code, with byte-identical results.
- **Unfinished code still checks and runs.** `todo("...")` is a typed hole: the checker reports the type it needs and the names in scope, and the rest of the program checks, runs and tests until the run reaches it.
- **Tests that look for counterexamples.** `prop "name" (xs: list[int]) { ... }` runs a property on 100 generated inputs and shrinks a failure to a minimal counterexample, deterministically. `requires`/`ensures` contracts are checked on every call and return.
- **Ask the toolchain, not grep.** `vo query callers|refs|type-at` answers from the checker (strings, comments and look-alike names never match), `vo impact --diff` lists the functions and tests a diff affects, and `vo edit rename` / `add-field` change every reference across files, check the result first, and write all files or none.
- **Debugging without print statements.** `vo run --trace` writes every call and return as JSON lines, runtime errors show each frame's parameters and locals, and `vo run --record` saves everything a run read so `vo replay` can run the fixed program on exactly that input.
- **The whole language fits in a prompt.** `vo reference` prints the complete reference, about 10,000 tokens with a runnable example in every section. That is all an agent needs to know.

The language also has records, variants with exhaustive `match`, `result[T]` with `try`, lists and maps, built-in tests, multi-file programs, functions as values and closures, and user-defined generics: inferred type arguments, `T: ordered`, and effect parameters for higher-order functions.

## Getting started

Requirements: GNU binutils for RISC-V and, on non-RISC-V machines, QEMU user mode.

```sh
sudo apt install binutils-riscv64-linux-gnu qemu-user   # make check-tools verifies them
make                                                     # builds build/vo
bin/vo version
```

`bin/vo` runs `build/vo` natively on RISC-V and through `qemu-riscv64` elsewhere.

```sh
bin/vo check --json main.vo        # diagnostics with fixes (JSON lines)
bin/vo run main.vo                 # check, then run
bin/vo test file.vo                # run the file's tests
bin/vo fmt main.vo                 # rewrite in canonical form (--check to only report)
bin/vo effects --update main.vo    # write vo.effects; prints the capability changes
bin/vo explain E0501               # catalog entry for an error code
bin/vo reference                   # the complete language reference
```

Every command takes `--json`. Exit statuses are stable: 0 success, 1 failed tests, 64 usage error, 65 check errors, 70 runtime error.

## Repository layout

| Path | Contents |
|---|---|
| `src/` | The toolchain in RISC-V assembly: `lex.s`, `parse.s`, `check*.s` (types, effects, functions as values, generics), `compile.s` and `vm.s` (the bytecode compiler and VM), `interp.s` and `rt_*.s` (the runtime), `fmt.s`, `main.s` (CLI) |
| `docs/reference.md` | The language reference (embedded in the binary; `vo reference`) |
| `docs/errors.md` | The error catalog: every code with an example and its fix (`vo explain`) |
| `tests/cases/` | Golden test cases: source plus expected stdout, stderr and exit status |
| `tests/*.py` | Fuzzer, fix applier, catalog and reference checks, formatter corpus, v1→v2 migration, effects-record change sets, scope fuzzer |
| `bench/`, `bench2/` | Agent benchmarks (v1: 50 single-file tasks; v2: multi-file, refactoring, hidden-property and capability tasks, plus a feature set) |
| `specs/` | Specifications, plans, contracts and task lists (`001-vo-lang-core`, `002-effects-functions-generics`) |

## Development

```sh
make test     # the quality gate: golden cases, determinism, fuzzing, fixes, catalog, reference,
              # formatter corpus, migration, effects-record change sets, scope fuzzing
make lint     # every assembly function is checked by the SESI riscv64 pack
sh tests/perf.sh
```

Conventions are in `.specify/memory/constitution.md`:
- A behaviour change lands first as a golden case that fails.
- Every error code has a catalog entry and a golden case.
- Every reference example is executed by the test suite.
- The reference stays under 15,000 tokens.

Known false positives and negatives of the assembly linter are tracked in `sesi-linter-issues.md`.

## Benchmarks

`bench2/run_agent.sh --lang vo|python` runs a headless agent (`claude -p`) on the same tasks in both languages, with three attempts, the same feedback rules and hidden grading. For capability tasks, vo solutions are judged by their declared effects and Python solutions by observing what they do. `python3 bench2/summarize.py` reports the results. On the v1 benchmark both languages were at the ceiling (about 98% solved on the first attempt), which is why v2 exists. The agent runs with no tools, no user settings and an empty working directory, so it sees only the prompt. Results and the status of every success criterion are in `specs/*/results.md`; the v2 run is not complete yet.

## Status

vo 0.1.0, schema 1. Built through two specs:
- **001 (core language and toolchain):** complete.
- **002 (effects, functions as values, generics, benchmark v2):** implemented and passing every quality gate; the benchmark v2 run is still to be completed.

What comes next is in [`docs/roadmap.md`](docs/roadmap.md). Not yet: a standard library beyond the builtins (the `net` effect exists, but there are no network operations yet), packages, and concurrency.
