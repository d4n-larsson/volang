# Quickstart: validating spec 027

Prerequisites: `make` (builds `build/vo`; `bin/vo` runs it).

## 1. A program in one file gets the structure warnings

```text
cp /home/dan/dev/priv/voledger/main.vo /tmp/ledger.vo   (the experiment's first version)
bin/vo check /tmp/ledger.vo
```

Expected: W0702 (more than 400 code lines) at line 1; W0701 at `main`, with related locations; W0704
for `parse_cardco` repeating `parse_nordbank`, if their bodies match. Exit 0.

## 2. Pure core

A `main` doing argument parsing and arithmetic between prints: W0701 at `main`. Move the logic into a pure
`fn plan(args: list[str]) -> result[Plan]`: no W0701, and `test "plan" { ... }` needs no `uses`.

## 3. Accepting a warning

```text
@allow("W0703", "one scanner, read top to bottom")
fn parse_number(...)
```

`vo check`: no W0703, summary `0 errors, 0 warnings (1 accepted)`. Remove the function's extra
statements: W0706 on the annotation, with a fix; `tests/apply_fixes.py` applies it.

## 4. Tests next to the code

`main.vo` imports `money.vo`; a test in `main.vo` calling only `money` functions: W0705. Move it to
`money.vo`: no warning, and `vo test main.vo` runs it (`money.vo::...` in the output).

## 5. Corpus and speed

```text
python3 tests/structure_corpus.py      # examples and doc examples clean, std only accepted, seeded found
python3 tests/structure_speed.py       # at most 10% slower (make gate)
tests/run.sh tests/cases/structure tests/cases/test
```

## 6. Docs

`bin/vo reference | grep -A8 "^## Structure"`; the site guide `site/docs/guide/structure.md` example
runs and checks clean (`tests/site_examples.py`); `python3 tests/ref_examples.py` within budget.
