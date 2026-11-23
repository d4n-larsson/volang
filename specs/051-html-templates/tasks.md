---

description: "Task list for vo-lang spec 051: HTML Pages (std.html)"
---

# Tasks: HTML Pages (`std.html`)

**Input**: Design documents from `specs/051-html-templates/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R9), data-model.md, contracts/html.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's golden cases land first and fail before its code.

**Organization**:
- Setup: the module and builtin wiring, a probe of the call-as-builtin hook (research R1), the results
  file, and room in the reference (R8), which is done before any new reference text.
- Foundational: the template tokenizer and the escape builtin.
- US1 (P1) rendering, escaped; US2 (P1) template diagnostics; US3 (P1) `html_reply` and forms;
  US4 (P2) fragments; US5 (P2) tests without a browser.
- Polish: the example, the safety corpus, speed, docs, three architectures, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Everything a program sees** (items, places, codes, headers, form rules): contracts/html.md. Hole
  places and the request form: data-model.md.
- **Golden cases**: `tests/cases/html/NAME.vo` with `.cmd` (`check --json {}` for diagnostics; absent
  for `run`), `.code`, `.out`/`.err`; std.web cases in `tests/cases/web/` likewise. Write the case,
  run `tests/run.sh tests/cases/html` and see it fail, then write the code; `tests/run.sh --accept`
  only after reading the actual output.
- **Codes**: E0335–E0340 exactly as contracts/html.md names them; each gets a `docs/errors.md` entry
  with an example and a golden case (constitution IV).
- **Reference**: `python3 tests/ref_examples.py` must report at most 15,000 tokens after every edit of
  `docs/reference.md`.
- **C code**: zero warnings (`make`), no static array over 64 KiB, `ponytail:` comments on known
  ceilings.
- **Between edits**: `make quick AREA=html` (T002); `make test` once per change via `tools/progress.py
  run test`; the gate only when asked. Never two suites at once.

## Phase 1: Setup

- [X] T001 Create `std/html.vo` with the header comment of a library module, `export record Html { text: str; }` with its doc comment, `export fn html(template: str) -> Html` (body: `return new Html { text: "" };`, never run: research R1), `export fn html_text(h: Html) -> str` and `export fn no_html() -> Html`, each with a doc comment and an example that needs no hole; add `std/html.vo` to `STD_FILES` in `Makefile` and `"html"` to `doc_modules` in `src/doc.c`
- [X] T002 Add area `html` to `tools/quick.sh` (case dirs `html web json_derive doc`; scripts `catalog.py apply_fixes.py ref_examples.py doc_conformance.py "html_safety.py --quick"`; files `src/check_html.c`, `std/html.vo`, `tests/html_*.py`); create `specs/051-html-templates/results.md` with headings SC-001 to SC-007 and "Reference room"
- [X] T003 Add builtin ids after `B_LOCAL_TZ` in `src/vo.h`: `B_HTML (118)` (a checked `html(...)` call, lowered by the compiler) and `B_HTML_ESCAPE (119)` (`__html_escape(place, s) -> str`); move `B_LAST` to 119; add `"html"`-call handling placeholders and the name `"__html_escape"` to the builtin name table in `src/check_expr.c`, `BU_HTML, BU_HTML_ESCAPE` to the enum and `_Static_assert` in `src/rt_call.c`, and their entries wherever `B_LOCAL_TZ` has one (grep `B_LOCAL_TZ`, `BU_LOCAL_TZ`, `__local_tz`); `B_HTML_ESCAPE` has no effect
- [X] T004 Probe research R1 before building on it: in `src/check_expr.c`, where a call's declaration is a standard-library function (before `check_argc`), send a call of `std/html.vo`'s `html` to a stub `check_html_call(call)` in a new `src/check_html.c` that accepts any number of arguments, types them, and sets `call->d.i = B_HTML`; in `src/compile.c` lower `B_HTML` like `B_FORMAT` with every hole appended unescaped; check that `import std.html.html; ... print(html_text(html("<b>{}</b>", "x")));` prints `<b>x</b>` under `vo run`, `vo test` and `vo replay`, and that `vo doc std.html.html` and hover still show the declaration. If the interpreter path (`src/interp.c`/`rt_call.c` builtin dispatch) also needs `BU_HTML`, add it the way `BU_FORMAT` is handled. Record the outcome in results.md
- [X] T005 Make room in the reference (research R8): shorten `docs/reference.md` by at least 160 tokens without losing a rule: the module rows of "Standard library and packages" cut to what each module is for and its effects (`vo doc` lists the items), and repeated explanations merged; run `python3 tests/ref_examples.py` and `python3 tests/doc_conformance.py`; record the counts before and after in results.md under "Reference room"

## Phase 2: Foundational

- [X] T006 In `src/check_html.c`, the template tokenizer of research R3 and data-model.md over the decoded template (reuse `check_format.c`'s decoding of the literal and its source offsets so spans point inside the template): states text, tag, attribute name, value, quoted value, unquoted value, comment, doctype (start only), raw text (`script`, `style`), escapable raw text (`textarea`, `title`); a stack of open elements; void elements `area base br col embed hr img input link meta source track wbr` never pushed; names compared in ASCII lower case. Output: the literal pieces and, per hole, its place (`text`, `attribute`, `url`) or the reason it is refused
- [X] T007 [P] In `src/rt_call.c` (or a new `src/html.c` if it exceeds 60 lines), `__html_escape(place, s)` of research R4: text and attribute escape `&` `<` `>` `"` `'` as `&amp;` `&lt;` `&gt;` `&quot;` `&#39;`; URL: strip leading ASCII whitespace and control bytes, take the scheme (text before the first `:` when it comes before any `/`, `?`, `#`), replace the value with `#` unless there is no scheme or it is `http`, `https` or `mailto` (ASCII case-insensitive), then escape as an attribute; one pass, output in a string builder

