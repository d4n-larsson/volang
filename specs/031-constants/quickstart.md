# Quickstart: validating spec 031

Prerequisite: `make` (the build under test is `bin/vo`). Details: [contracts/](contracts/).

1. **Plain use.** The example of [contracts/syntax.md](contracts/syntax.md) checks clean and prints
   `100 0.25 out/report.txt true`; with `max_items` changed to `10` the last word is `false`
   (`tests/cases/const/use*`).
2. **Mistakes.** One file with a call in a value, a cycle, `9223372036854775807 + 1`, `10 / 0`, a list
   value, `MAX_ITEMS`, an unused private constant, `set` on a constant and `const` inside a function:
   `vo check --json` reports E0212, E0213, E0333 twice, E0332, E0207, W0202, E0208 and E0122 in one pass,
   each at its own position, and `tests/apply_fixes.py` resolves the three that carry a fix
   (`tests/cases/const/err_*`).
3. **Files.** `config.vo` exports two constants; `main.vo` imports one and runs; importing a private one
   is E0209 with the `export` fix; using one without importing is E0204 with the `import` fix
   (`tests/cases/const/files*`).
4. **Literal positions.** `uses file.write(out_dir)` with `write_file(out_dir + "report.txt", ...)`
   checks and runs; `vo effects` shows `file.write("out/")`; `write_file("tmp/x", ...)` is still E0503;
   `@size(1, max_name)` rejects a longer name in `from_json`; `format(row, a)` with `const row = "{} of
   {}";` is E0316 at the use, quoting the value (`tests/cases/const/scope*`, `annot*`, `template*`).
5. **Review.** With a recorded `vo.effects`, changing `out_dir` from `"out/"` to `"./"` makes `vo check`
   report E0504 for every function that uses it, with a note at the constant; replacing the literal
   `"out/"` by the constant in the first place changes nothing in the record.
6. **Tools.** `vo query refs`, `type-at` (with the value), `vo edit rename --dry-run`, `vo impact --diff`
   and `vo doc` on a two-file program give the outputs of [contracts/tools.md](contracts/tools.md)
   (`tests/cases/query/const*`).
7. **Formatting.** `const   max_items=100 ;` formats to `const max_items = 100;` and formats to itself
   (`tests/cases/fmt/const*`).
8. **Equivalence and records.** `python3 tests/const_equiv.py` (1,000 programs, constants against
   literals: 0 differences) and `python3 tests/const_records.py` (examples: output and `vo.effects`
   unchanged).
9. **Everything else unchanged.** `make test`; `python3 tests/gc/baseline.py` reports 731 of 731
   reference runs unchanged; `python3 tests/ref_examples.py` reports the reference under 15,000 tokens.
