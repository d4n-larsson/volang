# Results: Typed Errors

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | `make test` passes (golden cases, scripts, examples with their replays, `diff_engines.py` 1000/1000); `gc/baseline.py` 731/731 reference runs unchanged; the only changed expectations are listed under Deviations |
| SC-002 | met | the notes API's operations fail with `NoteError` and one `note_problem` gives every status; its 7 tests pass and replaying `recordings/demo.jsonl` gives byte-identical output before and after |
| SC-003 | met | no caller in the examples or the standard library decides by comparing error text: the expenses test compares `ExpenseError` values; `std.web`'s prefix form stays for apps that use it |
| SC-004 | met | E0334 and E0412 at check time; an unhandled error case is E0401 with its fix (`typed_errors/use_missing_case`, `web/typed_missing`) |
| SC-005 | met | `tests/perf.sh` within its limits (2,000 → 8,000 items 51 → 161 ms); ten million calls returning a case without a value 0.40 s against a fixed `str` message 0.39 s; a case carrying a number 0.44 s against a message built from it 0.67 s (medians of five) |
| SC-006 | met | reference ~14,545 tokens of 15,000 (+290), 40 examples run |
| SC-007 | deferred | needs isolated agent runs scheduled with the user |

## Verification

| Check | Result |
|---|---|
| `make test` (x86-64) | passed, in two parts: the run up to `scope_fuzz.py`, then (after the fixes below) every later step |
| ARM64, RISC-V (qemu) | `typed_errors`, `web`, `prop`, `tasks`, `const`, formatter cases: 133/133 each, byte-identical |
| `catalog.py`, `ref_examples.py`, `site_examples.py`, `doc_conformance.py` | passed |
| `make gate` | not run |

## Deviations

- **std.web's error type is per operation, not per app** (research R9): `typed(op, answer)` turns a typed
  operation into an ordinary one, so `App` and `Route` keep their types.
- **Inference limit found**: a generic function value two generic calls deep does not get its effect
  arguments inferred (`on_get(p, typed(op, f))` with `op` generic in its effects), with or without typed
  errors. The notes API writes the five operations' types out in annotated `let`s; a fix belongs in its
  own spec (noted in the roadmap).
- **`map_err` is not probed by `doc_conformance.py`**: its sample `ok(1)` has no type of its own; its
  golden cases cover it.
- **Changed expectations**: `doc/all*` (the `map_err` row), `web/errors` and `web/steps` (line numbers in
  `std/web.vo` traces).
- **Fixes to spec 031 found by this run**: `tests/scope_fuzz.py` hid environment names as `"A_N" + "O"`,
  now a constant expression checked by `vo check`; it reads them from a call. `examples/release-check`
  went back to `make_dir("out")`, which its recording holds.