## Phase 3: User Story 1 - A page from a template, escaped (P1) 🎯 MVP

**Goal**: `html(...)` renders with every value escaped for its place, and pages compose.

**Independent Test**: `tests/run.sh tests/cases/html` (run cases) passes.

- [X] T008 [P] [US1] Golden run cases in `tests/cases/html/`: `text_escape` (scenario 1, byte for byte), `attribute_escape` (scenario 2, values with `"` `'` `<` `>` `&`), `url_schemes` (scenario 3: `javascript:`, `JaVaScRiPt:`, ` javascript:`, `data:`, `vbscript:`, `ftp:` become `#`; `/notes/1`, `?q=1`, `#top`, `https://x`, `mailto:a@b` kept and escaped), `compose` (scenario 4: an `Html` value inserted as it is), `list_of_html` (scenario 5, also an empty list), `scalars` (scenario 6: `int`, `float`, `bool` as `str(x)`), `braces` (`{{` and `}}`), `textarea_title` (holes there are text)
- [X] T009 [US1] In `src/check_html.c`, complete `check_html_call`: check each value against its hole's place (`str`, `int`, `float`, `bool` everywhere allowed; `Html` and `list[Html]` in text holes only; `str` only in URL holes) and store the pieces with place and value kind on the call (`call->e.p`, a vec like `format`'s `FmtPiece`)
- [X] T010 [US1] In `src/compile.c`, lower `B_HTML` (research R2): literal pieces appended; `str` values through `B_HTML_ESCAPE` with the place; `int`/`float`/`bool` converted with the `str` conversion first; `Html` values' `text` appended; `list[Html]` appended element by element; the result wrapped as an `Html` record. T008 passes

## Phase 4: User Story 2 - Template mistakes are caught before the program runs (P1)

**Goal**: every mistake of the story is a diagnostic at its place inside the template.

**Independent Test**: `tests/run.sh tests/cases/html` (check cases), `python3 tests/catalog.py`, `python3 tests/apply_fixes.py`.

