# Results: Readable Signatures and Named Effect Sets

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | After `vo fmt`, no `fn`, `test` or `prop` header line in `examples/`, `std/` or `bench/web/reference/` is over 100 characters (27 before: std.web 10, std.http 7, std.sqlite 4, std.process 2, the bench reference 3, notes-api 1). The only lines still over 100 are unbreakable string literals and function types written as one parameter (by design, FR-004). |
| SC-002 | met | notes-api declares `Storage` once (`api.vo`) and uses it in `open_notes` and `main`; the bench reference grew to 10 operations (list by sku, adjust quantity, count, rename, bulk delete added) and writes `file.read("data/"), file.write("data/")` once instead of on every operation (8 copies at 5 operations before); `bench/web/accept.py` still 29/29, the new operations checked by hand (201/200/404/422/204 answers). |
| SC-003 | met | `tests/effect_sets_records.py` (in `make test`): every clause repeated in two or more headers of a file is replaced by a set, in each example and 20 generated programs (44 sets): the same diagnostics, a byte-identical `vo.effects`, and the same test results or run output. notes-api's own `vo.effects` is unchanged by its `Storage` set. |
| SC-004 | deferred | Not run: like spec 029's SC-004 it needs isolated agent runs scheduled with the user. Command: `bench2/run_agent.sh --lang vo --only c01-line-count,...,c10-publish-scores` (the capability tasks) before and after, first-try results compared. |
| SC-005 | met | Every existing golden and step case passes unchanged except: `parse/export_misplaced` and `parse/export_twice` (E0110 now lists `effects` after `export`) and six cases whose `std/` trace positions moved when std was reformatted (`process/bad_args`, `bad_env_eq`, `bad_env_name`, `exact_text`, `web/errors`, `web/steps`) and 16 step cases for the same reason (traces, runtime errors, recording fingerprints); only numbers and hashes changed, checked mechanically file by file. |

## Verification

| Check | Result |
|---|---|
| `make test` (x86-64) | passed: golden 928/928, steps 255/255, every script (`effect_sets_records.py` included), examples, `diff_engines.py` 1000/1000 |
| `make ARCH=aarch64 test`, `make ARCH=riscv64 test` | not run yet (skipped for now at the user's request) |
| `make lint` | clean |
| Instrumented build (ASan, UBSan) | `effect_sets`, `fmt`, `effects`, `generic`: 116/116; step cases 3/3 |
| Stress build | `effect_sets`, `fmt`: 44/44; step cases 3/3 |
| Fuzzer | `fuzz.py --iterations 1000 --seed 30` with the new noise tokens: 0 failures |
| Reference | ~13,949 tokens of 15,000 (+124 for spec 030; budget 150) |

## Deviations

- **Comments between parameters** are a syntax error (E0110), as before, so the layout has none to keep;
  `sig_comments.vo` was dropped and the format contract says so.
- **Clause order**: `vo fmt` now sorts effect parameters with set names after the effects
  (`uses clock, E`), where before a clause with an effect parameter was printed as written. No file in
  std, the examples or the golden cases changed by it.
- **`Store[Storage]`** (a set as an effect argument without `uses`) is E0327 with the fix inserting
  `uses `, like an effect written without it.
- **An anonymous function's parameters** break like a call's arguments (the statement rule), and a call
  whose last argument is an anonymous function is broken at its arguments rather than inside the
  function's parameter list, so existing layouts stay as they were.
- **References to a set** in `vo query refs` are labeled `use` (as a record literal is), not `type`.
- **A set is "used"** when a clause, a type or another set names it (W0505), so a set used only by an
  unused set is reported only after that one is removed.
- **The reference's error-code section** lists ranges, not codes, so E0513 and W0505 appear only in the
  Effects paragraph.
