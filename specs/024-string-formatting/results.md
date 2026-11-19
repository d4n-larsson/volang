# Results: String Formatting

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | `tests/format_diff.py`: 100,000 random `int` values and finite `float` values with a precision, over every allowed spec combination (widths 0–12, precisions 0–10, `x`/`b`, `+`, `0`, `,`, alignments): 0 differences from Python's `format()`. `--cross build-aarch64/vo build-riscv64/vo` (through qemu): output identical on all three platforms. |
| SC-002 | met | One check case per kind of mistake (`not_literal`, `count`, `lone_brace`, `unsupported`, `bounds`, `combos`, `option_type`, `one_pass`, `arg_type`) and per habit (`percent`, `indexed`, `named`, `precision_no_f`, `type_letters`, `fill_zero`, `fstring`), each with its own code. `tests/apply_fixes.py` applies every fix and re-checks: 92/92 resolved (was 78 before the habit fixes); `habits_fixed` runs the fixed programs and prints what Python prints for the original habits. |
| SC-003 | met | E0120, E0315–E0318 in `docs/errors.md`, each with golden cases (`tests/catalog.py`: 115 sections, 0 problems). Every acceptance scenario of the spec is a line of a golden case in `tests/cases/format/` (26 cases). |
| SC-004 | met | `tests/format_speed.py`, median of 5 runs of 5,000,000 iterations: `format("{} {}", i, i + 1)` 0.356 s against `str(i) + " " + str(i + 1)` 0.351 s (1.02×); `format("{:>10}", i)` 0.170 s against `align_right(str(i), 10)` 1.264 s (0.13×). `{:.2f}` (no equivalent before): 2.35 s per 1,000,000 values. In `make gate`. |
| SC-005 | met | `tests/ref_examples.py`: ~12,902 tokens of 15,000 (+175 from 12,727, limit +350); the new example runs. |
| SC-006 | pending | Four formatting tasks added to the benchmark v2 feature set (`bench2/features/f21-receipt`, `f22-table`, `f23-percentages`, `f24-number-bases`; `bench2/validate.py --set features`: 24 tasks, 0 problems). The agent run is pending with the rest of benchmark v2 (roadmap, Benchmarks). |

## Verification

| Check | Result |
|---|---|
| `make test` (x86-64) | passed: golden 796, step 211 + 235 + 35, the self-tests, fuzz, fixes (92/92), catalog, reference, site (195 examples), doc conformance (652 calls, 129 examples), `gc/baseline` (731 runs), examples, diff-engines 1,000 (0 disagreements) |
| ARM64, RISC-V | `make ARCH=aarch64 test` and `make ARCH=riscv64 test` (qemu): the same counts, all passed |
| `make lint` | clean (C for x86_64, aarch64 and riscv64; SESI; platform files) |
| `make instrumented` (ASan, UBSan) | passed 796 golden cases; `format_diff.py --quick` and `apply_fixes.py` under it: clean |
| `make gate` | not run (only when asked); its new step, `tests/format_speed.py`, was run: 0 failures |

## Size

- `src/check_format.c` (new): 535 lines: template decoding, the spec parser, E0315–E0318, the `%`
  translator, the index and name fixes.
- `src/lex.c` +70 (E0120 and its fix), `src/num.c` +103 (`fmt_piece`), `src/stdprim.c` +55
  (`exact_fixed`), `src/compile.c` +39 (`cformat`); ids in `src/vo.h`, `src/check_expr.c`,
  `src/rt_call.c`.
- Reference: +1 builtin row, the "Formatting" paragraph with one example, two sentences changed.
- Tests: 26 golden cases in `tests/cases/format/`, `doc/format`, `query/hover_format`;
  `tests/format_diff.py`, `tests/format_speed.py`; 4 benchmark feature tasks.

## Deviations

- **More C than planned** (about 830 lines against the plan's 450): the fixes (index, name, `%`
  translation, f-strings, fill `0`) and the source-offset mapping are most of `check_format.c`.
- **`"{%d}"`** is treated as the C habit (E0316 with the fix `"{{{}}}"`) whenever the template has no
  real placeholder and its `%` verbs translate, instead of E0317 for the braces (contracts updated).
- **`{:5<}`** is reported as a fill character (Python reads `5` as the fill), not as options out of order.
- **Two more combinations are E0317**: `,` and a precision with `x`/`b` ("`x` cannot be combined with
  `,`."), as the contract's "Not allowed together" said; the message is in `contracts/diagnostics.md`.
- **`fmt_piece` has a `{}` fast path** (the `str` result directly) and builds numbers in a local buffer
  with one allocation: the first SC-004 measurement was 1.9× slower than `str` through a string builder.
- **The speed check is `tests/format_speed.py`** (Python, like `math_speed.py`), not a shell script, and
  times 5,000,000 iterations: at 1,000,000 the process start-up made the comparison noisy (1.05–1.17×
  for identical work).
- **`exact_fixed` has no 128-bit fast path** (research R3: only if SC-004 needed it; it did not). Marked
  `ponytail:` with the figure.
- **Catalog contract**: `contracts/codes.md` added and registered in `tests/catalog.py`, as earlier specs
  did.
- **Two existing checks needed updates for the new builtin**: `tests/doc_conformance.py` probes every
  builtin from its documented signature, which cannot express `args...` and a literal-only template;
  `format` joins `ok`, `err`, `todo` and `assume` in its skip list (its checks are the golden cases).
  `tests/gc/baseline.py` records one line per bench2 reference run; the 28 runs of f21–f24 were
  appended (the 703 earlier lines unchanged, checked).
- **Unused-variable warnings after `f"..."`**: the holes are not read (the literal is lexed as a plain
  string), so a variable used only there gets W0201 next to E0120. Warnings never fail a command, and
  the fix makes both go away.
