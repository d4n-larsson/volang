# Quickstart: validating HTML pages

## 1. Between edits

```sh
make quick AREA=html       # tests/cases/html, tests/cases/web, html_safety.py --quick, catalog, fixes
```

## 2. A page and its safety

```sh
cd examples/notes-pages
../../bin/vo check main.vo           # 0 errors
../../bin/vo test main.vo            # the pages through handle_request: list, add by form, done as a fragment
../../bin/vo run main.vo             # http://localhost:8080/notes in a browser
```

In the browser: add a note whose text is `<script>alert(1)</script>`: it shows as text. Mark a note
done: only its item is replaced.

## 3. Mistakes caught by vo check

Each file in `tests/cases/html/` named `*_error.vo` shows one diagnostic of contracts/html.md with its
place inside the template; `python3 tests/apply_fixes.py` applies the fixes.

## 4. Once per change

```sh
make test                                 # includes html_safety.py (1,000 strings)
python3 tests/html_safety.py --full       # with html5lib (the gate)
python3 tests/html_speed.py               # SC-005
python3 tests/ref_examples.py             # at or under 15,000 tokens
```

Details: [contracts/html.md](contracts/html.md), [data-model.md](data-model.md), [research.md](research.md).
