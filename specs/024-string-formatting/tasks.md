---

description: "Task list for vo-lang spec 024: String Formatting"
---

# Tasks: String Formatting

**Input**: Design documents from `specs/024-string-formatting/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R9), data-model.md, contracts/language.md,
contracts/diagnostics.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's golden cases land first and fail before its code.

**Organization**:
- Setup.
- Foundational: the builtin end to end for plain `{}` placeholders (ids, template parsing, lowering, one
  value), which every story needs; and the fixture rename the new builtin name forces.
- One phase per user story: US1 and US2 are P1, US3 is P2, US4 is P3.
- Docs and verification.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Golden cases** go in `tests/cases/format/`. The directory's default command is `run {}` (see
  `tests/run.sh`), so run cases need only `.vo`, `.out`, `.code`. Check cases add a `.cmd` with
  `check --json {}` and expect JSONL in `.out` and `65` in `.code`, as `tests/cases/check/arg_count.*`.
- **Expected outputs** come from contracts/language.md and contracts/diagnostics.md, and every number
  printed by a run case is checked with Python (`python3 -c "print(format(v, spec))"`) before it is
  written into `.out`.
- **Spans**: diagnostics inside a template point at byte ranges of the literal's source (research R1);
  use `diag_add(code, 0 /* error */, file, start, end, msg)` (`src/diag.c`), not `ck_err`, for those.
- **Between edits**: `make quick AREA=format`. `make test` once at the end (T040); never two suites at
  once.
- **Shortcuts**: marked `ponytail:` in code (research R2's concatenation, R3's missing fast path).

---

## Phase 1: Setup

- [X] T001 [P] Create `specs/024-string-formatting/results.md` with sections SC-001 to SC-006,
  Verification, Deviations, as in `specs/022-sleep/results.md`.
- [X] T002 [P] `tools/quick.sh`: a `format` area whose cases are `format check lex doc` and whose
  scripts are `apply_fixes.py`, `catalog.py`, `ref_examples.py`, `format_diff.py --quick`; map
  `src/check_format.c`, `tests/format_diff.py` and `tests/cases/format/*` to it. Check with
  `QUICK_FILES=src/check_format.c tools/quick.sh --which`.
- [X] T003 [P] `tests/apply_fixes.py`: add `format` to the case directories it scans for fixes
  (today `lex`, `parse`, `check`, `multi`), in the code and in the module docstring.

---

## Phase 2: Foundational (blocks every story)

- [X] T004 [P] Golden run cases in `tests/cases/format/`, failing until T010:
  - `plain`: `format("{} of {} done", 3, 10)` → `3 of 10 done`; `format("plain")` → `plain`;
    `format("{} {} {}", 2.5, true, "x")` → `2.5 true x`; `format("{}", 0.1 + 0.2)` prints what
    `str(0.1 + 0.2)` prints;
  - `braces`: `format("{{}} {}", 1)` → `{} 1`; `format("{{{}}}", 7)` → `{7}`;
  - `escapes`: a template with `\t` and `\"` around a placeholder prints them decoded.
- [X] T005 [P] Golden check case `tests/cases/format/arg_type` (`.cmd` `check --json {}`): a list
  argument and a record argument each give E0301 "Expected `int`, `float`, `bool` or `str`, found `T`."
  at the argument, both in one run. Fails until T008.
- [X] T006 Rename the user function `format` to `reflow` in `tests/prop_eval/h07-word-wrap/` (the
  definition and calls in `ref/main.vo` and every `bugs/*.vo`, the import `main.format` and the call in
  `props.vo`); check with `python3 tests/prop_eval.py` (or its h07 subset) that results are unchanged.
- [X] T007 Builtin ids (research R2, data-model "Lowering"):
  - `src/check_expr.c`: `B_COUNT` 83; `builtin_names[82] = "format"`, `[83] = 0` (unnamed, like 50 and
    51); no `builtin_sigs` entry for either;
  - `src/vo.h`: `B_FORMAT (82)`, `B_FMT_PIECE (83)`, `B_LAST (83)`;
  - `src/rt_call.c`: `BU_FORMAT`, `BU_FMT_PIECE` at the end of the enum, the `_Static_assert` updated;
    `BU_FORMAT` is unreachable (lowered by the compiler: return 0), `BU_FMT_PIECE` returns
    `(long)fmt_piece(a0, arg0_type(call)->kind, call->e.i)`;
  - `src/check_effects.c`: both ids pure (no effect bit), sized for the new `B_LAST`.
- [X] T008 `src/check_format.c` (new, research R1), entry `Type *check_format_call(Node *call)`,
  called from `check_builtin` in `src/check_expr.c` for `B_FORMAT`:
  - fewer than one argument: E0302 as for other builtins (`check_argc`);
  - the first argument must be `N_STR` (E0315 is US2: here just `check_arg` it as `str` and return
    `t_str` without pieces when it is not a literal);
  - parse the literal's source bytes (`file_get(node->file)->text` from `start + 1` to `end - 1`),
    decoding `\n`, `\t`, `\\`, `\"`, into a `Vec` of `FmtPiece` (`src/vo.h`: `{ Str *text; long arg;
    long spec; long start, end; }`, `text` non-zero for a literal piece), with `{{` → `{`, `}}` → `}`,
    empty literal pieces dropped, and `{}` a placeholder with spec 0;
  - check each argument after the template with `check_arg(call, i, 0)`; a type other than `int`,
    `float`, `bool`, `str` is `err_one_of(arg, "`int`, `float`, `bool` or `str`", t)`;
  - store the vector in `call->e.p`; return `t_str`.

  Declare it in `src/vo.h`. The Makefile picks the file up (`C_SRCS := $(wildcard src/*.c)`).
- [X] T009 `src/compile.c`: `cformat(node, dst, mark)`, called from the `N_CALL` case when
  `id == B_FORMAT` (research R2):
  - no pieces vector (a template that failed to parse never reaches the compiler; assert) or zero
    pieces: `LOADK` of an empty-string constant;
  - each literal piece: a synthetic `N_STR` node (`alloc(NODE_SIZE)`, copy of the call's `file`,
    `start`, `end`; `a.s` the piece's `Str`) compiled with `cexpr`;
  - each placeholder: a synthetic call node, as `c_expect_part` builds one: `d.i = B_FMT_PIECE`,
    `b.v` a one-element vector holding argument `arg + 1` of the original call, `e.i` the packed spec,
    compiled with `cbuiltin`;
  - joined left to right with `c_emit(OP_CONCAT + (OPC_ADD_INT - OP_ADD_INT), dst, left, right, node)`;
  - `ponytail:` comment: one concatenation per piece (quadratic in pieces); a join instruction if long
    templates are measured slow.
- [X] T010 `src/num.c`: `Str *fmt_piece(long value, long kind, long spec)`, declared in `src/vo.h`. For
  spec 0: `int_to_str`, `float_to_str`, `bool_str`, or the string itself (by `kind`: `TY_INT`,
  `TY_FLOAT`, `TY_BOOL`, `TY_STR`). Root intermediate strings with `GC_ROOT` as `float_to_str` does.

**Checkpoint**: T004 and T005 pass; `make quick AREA=format` is green; `tests/prop_eval.py` unchanged.

---

## Phase 3: User Story 1 — Fixed decimals and columns (P1) 🎯 MVP

**Goal**: `{:<12}`, `{:>8.2f}`, `{:^5}`, `{:5}` and `{:.Nf}` print as contracts/language.md says.

**Independent Test**: the run cases below, byte for byte.

- [X] T011 [P] [US1] Golden run cases in `tests/cases/format/`, failing until T014:
  - `decimals`: `{:.2f}` of `3.14159` → `3.14`; of `2.675` → `2.67`; of `-0.001` → `-0.00`; `{:.0f}` of
    `2.5` → `2`, of `3.5` → `4`, of `0.5` → `0`; `{:.3f}` of `1.0005` (Python's value); `{:.20f}` of
    `0.1` → `0.10000000000000000555`; `{:.1f}` of `99.95` (Python's value); `{:.2f}` of `1.0 / 0.0` →
    `inf`, of `0.0 / 0.0` → `nan`, of `-1.0 / 0.0` → `-inf`; `{:.100f}` of `1.0 / 3.0` (Python's value);
    a large value (`1000000.0 * 1000000.0 * 1000000.0 * 1000000.0`, `{:.2f}`) and a tiny one (`1.0`
    halved 1074 times, `{:.5f}` → `0.00000`);
  - `columns`: `format("{:<12}|{:>8.2f}", "coffee", 4.5)` → `coffee      |    4.50`;
    `format("[{:5}][{:5}][{:5}]", 42, "ab", true)` → `[   42][ab   ][true ]`; `{:^5}` of `"ab"` →
    `" ab  "`; `{:3}` of `"héllo"` → `héllo` (never cut); `{:6}` of `"héllo"` → `héllo ` (code points);
    `{:>6.2f}` of `1.0 / 0.0` → `   inf`;
  - `receipt`: Story 1's receipt: three items and a total, one `format` per line.
- [X] T012 [US1] `src/stdprim.c`: `long exact_fixed(unsigned long bits, long prec, char *out)` (research
  R3), declared in `src/vo.h`. For finite `|x|` (sign handled by the caller): writes the integer part's
  digits then exactly `prec` fraction digits (no point; returns the number of integer digits, at least
  1), the exact value rounded at 10^-prec with ties to even on the last kept digit, the carry rippling
  into a new leading digit (`99.995` at `.2` → `100.00`). Reuse `bn_n`, `bn_d`, `bn_c`, `bn_set`,
  `bn_shl`, `bn_mul_small`, `bn_cmp`, `bn_sub` as `exact_digits` does: integer part by dividing N by D
  (digits of N / D, at most 309), then `prec` times `N = (N mod D) * 10`, digit = N / D; round on
  `2 * remainder` against D. Caller's buffer: 309 + 100 + 2 bytes.
- [X] T013 [US1] `src/check_format.c`: parse `{:spec}` in the fixed order of contracts/language.md's
  grammar (`[align]["+"]["0"][width][","]["." precision "f"][radix]`) into the packed spec of
  data-model.md ("Packed spec": "0–1 align: 0 default, 1 `<`, 2 `>`, 3 `^`; 2 plus; 3 zero; 4–13 width
  0–1000; 14 group; 15 has precision; 16–22 precision 0–100; 23–24 radix 0 none, 1 `x`, 2 `b`"). In this
  task only alignment, width and `.Nf` must be accepted; `+`, `0`, `,`, `x`, `b` are parsed into the
  spec too (US4 tests them). Anything the grammar rejects returns a parse failure for US2 to report (for
  now: no pieces, the call checked as `str`).
- [X] T014 [US1] `src/num.c`, `fmt_piece` for alignment, width and precision (data-model "Formatting one
  value"):
  - `float` with precision: `nan`, `inf`, `-inf` as `float_to_text` writes them; otherwise sign (`-`
    when the sign bit is set, so `-0.00`), then `exact_fixed` digits with a `.` before the last `prec`
    digits (none for `.0f`);
  - padding to `width` code points (`cp_count`) with spaces: default right for `int` and `float`, left
    for `str` and `bool`; `<`, `>`, `^` (`^`: left gets `pad / 2`, right the rest);
  - never cut a longer value.

**Checkpoint**: T011 passes; MVP: Story 1 works for every value a program can compute.

---

## Phase 4: User Story 2 — Mistakes are caught before the program runs (P1)

**Goal**: every template mistake is a check error at the placeholder; all in one pass; no runtime
failures.

**Independent Test**: the check cases below (code, span, message, fix in the JSONL).

- [X] T015 [P] [US2] Golden check cases in `tests/cases/format/` (`.cmd` `check --json {}`), failing
  until T018–T020, messages verbatim from contracts/diagnostics.md:
  - `not_literal`: `let t = "{}"; format(t, 1)` → E0315 at `t`, no fix;
  - `count`: `format("{} and {}", a)` → E0316 at the template, "The template has 2 placeholder(s) and 1
    argument(s) follow it.", no fix; and `format("{}", 1, 2)`;
  - `lone_brace`: `format("a { b", 1)` and `format("a } b")` → E0317 at the brace;
  - `unsupported`: `{:e}`, `{:#x}`, `{:*^9}`, `{:X}`, `{: }`, `{:5<}` (wrong order) → E0317 listing the
    options, no fix;
  - `bounds`: `{:1001}` and `{:.101f}` → E0317 "Width is at most 1000 and precision at most 100.";
  - `combos`: `{:<05}`, `{:0,}` → E0317 "`0` pads numbers by itself: drop the …"; `{:,x}` and `{:.2fx}`
    → E0318 / E0317 as contracts/language.md's "Not allowed together" says;
  - `option_type`: `{:.2f}` with an `int` → E0318 with fix `float(n)` on the argument; `{:x}` with a
    `float`, `{:+}` with a `str`, `{:,}` with a `bool` → E0318, no fix;
  - `one_pass`: a function with four different template mistakes and one ordinary type error elsewhere:
    all five reported, nothing cascades from them (no E0301 on the `format` result).
- [X] T016 [P] [US2] `docs/errors.md`: entries E0315 `format_template_not_literal`, E0316
  `format_argument_count`, E0317 `format_placeholder_invalid`, E0318 `format_option_type`, in the
  E03xx section after E0314, each with the paragraph, `Fixable:` (never / sometimes / sometimes /
  sometimes), an Example and a Fixed program, as the E0301 entry. `tests/catalog.py` must pass.
- [X] T017 [P] [US2] Golden run case `tests/cases/format/no_runtime_error`: the largest width and
  precision (`{:1000}` of `1`, `{:.100f}` of `1.0 / 3.0`), the minimum int, NaN: the program runs with
  exit 0 (FR-012).
- [X] T018 [US2] `src/check_format.c`: E0315 (the first argument is not `N_STR`: report, then check the
  other arguments as expressions only) and E0316 (count mismatch: report at the template literal's span,
  then check arguments without specs). Leave `call->e.p` unset after either.
- [X] T019 [US2] `src/check_format.c`: E0317 at the placeholder's source range for: a lone `{` or `}`;
  an option outside the grammar or out of order; width > 1000 or precision > 100; `0` with an alignment;
  `0` with `,`; `,` or `.Nf` with `x`/`b`. Report every bad placeholder of a template (one pass), then
  skip spec checks for that call.
- [X] T020 [US2] `src/check_format.c`: E0318 by argument type, per contracts/language.md "Options by
  argument type" (`width`, `<` `>` `^`: all; `+`, `0`, `,`: `int` and `float`; `.Nf`: `float`; `x`, `b`:
  `int`), message "`$1` applies to $2, but the argument is `$3`."; for `.Nf` on `int`, the fix wraps the
  argument (`diag_add_fix` over the argument's span with `float(` + its source text + `)`, as
  `suggest_conversion` does).

**Checkpoint**: T015–T017 pass; `tests/apply_fixes.py` applies the E0318 fix and the case re-checks.

---

## Phase 5: User Story 3 — Habits from other languages get a fix (P2)

**Goal**: `%d`, `{0}`, `{name}`, `{:.2}`, `{:d}`, `{:0>5}` and `f"..."` each get a diagnostic whose fix
gives a program that checks and prints the intended text.

**Independent Test**: the check cases below; `tests/apply_fixes.py` applies each fix and re-checks.

- [X] T021 [P] [US3] Golden check cases in `tests/cases/format/`, failing until T024–T026:
  - `percent`: `format("%d items, %.2f each", n, p)` → E0316 with fix `"{} items, {:.2f} each"`;
    `format("%5s|%-5d|%05d|%+d|%f|%x|%%", s, n, n, n, p, n)` → fix `"{:>5}|{:<5}|{:05}|{:+}|{:.6f}|{:x}|%"`;
    `format("%e", p)` and `format("%X", n)` → E0316, no fix; `format("{%d}", n)` → fix `"{{{}}}"`;
  - `percent_text`: `format("{}%", pct)` → no diagnostic (run case printing `50%`);
  - `indexed`: `format("{0} and {1}", a, b)` → E0317, fix `"{} and {}"`; `format("{1} {0}", a, b)` →
    no fix;
  - `named`: `format("hi {name}, {user.age}")` with no arguments → E0317, fix
    `format("hi {}, {}", name, user.age)` (the whole call); with an argument present → no fix;
  - `precision_no_f`: `{:.2}` → E0317 "Precision needs `f`: `{:.2f}` is 2 decimals.", fix `{:.2f}`;
  - `type_letters`: `{:d}`, `{:s}`, `{:>5d}` → E0317, fixes `{}`, `{}`, `{:>5}`;
  - `fill_zero`: `{:0>5}` with an `int` → E0317, fix `{:05}`; with a `str` → no fix;
  - `fstring` (in `tests/cases/format/`, `.cmd` `check --json {}`): `print(f"hi {name}, {n:>3}")` →
    E0120 over `f"..."`, fix `format("hi {}, {:>3}", name, n)`; `f"{a + b}"` → E0120, no fix; the rest
    of the file reports its own errors normally (no E0110).
- [X] T022 [P] [US3] `docs/errors.md`: entry E0120 `f_string` after E0119 (Fixable: sometimes); E0316's
  Fixed example shows the `%` translation. `tests/catalog.py` must pass.
- [X] T023 [P] [US3] Golden run case `tests/cases/format/habits_fixed`: the programs of T021 with their
  fixes applied by hand, printing the intended text (so the fixed forms are pinned by output too).
- [X] T024 [US3] `src/check_format.c`: the `%` translator (contracts/diagnostics.md, E0316 table): only
  when the template has no `{}` placeholder and at least one argument; parse `%` `[-+0]*` `[width]`
  `[.prec]` verb; translate by the argument's checked type (`%5s` on `str`/`bool` → `{:>5}`, on numbers
  → `{:5}`; `%-5d` → `{:<5}`, `%-5s` → `{:5}`; `%f` → `{:.6f}`); `%%` → `%`; literal `{`/`}` doubled. A
  fix only when every verb translates and their number equals the arguments'. The fix replaces the
  template literal's source span (re-escaping `\n`, `\t`, `\\`, `\"`).
- [X] T025 [US3] `src/check_format.c`: E0317 fixes (contracts/diagnostics.md, E0317 table): indexed
  placeholders all in order `0, 1, 2, …` → `{}` (one fix replacing the template); all placeholders named
  `name(.name)*` and no arguments → replace the whole call's span with `format("…{}…", names…)`; `.N`
  without `f` → `.Nf`; a trailing `d` or `s` removed; fill `0` with `>` on an `int`/`float` argument →
  `0` flag.
- [X] T026 [US3] `src/lex.c`: identifier `f` immediately followed by `"` (research R6): report E0120
  (`lx_err`) over `f"…"`, do not push the `f` token, lex the string normally. The fix, when every
  `{…}` hole (not `{{`/`}}`) is `name(.name)*` optionally followed by `:spec`, replaces the span with
  `format("TEXT", HOLES)`. The message: "vo has no f-strings: use `format` with `{}` placeholders and the
  values as arguments."

**Checkpoint**: T021–T023 pass; `tests/apply_fixes.py` covers every fix of this phase.

---

## Phase 6: User Story 4 — Numbers for people and for machines (P3)

**Goal**: `+`, `0`, `,`, `x`, `b`, equal to Python's output.

**Independent Test**: the run case below and `tests/format_diff.py` (SC-001).

- [X] T027 [P] [US4] Golden run case `tests/cases/format/numbers`, failing until T029:
  `{:,}` of `1234567` → `1,234,567`; `{:,.2f}` of `1234567.891` → `1,234,567.89`; `{:,}` of `-1000`,
  `999`, `0`; `{:+}` of `5` → `+5`, of `0` → `+0`; `{:+.1f}` of `-0.25` → `-0.2`; `{:05}` of `42` →
  `00042`, of `-42` → `-0042`; `{:+06.2f}` of `3.14159` → `+03.14`; `{:x}` of `255` → `ff`, of `-255`
  → `-ff`; `{:08b}` of `5` → `00000101`; `{:x}` and `{:b}` of `-9223372036854775807 - 1` (Python's
  values); `{:,}` of a float without precision (`1234.5` → `1,234.5`); `{:06.2f}` of `1.0 / 0.0` →
  `   inf`. Verify every expected line with Python first, except the `inf` line (Python prints
  `000inf`; vo pads infinities with spaces by design, contracts/language.md).
- [X] T028 [P] [US4] `tests/format_diff.py` (research R7): generates vo programs of 5,000 lines,
  `print(format("{:SPEC}", EXPR));`, with values built as R7 says (int literals; `float(a) / float(b)`
  with `|a|, |b| < 2^53`; `* 10.0` / `/ 10.0` chains; ties `0.125`, `2.5`, `2.675`, `1.005`; powers of
  two; extreme normals and subnormals by repeated halving), and specs from every allowed combination of
  align × `+` × `0` × width 0–12 × `,` × precision 0–10 × radix, per contracts/language.md (floats only
  with a precision). Seeded (`random.Random(1)`); 100,000 lines by default, `--quick` 5,000; `--cross
  BIN…` compares the outputs of several builds (`qemu-user` through `bin/vo` with `VO_BIN`). Prints the
  first differing line with its value and spec, exits 1 on any difference.
- [X] T029 [US4] `src/num.c`, `fmt_piece` completed: `+` (a `+` for non-negative values, including
  `0.0`; `-0.0` keeps `-`); grouping `,` every three digits of the integer part (decimal `int` and
  `float`); `x`/`b` of the magnitude as `unsigned long` (`0 - (unsigned long)v` for negatives, so the
  minimum int works), lowercase; zero flag: zeros between the sign and the digits up to the width; NaN and
  infinities padded with spaces even with `0`; floats without precision: `float_to_text`'s text, then
  `+`, `,`, `0` and width as above.

**Checkpoint**: T027 passes; `python3 tests/format_diff.py` reports no difference.

---

## Phase 7: Polish and verification

- [X] T030 [P] `docs/reference.md` (contracts/language.md "Reference text"): the builtin row in the
  `## Builtins` table; the strings bullet's last sentence ("… and no interpolation: use `format` or
  `+`."); the "Formatting" paragraph with its runnable example and output; the catalog sentence "E0101
  to E0120". `python3 tests/ref_examples.py` passes; record the token growth (≤ 350) in `results.md`.
- [X] T031 [P] Golden case `tests/cases/doc/format` (`.cmd` `doc format`): the row's signature and
  notes; and `tests/cases/query/` hover on a `format` call (`query type-at`) showing the same doc, as
  the spec 023 cases for other builtins.
- [X] T032 [P] `site/docs/guide/basics.md`: the strings bullet (lines 47–48) says to use `format` or
  `+`, with one short example `format("{:<8}{:>6.2f}", name, price)` that `make site-check` runs; and
  `site/docs/guide/standard-library.md`: where it shows `pad_left`/`align_right`, mention `format`'s
  width and `0` options.
- [X] T033 [P] `docs/roadmap.md`: spec 024 row **Done**; the "String formatting" gap row removed; a short
  section with what was built and "Not done (on purpose)": interpolation, `e`, `%`, fill characters, `#`,
  debug printing of records and lists, locale-aware output. `README.md`: the status line and spec range
  (001–024).
- [X] T034 [P] SC-004 timing in `tests/perf/format_speed.sh` (or a function in `tests/perf.sh`):
  1,000,000 iterations of `format("{} {}", a, b)` against `str(a) + " " + str(b)`, and of
  `format("{:>10}", i)` against `align_right(str(i), 10)`; median of 5 runs each; `format` not slower.
  Time `{:.2f}` for the record. If a `format` figure is slower, add R3's 128-bit fast path to
  `exact_fixed` (`|x| < 2^64`, `prec ≤ 19`) or fix the cause, and record it in `results.md`. Not in
  `make test` (needs an idle machine); add to `make gate`.
- [X] T035 In `src/compile.c` and `src/stdprim.c`, every `ponytail:` comment of this change names its ceiling and upgrade (R2's concatenation;
  R3's bignum-only digits unless T034 added the fast path).
- [X] T036 `make lint` (strict warnings on all three architectures).
- [X] T037 `make instrumented` (ASan/UBSan): `tests/cases/format/` and `tests/format_diff.py --quick`
  run clean.
- [X] T038 Cross-platform SC-001: `make ARCH=aarch64` and `make ARCH=riscv64`, then
  `python3 tests/format_diff.py --cross build/vo build-aarch64/vo build-riscv64/vo` reports identical
  output.
- [X] T039 SC-006 (benchmark): add four formatting tasks (receipt, table, percentages, hex dump) to the
  benchmark v2 feature set in `bench2/` with hidden graders, following the existing feature-set task
  layout; record in `results.md` that the agent run is pending with the rest of benchmark v2 (as the
  roadmap's Benchmarks section says).
- [X] T040 `make test` on x86-64, then `make ARCH=aarch64 test` and `make ARCH=riscv64 test`, one at a
  time.
- [X] T041 Fill in `results.md` (each SC with its figure or evidence); set the spec status to
  Implemented; mark every task [X].

---

## Dependencies & Execution Order

- **Phase 1** (T001–T003): in parallel.
- **Phase 2**: T004, T005 (failing cases) and T006 (rename) in parallel; then T007 → T008 → T009 →
  T010. Blocks every story. T006 must land before or with T007: once `format` is a builtin, the h07
  fixtures stop checking (E0202).
- **US1** (T011–T014): after Phase 2. T012 and T013 are independent (different files); T014 needs both.
- **US2** (T015–T020): after Phase 2 and T013 (the spec parser it reports from). T018 → T019 → T020
  (same file).
- **US3** (T021–T026): after US2 (its fixes attach to US2's diagnostics). T024 and T025 are in the same
  file (sequential); T026 (`src/lex.c`) is independent of both.
- **US4** (T027–T029): after US1 (`fmt_piece`'s padding and `exact_fixed`); T028 can be written any
  time after Phase 2.
- **Polish**: T030–T034 in parallel after the stories; T035 → T036 → T037 → T038; T039 any time; T040
  → T041 last.

## Parallel Examples

- Phase 2: T004, T005 and T006 together.
- US1: T011 (cases), T012 (`src/stdprim.c`) and T013 (`src/check_format.c`) together.
- US2: T015, T016 and T017 together, then T018–T020.
- US3: T021, T022, T023 and T026 together, then T024 → T025.
- US4: T027 and T028 together, then T029.
- Polish: T030, T031, T032, T033 and T034 together.

## Implementation Strategy

- **MVP**: Phases 1–3: `format` with `{}`, columns and fixed decimals (the need vo cannot meet today).
- **Then**: US2, which turns every template mistake into a check error: the reason `format` is a
  checked builtin.
- **Then**: US3 (habit fixes, which cut agents' round trips) and US4 (signs, zeros, grouping, hex, with
  the Python differential test).
- **Last**: reference, docs, speed, the three platforms once.
