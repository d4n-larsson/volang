# Research: vo-lang Core Language and Toolchain

Phase 0 output. Each entry records a decision, why it was made, and what else was considered. No NEEDS CLARIFICATION items remain.

## 1. Execution strategy

- **Decision**: A tree-walking interpreter over the type-checked AST. The checker rewrites generic operator nodes into type-specific ones (e.g. `+` becomes `ADD_INT`, `ADD_FLOAT`, or `CONCAT`), so the evaluator never inspects types at runtime.
- **Rationale**: This is the least code that meets the spec. The spec excludes standalone executables, and SC-004 only needs sub-second startup at 1k lines. AST nodes already carry spans, so runtime errors and call traces (FR-025) need no extra mapping.
- **Alternatives**: A bytecode VM (roughly +2k lines of assembly for speed nobody asked for). Native code generation (needs an emitter, a linker step, and a debug mapping; out of scope for v1).

## 2. Platform, ABI, and build

- **Decision**: A static RV64 ELF that uses Linux syscalls directly (asm-generic numbers, 64-bit `time_t`, so plain `clock_gettime`), with no libc and entry at `_start`. It is built with `riscv64-linux-gnu-as -march=rv64imafd -mabi=lp64d` and `riscv64-linux-gnu-ld`. On x86 hosts, `bin/vo` runs it as `exec qemu-riscv64 build/vo "$@"`; on RV64 hosts it runs `build/vo` directly. Every edit is checked by the SESI `riscv64` pack (RV64IMAFD with the lp64d calling convention, including `partial_save` for `sw`/`lw` used on 8-byte registers).
- **Rationale**: The user requires assembly. With RV64, the language's 64-bit `int`, its `float`, and pointers each fit in a single register (§3, §5). Both the binutils and qemu packages are available through apt. Skipping libc removes every runtime dependency. Because no crt0 sets `gp`, the binary is linked with `--no-relax` (otherwise `ld` turns `la` into `gp`-relative addressing). Note: riscv64 Linux has no `renameat`; `vo fmt` uses `renameat2`.
- **Alternatives**: RV32. This was the original choice, made when the SESI pack covered only RV32. It was rejected once the pack gained RV64 support (2026-11-13): RV32 would have required register-pair arithmetic for every `int` operation, software 64-bit division, and memory round trips to move f64 values between float and integer registers. Bare metal with an SBI console was also rejected, because it has no files, args, or stdin, and FR-015 requires them.

## 3. Checked 64-bit integer arithmetic

- **Decision**: `int` is a signed 64-bit value held in one register. The evaluator emits checked operations inline:
  - **add/sub**: `add t,a,b`. Overflow occurs when `(t < a) != (b < 0)` (two `slt` and one `bne`).
  - **mul**: `mul lo` and `mulh hi`. Overflow occurs when `hi != lo >> 63` (arithmetic shift).
  - **div/rem**: hardware `div`/`rem`. RISC-V does not trap, so `b == 0` (R0002) and `a == MIN && b == -1` (R0001) are checked explicitly first.
  
  Integer↔text and float↔text conversion live in `num.s`.
- **Rationale**: 32-bit integers would make agents hit R0001 overflow on ordinary sums and counters. On RV64, 64-bit checked arithmetic takes only 3–5 instructions per operation.
- **Alternatives**: 32-bit `int` (rejected for the reason above). Arbitrary precision (far more code; YAGNI).

## 4. Memory management

