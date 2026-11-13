# Results: vo-lang v1 (001-vo-lang-core)

Recorded 2026-11-13 on an x86_64 host. `bin/vo` runs the RV64 binary under `qemu-riscv64`.

## Build

- 23 assembly files, about 21,200 lines (RV64IMAFD, lp64d, no libc), producing one static binary.
- The SESI `riscv64` lint is clean for every file except `src/jmp.s`. That file is setjmp/longjmp, which breaks the calling convention on purpose; the exemption is explained in its header.
- Lint issues found along the way are collected in `sesi-linter-issues.md`.

## Quality gate (`make test`): passing

| Check | Result |
|---|---|
| Golden cases (`tests/run.sh`) | 134 / 134 pass (cli, lex, parse, check, run, test, fmt, multi) |
| Determinism, quick (`--repeat 3`, run cases) | 38 / 38 |
| Fuzz, quick (200 iterations, check + run) | 0 failures |
| `apply_fixes.py` | 41 / 41 fixes resolve their diagnostic (100%) |
| Fix coverage (SC-003) | 24 / 25 diagnostics with always-fixable codes carry a fix (96%); situational codes 22 / 35; overall 46 / 84 (55%) |
| `catalog.py` | 52 codes: every Example produces its code, every Fixed version checks and runs clean, every code has a golden case |
| `ref_examples.py` | 25 examples pass; the reference is ~6,500 tokens (budget 15,000) |
| `fmt_corpus.py` | 70 files: idempotent, and behavior is unchanged |

## Success criteria

| Criterion | Result |
|---|---|
| SC-001: agent solves ≥70% of tasks on the first attempt, ≥95% within 3 | **met**: 98% first attempt, 100% within three (see Benchmark) |
| SC-002: reference ≤ 15,000 tokens | **met**: about 6,500 tokens (words × 1.3) after the follow-ups |
| SC-003: diagnostics carry code, span and message; ≥80% of always-fixable diagnostics have a fix; ≥95% of fixes resolve the error | **met**: every record has a code and span (enforced by the schema and golden cases); 96% coverage; 100% of fixes resolve |
| SC-004: check + run of a 1,000-line program in under 1 s | **met**: 1,121 lines, check 31 ms + run 37 ms = 68 ms under qemu (`tests/perf.sh`); 10,000-line and 10,000-character inputs work |
| SC-005: formatter idempotent and behavior-preserving | **met**: 66 corpus files |
| SC-006: identical output over 100 runs | **met**: `tests/run.sh --repeat 100 tests/cases/run`, 33 / 33 (8 min 26 s) |
| SC-007: no crash, hang or unstructured output | **met**: 5,000 fuzz iterations (seed 1) before the follow-ups. After them, a 1,500-iteration run found one crash (`0.(1)`: a `.` with no name followed by `(`); it was fixed and added as golden case `parse/dot_without_name`. Re-runs: seed 7, 1,500 iterations, and seed 8, 3,000 iterations, both with 0 failures |
| SC-008: agents need fewer attempts with vo than with Python | **not met (tie)**: 1.02 mean attempts for both; the task set is too easy to separate the languages |

## Benchmark (SC-001, SC-008)

50 tasks in `bench/tasks/` (188 stdin/stdout cases, all verified against `ref.py`). They are run with `bench/run_agent.sh --lang vo|python` (headless `claude -p`, at most 3 attempts) and summarized by `bench/summarize.py`.

Run on 2026-11-13 with the default `claude -p` model, against the toolchain **before** the follow-up fixes below (the binary was frozen for the run).

| Language | First attempt | Within 3 | Mean attempts | Agent time |
|---|---|---|---|---|
| vo (context: `vo reference` only) | 49 / 50 (98%) | 50 / 50 (100%) | 1.02 | 908 s |
| Python | 49 / 50 (98%) | 50 / 50 (100%) | 1.02 | 279 s |

- **Same retry in both languages.** The only retry was the same task in both, `32-csv-quoted` (quoted CSV fields), so it measures the task rather than the language.
- **SC-001 is met with a wide margin**: an agent that had never seen vo wrote correct programs from the reference alone.
- **SC-008 is a tie, not a win.** Both languages hit the ceiling, so these 50 tasks cannot show a difference. A harder task set (longer programs, refactoring of existing code, more error-prone I/O) is needed to test the claim. vo's agent time is about 3× Python's, mostly because every prompt carries the ~6.5k-token reference.
- Raw results: `bench/results/{vo,python}.jsonl`. Summary: `python3 bench/summarize.py`.

## Follow-ups (from the benchmark-task author's friction log)

Status after the follow-up pass on 2026-11-13. Each fix has golden cases and passes the full gate.

