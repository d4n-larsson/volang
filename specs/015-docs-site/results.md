# Results: Documentation Site

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | **Met** | `tests/site_examples.py` (in `make test`): 40 pages (the hand-written pages and the example READMEs), 180 examples, 0 problems. Generated pages keep their existing checks: the reference (`ref_examples.py`, 34 examples), the catalog and `vo doc` (`doc_conformance`). Its self-test fails on a deliberately broken example (`tests/site_examples_fixtures/broken.md:6`). Output that the checker cannot run (stderr reports, `vo.effects` contents) is in `text` blocks, never `vo` blocks. |
| SC-002 | **Met** | 101 error pages, one per `##` entry of `docs/errors.md`; every code is checked against `vo explain` while the pages are assembled. 6 library pages from `vo doc --json`, one anchor per item. The reference is split from `docs/reference.md`. All are regenerated on every build, never edited by hand. |
| SC-003 | **Met** | `mkdocs build --strict` with link and anchor validation passes: 0 warnings, 175 pages. Three deliberate breaks each abort the build (below). |
| SC-004 | **Pending** | `tests/site_agents.py` (3 isolated runs given only Getting started) is ready. It is run only on request, like the other agent benchmarks. The install steps themselves pass `site_examples.py --start` in a fresh clone. |
| SC-005 | **Met** | `tests/site_search.py`: 36 lookups against the built search index, all 36 found in the first three (90% needed). |
| SC-006 | **Met locally** | `make site-check`, including the toolchain build, the venv and every check, takes 13 s. The 15-minute publish time can be measured only after Pages is enabled (Settings → Pages → Source: GitHub Actions). |
| SC-007 | **Met (contrast)** | The Nord color scheme, with Nord colors that fall below 4.5:1 replaced by the nearest passing shade of the same hue. `tests/site_contrast.py`: text, links, header, footer, code and all 14 syntax colors meet 4.5:1 in both palettes (40 pairs), 175 pages, 0 problems. Keyboard use relies on Material's navigation, search and skip link and was not tested separately. |

## Verification

| Check | Result |
|---|---|
| `python3 tests/site_examples.py` | 40 pages, 180 examples, 0 problems |
| `--coverage` | 20 reference sections, 16 chapters, 0 problems |
| `--options` | 14 options listed, 14 command pages, 0 problems |
| `--start` | 3 steps (clone and build, PATH, `make ARCH=aarch64`), 0 problems |
| `site_lexer.py` | 230 blocks, 0 with error tokens |
| `site_agents_files.py` | 175 pages in `llms.txt`, 175 Markdown copies, 0 problems |
| `site_search.py` | 36/36 |
| `site_contrast.py` | 175 pages, 0 problems |
| `ref_examples.py` | 34 examples, ~11,107 of 15,000 tokens, unchanged |
| `make gate` | passed: lint, test (including `site_examples.py`), UB traps, 1,000 programs against the assembly release (0 disagreements), collector stress, speed 0.989 |

## Deliberate breaks

Each was made in `build/site-src`, built with the strict configuration and then restored; every one exits 1 with a warning naming the source.

1. A link to a missing page (`guide/basics.md` → `nonexistent.md`).
2. A link to a missing anchor (`effects.md#no-such-anchor`).
3. A generated page removed (`errors/E0503.md`): the catalog index and `guide/effects.md` both report it.

The restored sources build with exit 0.

## Problems found while building the site (fixed)

- **The examples convention's ```` ```error CODE ```` fence did not render.** Python-Markdown's superfences reads a two-word info string as inline code, which then pairs every later fence wrongly and swallows headings. This affected the guides and the generated reference and catalog pages. `site/hooks.py` renders it as a titled `text` block; the Markdown copies for agents keep the original form.
- **Console sessions compared `vo test` timings literally** (`(0ms)` vs `(1ms)`); `tests/doc_examples.py` now normalizes them as it already did for `vo-test` blocks.
- **The lexer stayed in its `uses` state after an effect list in a function type** (`f: fn(int) -> int uses console, x: int`). Any token that cannot be part of the list now ends it.
- **`make site` did not reinstall a changed lexer**, because pip skips a local package with an unchanged version. It is now force-reinstalled.
- **Pages that open with a list had no summary in `llms.txt`**; the first item is used instead.
- **"Property tests" was not found by searching for it**; the chapter is now titled that way.