- **Decision**: A bump allocator over `mmap`'d 16 MiB chunks, with a hard cap of 512 MiB. Nothing is ever freed. Growing a list or map copies it into a new block and abandons the old one. When the cap is reached, the program stops with R0006 `out_of_memory`.
- **Rationale**: FR-010 only requires that the programmer never manages memory and that memory errors are impossible. A program that never frees cannot use memory after freeing it. Agent-sized programs fit easily in 512 MiB.
- **Ceiling**: Allocation-heavy loops, such as string building in a million-iteration loop, can exhaust the cap. The upgrade path is a copying collector: object headers already carry their size (data-model.md), so one can be added without changing object layouts.
- **Alternatives**: Reference counting (needs a refcount update at every copy, spread across the interpreter). Mark-sweep or copying GC now (needs root scanning of the interpreter's native stack; not needed for v1).

## 5. Value representation

- **Decision**: Every value occupies a uniform, untagged 8-byte slot, which is exactly one integer register: `int` is an i64, `bool` is 0 or 1, reference types hold a 64-bit pointer, and `float` holds the f64 bit pattern. A float moves between registers with `fmv.d.x`/`fmv.x.d` only where arithmetic needs it. That lets the evaluator pass every value through the same `a0` path. Static types make runtime tags unnecessary. Heap layouts are defined in data-model.md. Maps are open-addressing hash tables (FNV-1a with a fixed seed) indexing an entries array kept in insertion order, so iteration order is deterministic (FR-009).
- **Alternatives**: Tagged values (only needed for dynamic typing). Sorted-key maps (O(log n) inserts with shifting; insertion order is just as deterministic and cheaper).

## 6. Syntax decisions (full grammar in contracts/grammar.md)

Each decision serves FR-001 (one spelling per construct), FR-002 (kind known from the first token), and FR-003 (explicit delimiters):

| Decision | Reason |
|---|---|
| Every statement starts with a keyword (`let var set if while for match return break continue`) or is a call | First-token classification. Assignment is `set x = …;`, so `x = …` never parses as an expression |
| `;` ends every simple statement; blocks use `{}` and take no `;` | Explicit terminators and no automatic semicolon insertion. An error on a missing `;` comes with a mechanical fix |
| Construction uses a keyword: `new Point { x: 1 }`, `new list[int]`, `new map[str, int]` | Avoids the Go ambiguity of `if x {`. Empty collections have exactly one spelling |
| No trailing commas, no optional parentheses, no `else`-less ternary, no lambdas | One spelling |
| `match` is a statement, it must be exhaustive, and there is no wildcard | FR-013. Adding a case forces every match on that type to be updated |
| `result[T]` is a built-in variant with cases `ok(T)` and `err(str)`; `try e` propagates errors; an unused `result` is an error | FR-007. The control flow is visible at the call site through the `try` keyword |
| Comments are whole lines beginning with `#`; trailing comments are not allowed | The formatter keeps comments as statement-level nodes, so it never loses or moves them |
| Naming is enforced: `Types` and `Variants` in UpperCamel, everything else snake_case | Readable at a glance, and helps the fix-suggestion matcher |
| Imports name one item: `import geom.Point;` loads `geom.vo` and binds `Point` | FR-016. A file plus its import lines fully determines its meaning |

## 7. Diagnostics format

- **Decision**: Human mode prints `file:line:col: error[E0301]: message`, followed by the source line, a caret span, and `fix:` lines. `--json` emits JSON Lines with a `"v":1` schema version and a final `summary` record (contracts/output-schema.md). Columns are 1-based and counted in code points. Each record also includes `text`, the exact source covered by the span, so an agent can apply a fix by string replacement without counting columns.
- **Collection**: Diagnostics are collected into an arena list, sorted by position, capped at 50 (with a `truncated` count in the summary), and printed once. Cascade suppression works by giving AST nodes that failed to parse or type-check the `<error>` type. That type is compatible with everything and produces no further diagnostics.
- **Fix suggestions**: Undefined names use Levenshtein distance ≤ 2 against names in scope. Missing imports come from a top-level-name scan of `*.vo` files under the importing file's directory (`getdents64`). Mechanical fixes cover a missing `;`, a trailing comma, `let`→`var` on assignment, and wrapping in `str(…)` when a `str` is expected and an `int`/`float` is found. When fixes overlap or touch, even across diagnostics, only the first in source order is kept.

## 8. Time limits

- **Decision**: The interpreter increments a step counter at every loop iteration and function call. Every 65,536 steps it reads `clock_gettime64(CLOCK_MONOTONIC)` and compares against the deadline. If a deadline is exceeded, it raises R0009 `timeout`, which exits with status 124. In `vo test`, each test gets its own deadline, and a timeout marks that test and moves on to the next.
- **Blocking stdin**: A step counter cannot interrupt a blocked `read`. Before every read of fd 0, `read_line`/`read_all` call `ppoll` on fd 0 with the remaining time until the deadline, and raise R0009 when it times out. Files are never slow enough to need this.
- **Alternatives**: Signals and timers (`timer_settime` plus a signal handler means asynchronous state in assembly; polling is simpler and deterministic).

## 9. Stack exhaustion

- **Decision**: At `_start`, `mmap` a 64 MiB stack and move `sp` to the top of it. This works the same natively and under qemu-user, where the guest stack size is fixed by the emulator and `setrlimit` cannot grow it. The parser, checker, and evaluator each compare `sp` against `base + 1 MiB` when entering their recursive routines. If the limit is crossed, the parser and checker emit E0115 and the evaluator raises R0005.
- **Rationale**: One comparison per frame. This also covers deeply nested source, not only deep recursion at runtime.

## 10. Reference, error catalog, and examples

- **Decision**: `docs/reference.md` and `docs/errors.md` are embedded with `.incbin`. `vo reference` prints the reference unchanged. `vo explain CODE` finds the `## CODE` heading in errors.md and prints that section. In the reference, every ` ```vo ` block followed by an ` ```output ` block is extracted by `tests/ref_examples.py`, then run and compared (FR-030). The script also counts tokens (approximated as `words × 1.3`) against SC-002. Multi-file, `vo-test` and `vo-fmt` example blocks follow conventions defined in tasks.md. A separate `tests/catalog.py` runs every Example and Fixed program in the catalog, and checks that each code has a golden case that triggers it. Each catalog section carries a `Fixable: yes|no` line, which SC-003's fix-coverage measure uses.
- **Rationale**: One source of truth, so the installed binary cannot drift from its documentation.

## 11. Float text conversion

- **Decision**: Float literals and `parse_float` accept at most 15 significant digits and no exponent. They are converted as an exact int64 mantissa divided by an exact power of ten, which is correctly rounded in IEEE f64 (the classic fast path). `str(f)` scales to 15 significant digits, rounds to an int64, emits digits, and trims trailing zeros (`3.14`, `1.0`, `0.1`). `nan` and `inf` print as words.
- **Ceiling**: The output is not always the shortest round-trip form; 15 digits is enough to identify a value but is not minimal. The upgrade path is a Ryu port.
- **Alternatives**: Exact or shortest algorithms (Ryu or Grisu need 128-bit tables and ~1k lines of assembly).

## 12. Testing the toolchain

- **Golden tests**: `tests/cases/<area>/name.vo`, with optional `name.cmd` (arguments), `name.in` (stdin), `name.out` (expected stdout, including any `--json` records), `name.err` (expected stderr), and `name.code` (exit status). The full conventions are in tasks.md. `tests/run.sh` runs each case and diffs the results. This one mechanism covers lexing through formatting, because everything is observable through the CLI.
- **Fuzzing (SC-007)**: `tests/fuzz.py` is created in the foundational phase and runs in short form (200 iterations) in every `make test`. mutates corpus files by deleting, duplicating, or swapping bytes and tokens, runs `vo check --json` and `vo run --timeout 2`, and asserts three things: the exit status is in {0–63, 65, 70, 124}, every stdout line parses as JSON, and the process never takes longer than the timeout plus 1 s.
- **Determinism (SC-006)**: `tests/run.sh --repeat 100` reruns the `run/` cases and compares their outputs byte for byte.
- **Agent benchmark (SC-001, SC-008)**: 50 tasks in `bench/tasks/`. `bench/run_agent.sh` gives a headless agent (`claude -p`) only the output of `vo reference` and the task, allows at most 3 attempts with toolchain output fed back, and records the attempt counts. The SC-008 baseline runs the same tasks in Python.
- **Assembly correctness**: The SESI riscv64 hook checks every edit for calling-convention problems. Behavior is covered by the golden cases, including `run/int_*.vo`, which exercises the overflow and division edge cases (`MIN/-1`, `/0`, `mulh` boundaries).

## 13. Toolchain prerequisites (not installed on the dev host as of 2026-11-13)

`sudo apt install binutils-riscv64-linux-gnu qemu-user` (provides `qemu-riscv64`). Python 3.12 is already present. Verified that both packages are available on the host's Ubuntu release. The SESI `riscv64` pack is enabled in `.sesi/packs`, and its examples lint as expected.