From the benchmark-task author's friction log (55 probe snippets of habitual Python/JS/Rust mistakes, plus 10 vo solutions written from `vo reference` alone; all 10 passed first time):

1. **Insertion fixes glued to tokens: done.** E0110 insertion is now offered only for `)`/`]` before a token that can follow them. There are dedicated messages with working fixes for: `&&`, `||` and `!` → `and`, `or`, `not`; `'a'` → `"a"`; `xs.len()` → `len(xs)`; `elif` → `else if`; `Point { … }` → `new Point { … }`; `,` between fields → `;`; `var x: int;` → `= 0` (or `""`, `false`, `0.0`, `new list[T]`); `x += 1;` → `set x = x + 1;`; `-> void` removed. `?` and `{…}` used as a map literal get explanatory messages. Original text: A fix that inserts the expected token (E0110) is placed at the end of the previous token without spacing. In cases like `for k, v in m` or `case Shape.circle(r)` it is the wrong fix, and applying it brings the error back. Proposal: offer the insertion only when the next token can follow the inserted one. Add dedicated messages and fixes for common carry-overs: `&&`, `||` and `!` → `and`, `or`, `not`; `'a'` → `"a"`; `xs.len()` → `len(xs)`; `elif`; a ternary `?:`; `{...}` map literals; a record literal without `new`; commas between record fields; `var x: int;` without an initializer.
2. **E0201 suggestions on short names: done.** Names of 4 characters or fewer only get distance-1 suggestions, and candidates must fit the use: functions or builtins (never `main`) at call sites, values elsewhere, records or variants for capitalized names. `case Ok(v)` gets a fix to `ok`, its binding no longer cascades, and E0401 is not added on top. `return ok;` gets a fix to `ok()`. `int("5")` points to `parse_int`. Original text: For example `max(a, 4)` → `a` and `min` → `main`. Proposal: require the distance to be smaller than a third of the name length, and match the kind (only suggest functions or builtins at a call site).
3. **Missing builtins: done**, except the keyed sort, which needs first-class functions (still open): `abs`, `min`, `max`, `upper`, `lower`, `ord` and `chr` were added. Original text: `abs`, `min`, `max`, `upper`, `lower`, `ord`/`chr` (or `char_code`), and a keyed sort for records. Each addition costs reference tokens; there is plenty of budget left (6.3k of 15k).
4. **Memory: partly done.** The reference now recommends `push` + `join` for building strings. The out-of-memory report path was fixed: it previously fell back to T0003 because the R0006 report itself needs memory, and now gets 64 MiB of headroom (golden case `run/oom_small`). The garbage collector remains open. Original text: The arena never frees (the `ponytail:` marker in `sys.s`), so building a string char by char with `+` exhausts 512 MiB at about 35k characters. Short term: the reference should recommend `push` + `join`. Real fix: the planned copying GC.
5. **Number text rules: done.** Trailing zeros no longer count as significant digits, so `100000000000000.0` and larger integer-valued floats are accepted and exact. `read_line` strips `\r`. The reference documents that `parse_int` requires the whole text to be the number. A number glued to letters (`1e3`) now gets a clear E0105. Original text: `parse_int` rejects surrounding spaces and `+`, and `read_line` keeps a trailing `\r`. Float literals count trailing zeros as significant digits, so no integer-valued float of 10^14 or more can be written. Proposal: document this in the reference; count significant digits without trailing zeros; strip `\r` in `read_line`.
6. **E0207 renames only the declaration: done.** Every use of a badly named symbol gets its own E0207 with a fix, so one round of fixes renames everything (verified end to end). Original text: Uses keep the old name and need a second round of fixes. Proposal: one fix with several edits (a schema extension), or rename every use.
7. **Meaning-changing conversion fix: done.** An int/float mix now converts the int side with `float(...)`. Original text: `x / 3.0` with an int `x` gets `int(3.0)`, turning a float average into integer division. Proposal: for arithmetic, convert the int operand with `float(...)` instead.
8. **Unchanged, by design:** `vo fmt` drops parentheses that are redundant by precedence but help readability** (`a or (b and c)`). This is by the canonical-form rule, so it is recorded only.

## Other notes

- **Agent help.** Two background agents wrote the error catalog content, the language reference and the 50 benchmark tasks. Each was validated by the scripts above (`catalog.py`, `ref_examples.py`, and `ref.py` checks for all 188 benchmark cases). Their bug reports led to fixes for the empty string literal `""`, `explain`'s `fixable` flag, unhelpful suggestions, cascades after E0307, `+=`, `1e3`, `for _`, and the out-of-memory report.
- **SESI linter.** It caught many real frame-layout bugs while this was being written. Its gaps (13 entries, 2 patched in the pack with approval) are in `sesi-linter-issues.md`.