## Fixes the guides revealed

None applied: the toolchain and the embedded reference are unchanged (FR-018). The guides document what the toolchain does today. Where that differs from the reference they say so (for example, a `for` over a list sees elements pushed during the loop). The problems the writers found are listed under Follow-ups.

## Deviations

- The toolchain guide has 16 pages, not 15: `impact.md` was added (one per command).
- `vo-test` and `console` blocks normalize millisecond timings; JSON `"ms"` fields are not normalized, so `vo test --json` is not shown in checked sessions.

## Follow-ups

Problems found by the guide writers. Each has a minimal reproduction in their reports; none blocks the site.

**Behaviour that contradicts the reference or `vo explain`:**
1. W0504 ("no effects record") fires for a single-file program that imports from std; `vo explain W0504` says single-file programs get no warning. Because of it, std examples on the site cannot be bare `vo` blocks.
2. A duplicate field in `new P { x: 1, x: 2 }` is accepted (the last wins); the reference says each field is given exactly once.
3. W0201 misses a `var` that is set but never read.
4. `vo effects --update` does not print every change it writes: new pure entries are written silently, and the old scope is not printed when a scope widens. Text and JSON disagree on a narrowed scope (`-` vs `narrowed`).
5. A literal URL outside a `net` scope, passed through a library call, is not caught at check time (it is R0010 at run time). The reference says literal targets outside the scopes are E0503.
6. The E0306 fix for an empty list of a std type (`new list[Header]`) names a type that is not imported, giving E0201.
7. Unknown or inapplicable options and extra arguments are accepted silently (`vo check --seed 4`, `vo version foo`); `--remove` is missing from the unknown-option list; `vo --json frob` reports the unknown command `--json`.
8. When `main` returns `err`, `vo run --json` prints the error as text, with no JSON record.
9. An unwritable `--trace`/`--record` path is reported only after the program has run, with its effects.
10. `vo replay` of a recording without its program reports T0008 rather than T0002.
11. `vo test --trace` writes a pseudo-code `ASSUME` error record for each rejected property input.
12. `vo query type-at` on a called function's name gives the call's result type.
13. `vo package` accepts an entry file that does not exist.
14. W0502 and E0504 for package files print absolute paths; a removed pure function gives no W0502.
15. W0501 reports a false positive when an undefined function is called, and its fix leaves a trailing space.
16. A refused `vo edit --json` ends without a `summary` record.

**Messages:**
- E0208 calls a `for` variable "a parameter".
- `slice(xs, 2, 1)` reports an index out of bounds rather than a > b.
- Using a generic named function as a value gives a confusing E0301.
- Renaming a field onto an existing name gives E0305 "missing fields".
- `--mem-stats` peak values differ between text and JSON.

**Questions of design (for the user):**
- Should a `for` over a list iterate over a copy, as it does for the collections?
- Should a record that contains itself directly (`next: Node`) be rejected?
- Should a parameter be allowed to shadow a top-level function?
- Property inputs depend on the property's position among the file's tests. Float shrinking ends in very long numbers, and integer shrinking stops short of the boundary.
- A `vo run` killed by SIGTERM loses output it had already printed.

**Reference gaps:**
- The comparison operators and `min`/`max`/`sort` are described for int/float/str only, but they work on records of ordered fields.
- Both `case ok()` and `case ok { }` (and a case with a payload but no binding) are accepted and not normalized by `vo fmt`.
- The `test` record's `uses` and `spec` fields and the `spec_result` kind are missing from "Machine-readable output".
- Every command accepts every option.
