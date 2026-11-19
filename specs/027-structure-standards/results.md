# Results: Application Structure Standards

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met (first half); second half follow-up | The voledger agent's single-file program (`main.vo`, 672 code lines) gets W0702 (672 code lines), W0701 (`main`: 28 statements that use no effect, with its three longest stretches) and W0703 (`main`: 51 statements). Its two CSV parsers are not reported as copies under the exact normalization (they differ in calls). A restructured version that passes its acceptance check comes from rerunning the experiment with this feature (SC-007). |
| SC-002 | met | Every example program checks with no structure warning (`tests/structure_corpus.py`); the reference's and site's runnable examples check with no warning at all (`tests/doc_examples.py` now requires it); std has none that is not accepted (three reasoned `@allow` in `std/json.vo`). Golden churn: the 11 `tests/cases/edit` step cases (one shared fixture with a test calling only `catalog.vo`: W0705) and 2 `tests/cases/spec` cases (E0116's message names `@allow`), 13 of about 1,100 golden programs (1.2%, limit 5%). |
| SC-003 | met | `tests/structure_corpus.py`: 25 seeded violations (5 per rule W0701–W0705), each reported; 12 well-structured programs (7 examples, 5 generated), none reported. |
| SC-004 | met | `tests/structure_speed.py` (`make gate`): the largest example and a generated, well-structured 5,153-line program (13 modules, 400 functions), fastest of 21 interleaved runs with and without the rules: within ±7%; cycle counts inside the binary (measured during implementation) put the rules at 7–10% of `vo check` on the generated program and about 5% on the example. |
| SC-005 | met | Reference +188 tokens (13,068 → 13,256 of 15,000). |
| SC-006 | met | Seven catalog entries (W0701–W0706, E0121) with examples and fixed versions, run by `tests/catalog.py` (131 sections, 0 problems); W0702 and W0703 examples use the `# (the next line, K times: ...)` form, which `tests/catalog.py` expands. |
| SC-007 | met | Sitegen rerun with this feature (2026-11-19, a new agent session, same spec): 5 files (`main.vo` 289 code lines, `site.vo` 382, `markdown.vo` 337, `template.vo` 278, `util.vo` 84) against one 1,050-line file before; effects only in `main.vo` (the four modules have no `uses`); 23 tests next to their code (9/5/4/3/2 by file); `vo check` 0 errors, 0 warnings, no `@allow`; acceptance 30/30 on the first complete attempt. |

## Verification

| Check | Result |
|---|---|
| `make test` (x86-64) | exit 0 (the first run found `tests/record_changes.py` treating W0705 on its fixtures as unexpected: it now ignores structure codes, which are about the fixtures' layout) |
| ARM64, RISC-V | `make ARCH=aarch64 test`: exit 0; RISC-V: running when this was committed |
| `make lint` | clean |
| `make instrumented` | 876 golden cases pass; `tests/structure_corpus.py` passes on `build-instrumented/vo` |
| collector stress | `tests/run.sh --stress 64 tests/cases/structure`: 31 of 31 |

## Thresholds

Measured in planning (research R1–R5) with an approximate analyzer; the exact counts from the
implementation agree in kind and differ in detail: std's `json.parse_string` and `json.parse_number` have
56 and 55 statements (estimated 50 and 53), so both carry `@allow`, as does `json.vo` itself (715 code
lines). R = 15, F = 400, S = 50, D = 6 unchanged.

## Deviations

- **Effect bit per statement** (T006): computed by the rules after checking (from each call's builtin id,
  declaration, generic bindings or function-value type), not recorded in `src/check_effects.c` while
  checking: a user function's effects are final only after the file's fixpoint.
- **Duplicates within one block are not reported** (research R5, found by the corpus check): a block that
  repeats a pattern (`let a = field(x, "a"); let b = field(x, "b"); ...`, a chain of similar `let`s) is not
  a copy. Copies in two branches of one `if` are still found.
- **Effect-only copies** compare literal values too (`fs.make_dir` and `fs.delete` differ in an operation
  code, not only in effects), and only functions whose plain signature matches one with effects are
  compared.
- **W0702 is accepted on the file's first `fn`, `test` or `prop`** (not its first declaration: `@allow` cannot
  stand above a `record`). Documentation lookup (`vo doc`, hover) now skips annotations between a doc comment
  and its function, and `tests/doc_conformance.py` likewise.
- **Checking std**: a std file cannot be checked on its own (its primitives exist only inside the
  library), so `tests/structure_corpus.py` checks a program importing every module with the maintainers'
  switch `VO_STRUCTURE_STD=1`; `tests/structure_speed.py` uses `VO_STRUCTURE_OFF=1`. Neither is documented
  for programs.
- **Seeds are generated** by `tests/structure_corpus.py` instead of files under `tests/structure_seeds/`.
- **Examples restructured** (FR-012): the tests of `expenses`, `helpdesk`, `notes-api` and `status-board`
  moved from `main.vo` into the modules they test (with their test helpers); `main.vo` keeps the effects;
  READMEs updated. `expenses`' `expect(false)` (W0603, older) became a check of the error text.
- **Performance work** (SC-004): normal forms are hashed without building strings (one output layer that
  hashes or appends), windows are prefiltered by per-statement shape hashes, strings are built only to
  confirm equal hashes, and stretches are recorded only for functions with effects.