- [X] T011 [P] [US2] Golden check cases in `tests/cases/html/`, one diagnostic each, with JSON output: `not_literal_error` (E0335: a template built with `+`), `hole_option_error` (E0336: `{:x}`, and `{0}`/`{name}` with the fix to `{}`), `count_error` (E0336: two holes, one and three values), `nesting_error` (E0337: `<p><b></p>`), `open_error` (E0337: `<ul><li>a</ul>`), `void_end_error` (E0337: `</br>`), `doctype_error` (E0337: doctype not at the start), `unsafe_script_error`, `unsafe_style_error`, `unsafe_onclick_error`, `unsafe_style_attr_error`, `unsafe_comment_error`, `unsafe_name_error`, `unsafe_partial_url_error` (E0338, each naming its place), `unquoted_error` (E0338 with the fix that adds `"`), `markup_in_attribute_error` (E0339: `Html` and `list[Html]` in an attribute, an `int` in a URL hole), `bad_type_error` (E0301: a record value)
- [X] T012 [US2] In `src/check_html.c`, report E0335–E0339 with spans inside the template (one pass, every independent error, no cascades after a nesting error: constitution II); the E0336 fix as `check_format.c` does for `{0}`; the E0338 fix for an unquoted attribute hole; messages as in contracts/html.md
- [X] T013 [P] [US2] Golden check cases `tests/cases/html/opaque_new_error`, `opaque_field_error`, `opaque_set_error` (E0340 outside `std.html`) and `not_json_error` (E0328 for `to_json` of a record holding an `Html`)
- [X] T014 [US2] E0340 in `src/check_expr.c` for `new Html`, field reads and `set` of a field of type `Html` outside `std/html.vo`; in `src/check_generic.c`'s `json_why`, `Html` answers "markup cannot be read from or written as JSON"
- [X] T015 [US2] Catalog entries E0335–E0340 in `docs/errors.md` (meaning, example, fix) per contracts/html.md; `python3 tests/catalog.py` and `python3 tests/apply_fixes.py` pass

## Phase 5: User Story 3 - Pages and forms in std.web (P1)

**Goal**: answer pages; bind HTML forms into input records.

**Independent Test**: `tests/run.sh tests/cases/web` and `python3 tests/web_fuzz.py --quick`.

