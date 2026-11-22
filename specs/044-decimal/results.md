# Results: Decimal (spec 044)

Status: implemented. `make test` passed (run twice: the first run failed on the four recordings below). The gate was not run.

`std/decimal.vo` is written in vo: no builtin, opcode, syntax or diagnostic code was added. The C
changes are the module's name in `Makefile` and `src/doc.c`.

## SC-001: the functions agree with Python's `decimal`

`python3 tests/decimals.py` (groups `arith`, `round`, `text`): 10,000 random cases each for `add`,
`sub`, `mul`, `compare` with `equal`, `normalize`, `round_to`, `div`, `parse_decimal`, and
`decimal_text` with `grouped_text`; 2,000 each for `to_int` and `to_float`. Places 0 to 18; the
extreme units, exact ties and 19-digit divisors are over-represented; all six rounding modes. 0
differences. The cases whose exact result does not fit (units outside 64 bits or more than 18 places)
are run one per process, 200 per function: every one stops with R0001.

Two deliberate faults were put into `div` to see that the script notices (a wrong digit weight on the
path for 19-digit divisors; a wrong half test): each failed `round/div`, the second also
`round/round_to`. They were removed.

Golden cases: `run/decimal` (the acceptance scenarios and edge cases, printed), `run/decimal_overflow`,
`run/decimal_overflow_places`, `run/decimal_div_zero`, `run/decimal_bad_places`, `check/decimal_pure`,
`check/decimal_no_mode`.

## SC-002: allocation

`--group alloc`: 10,000 random amounts with part counts and ratio lists, valid and invalid: the parts
are those of the rule in contracts/decimal.md, they sum to the amount, and each is within one unit of
its exact share. `check/decimal_pure` holds the same as a property in vo.

## SC-003: speed

100,000 invoice lines (quantity × price, VAT rounded half-even to 2 places, running total): 80 ms on
x86-64 (best of 3; bound 1 s). Emulated, not bounded: 798 ms (ARM64), 454 ms (RISC-V).

## SC-004: the expenses example

Its output on its sample data is byte-identical to before; its 5 tests pass. Its own money handling
(`money` and `sum`) went from 16 lines to 10 (`amount_of` and `sum`); the file as a whole grew from 206
to 213 lines, because five imports were added. `vo.effects` was regenerated (it was already missing
two functions).

## SC-005: budget, docs, architectures

- Reference: 14,873 tokens of 15,000 (one library row).
- `tests/doc_conformance.py`: every example of the module runs. `tests/site_examples.py` and
  `make site-check` pass.
- `run` (116) and `check` (64) golden areas and `tests/decimals.py` pass with `build-aarch64/vo` and
  `build-riscv64/vo` under qemu (1,000 cases per function there).

## Existing golden cases changed

`tests/cases/doc/all.out` and `all_json.out`: the module listing gains `std.decimal`, and the
`std.option` line loses "(spec 032)" (see "Also in this change").

The recorded fingerprint (one line each) in `fs_steps/record_replay`, `http/record_replay`,
`https_steps/record_replay` and `tasks_steps/record_replay`: a recording's fingerprint covers the
source of the library modules the program imports, and the doc comments of `std.http`, `std.json` and
`std.process` lost their spec numbers. Nothing else in those files changed.

## Deviations

- **`add` and `sub` do more than the plan said**: R2 planned plain `int` arithmetic, which stops when
  scaling an operand overflows although the sum fits (-0.3 + 922337203685477581). They now add the
  head and the rest separately (`scaled_sum`), so only a result that does not fit stops.
- **`round_to` is `div` by one**: one code path for rounding instead of two.
- **Allocation skips parts with ratio 0** when handing out left-over units (0.05 by 0, 3, 7 is 0.00,
  0.02, 0.03), which the spec's rule did not say.
- **`total` adds in list order**: a partial sum that does not fit stops, even if the whole would fit,
  as a loop of `+` on ints does. FR-012 is not met for that case.
- **The test script is `tests/decimals.py`**, not `decimal.py`: that name shadows Python's module.
- **SC-004's "5 fewer lines"** holds for the money functions, not for the file (above).
- The example's `sum` adds a zero with two places so that an empty report prints `0.00`: the example's
  own property caught that `total([])` has no places.
- T011 has an extra case (`decimal_overflow_places`).

## Not verified

- The gate.
- Real ARM64 or RISC-V hardware (qemu only).
- Runtime errors inside the module are reported at a line of `std/decimal.vo` with the caller in the
  trace; the four `.err` files therefore change when lines of the module move.

## Also in this change (not part of the spec)

The documentation site no longer has a "Project" section: `tools/site.py` does not generate the
security model, the security review, the roadmap or the list of specifications, and the doc comments
shown on the site (five std modules, two examples) no longer cite spec numbers.
