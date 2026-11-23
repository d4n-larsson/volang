# Results: HTML Pages (`std.html`)

## Probe of research R1

`html(...)` declared in `std/html.vo` and turned into builtin `B_HTML` by the checker (`src/check_html.c`,
hooked in `check_expr.c` before the argument count) compiles through `chtml` in `src/compile.c` and
renders under `vo run`, `vo test` and `vo replay`; `vo doc std.html.html` shows the declaration. Only
the compiler path lowers builtins of this kind (as for `format`), so no interpreter change was needed.

## Reference room

Before: `~14999 tokens of 15000`. After shortening (module rows of "Standard library and packages" cut to
purpose and effects; the `https`, `serve`, `serve_workers` and `std.web` bullets tightened; the
`explain` bullet dropped, as "Machine-readable output" says the same; `mcp` and `lsp` in one bullet;
`--mem-stats` shorter): `~14821 tokens`, 178 freed.

Then the new text (the `std.html` row, the template rules, forms and `html_reply` in the `std.web`
bullet): `~14998 tokens of 15000`.

## SC-001: every scenario a case, three architectures

`tests/cases/html/` (23 cases: rendering, every diagnostic, opacity, JSON exclusion, the page tests) and
`tests/cases/web/forms` pass, also `--repeat 20`, and the same 35 html and web cases pass on
`build-aarch64/vo` and `build-riscv64/vo`.

## SC-002: hostile strings

`python3 tests/html_safety.py`: 1,000 strings x 9 templates (text, both attribute quotes, a whole URL, a
URL after its start, `img src`/`alt`, `title`/`textarea`, a form's `action` and `value`, list items), 0
problems with `html.parser`. Also 200 strings on RISC-V. `html5lib` is not installed on this machine, so
the second parser was not run; the script uses it when present.

## SC-003: diagnostics

Every mistake of User Story 2 has a case with its code and place; `catalog.py` and `apply_fixes.py`
pass (the one fix, quoting an unquoted attribute hole, applies).

## SC-004: the example in a browser

`examples/notes-pages` served over HTTP (adding through the form, the list, done through the script's
request, an empty note refused, the script) answered as expected, and its recording replays; its five
tests pass. Not looked at in a browser here (no browser that runs beside the desktop one).

## SC-005: speed

`python3 tests/html_speed.py`: a page of 1,295,131 bytes from 10,000 items (30,000 values) in 7 ms,
median of 3 (target under 50 ms).

## SC-006: reference

At or under 15,000: `~14998`.

## SC-007: form binding

`tests/cases/web/forms` covers the binding rules for forms (types, limits as 422, unknown and repeated
fields, a name in form and query, checkboxes, other content types, a `body` field). Form twins of every
JSON binding case were not written: the rules are the query parameters', which the existing cases test.

## make test and the instrumented build

`make test` passes (x86-64). The first run stopped at `tests/cases/doc/all` and `all_json`, whose module
listings now include `std.html` (accepted; the expected change). The instrumented build (memory and
undefined-behavior checks) passes `tests/cases/html`, `web`, `json_derive` and `effects` (118 cases), the
safety corpus and the example's tests. The gate was not run.

## Differences from the plan

- URL attributes: a hole after a start of the URL that fixes the scheme (`/notes/{}`, `?q={}`,
  `mailto:{}`) is attribute text instead of an error; refusing it made the most common link an error.
  The spec, the contract and E0338's catalog entry say so.
- Inside `<script>` and `<style>` a single brace is the script's or style sheet's own (CSS rules work as
  written); `{}` there is still refused.
- `{0}`, `{name}`, `{:x}` are one E0336 each, with no fix.
- Forms reuse the query-parameter binding: `std.web`'s `bind` adds the form's fields to the query's
  (after checking each is a field of the input), and the binder treats a missing form `bool` as false.
- `tests/doc_conformance.py` makes `Html` sample values with `no_html()`.