- [X] T016 [P] [US3] Golden cases in `tests/cases/web/`: `html_reply` (status, body, the three headers exactly as contracts/html.md), `form_binding` (`text=buy+milk%21` → `"buy milk!"`; `int`, `float`, `bool` fields; a checkbox absent → `false`, `on` → `true`; field annotations `@size` giving 422), `form_errors` (a non-number for an `int` field 400 naming it; an unknown field 400; a field twice 400; a name in both form and query 400; invalid UTF-8 after decoding 400), `form_content_types` (multipart 415; `text/plain` 415; a record with a `body` field and a form body 415), `form_get` (a `GET` form's query string binds as today)
- [X] T017 [US3] In `std/web.vo`: `export fn html_reply(status: int, page: Html) -> Response` with the headers of contracts/html.md and a doc comment with an example; in `bind`, decode an `application/x-www-form-urlencoded` body into the request form's `form` part (refactor `query_values` to take the text), keep JSON as is, and answer other content types 415 as now
- [X] T018 [US3] In `src/json_derive.c`'s `web_record`: look a field up in path, then headers, then `form`, then query; a name in both `form` and query is a 400 problem naming it; for fields read from `form`, a missing `bool` is `false` and `on`/`true` are `true` (other texts the usual 400). T016 passes; `tests/run.sh tests/cases/web tests/cases/json_derive` passes unchanged otherwise
- [X] T019 [P] [US3] SC-007: for each JSON binding case in `tests/cases/web/binding.vo` and `tests/cases/json_derive/`, add its form twin where form encoding can express it (flat records of scalars) in `tests/cases/web/form_twins.vo`

## Phase 6: User Story 4 - Fragments that a page swaps in (P2)

**Independent Test**: `tests/run.sh tests/cases/html/fragment`.

- [X] T020 [US4] Golden case `tests/cases/html/fragment.vo`: one item template used by a list page and alone; the item's markup is byte-identical in both; `html_reply` of the fragment has the same headers and exactly the fragment as body

## Phase 7: User Story 5 - Tested without a browser (P2)

**Independent Test**: `vo test` on the case and on the example.

- [X] T021 [US5] Golden test case `tests/cases/html/page_test.vo` (`.cmd` `test {}`): a small `web_app` with a page route, tests through `handle_request` checking the status and that `html_text` of the page contains a hostile note escaped; run 20 times with `tests/run.sh --repeat 20 tests/cases/html`

## Phase 8: Polish

- [X] T022 Create `examples/notes-pages/` (FR-013): `main.vo` (serve with `serve_app` on `serve("8080")`, SQLite in `data/` as notes-api does), `notes.vo` (storage, as in notes-api), `pages.vo` (the layout, list, item and form templates; routes: `GET /notes` page, `POST /notes` from a form then redirect 303 to `/notes`, `POST /notes/{id}/done` answering the updated item as a fragment), tests through `handle_request` for each route, `README.md` in the examples' format, `vo.effects`, and a recording in `recordings/` made with `vo run --record` and the requests of its README
- [X] T023 [P] Create `tests/html_safety.py` (SC-002): build at least 1,000 hostile strings from parts (quotes, `<`, `>`, `&`, entities like `&lt;` and `&#x6a;`, `javascript:`/`data:`/`vbscript:` with mixed case, leading spaces, tabs, newlines and control bytes, ` `, long runs), render each into every place of a fixed set of templates with one vo program (values from stdin, one per line), parse each output with `html.parser` and, when installed, `html5lib`; fail unless the elements and attributes equal the template's and every URL attribute's scheme is none, `http`, `https` or `mailto`. `--quick` uses 200 strings; add `"html_safety.py --quick"` to the `test` target's script list in `Makefile`
- [X] T024 [P] Create `tests/html_speed.py` (SC-005): render a 1 MiB page from 10,000 values (median of 3 runs, native build) under 50 ms; print the time; record it in results.md
- [X] T025 Write the reference text (FR-012) in the room made by T005: the `std.html` row, the template rules (holes, places, refused places, composition) beside Formatting, forms and `html_reply` in the `std.web` bullet; `python3 tests/ref_examples.py` at or under 15,000 tokens; record the count
- [X] T026 [P] Create `site/docs/guide/pages.md` (templates, places, composition, a layout function, forms, fragments, testing) with checked examples, add `"pages"` after `"apis"` to `GUIDE` in `tools/site.py`, link it from `site/docs/guide/apis.md`; `python3 tests/site_examples.py` and `make site-check` pass
- [X] T027 [P] Run `VO_BIN=$PWD/build-aarch64/vo tests/run.sh tests/cases/html tests/cases/web` and the same with `build-riscv64/vo` after `make ARCH=aarch64` and `make ARCH=riscv64` (SC-001); record in results.md
- [X] T028 Run `make test` once; run `make instrumented`'s suite on `tests/cases/html tests/cases/web` (new C code); fill results.md; update `docs/roadmap.md`, `CHANGELOG.md`, the Templates row of `docs/review-2026-11/ecosystem.md`, and the spec's status line

## Dependencies

- T001 → T003 → T004 (the probe) → everything that builds on R1. T005 may run any time before T025.
- T006, T007 → US1 (T008–T010) → US2 (T011–T015; T013–T014 only need T001).
- US3 (T016–T019) needs T001 and T007 only (`html_reply` takes an `Html`), so it can follow T004 in
  parallel with US1 and US2 by file.
- US4 and US5 need US1 and US3. Polish needs all.

## Parallel examples

- T006 and T007 (different files).
- T008, T011, T013, T016 (case files) while the code is written.
- T023, T024, T026, T027 at the end.

## Implementation strategy

1. Setup with the probe first: if research R1 does not hold, stop and revise the plan before the
   tokenizer is written.
2. US1 + US2: the module is usable and safe; that is the MVP.
3. US3: pages and forms in `std.web`.
4. US4, US5, then the example and the safety corpus.
