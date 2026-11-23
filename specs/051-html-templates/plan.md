# Implementation Plan: HTML Pages (`std.html`)

**Branch**: `051-html-templates` (work on `main`, as for earlier specs) | **Date**: 2026-11-23 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/051-html-templates/spec.md`

## Summary

`std.html` is a small vo module with one record, `Html`, and the functions around it. The work is in
the checker: a call of `std.html.html(template, values...)` is checked the way `format` is (the
template parsed from its source text, every diagnostic pointing inside it) and then marked as a
builtin call, which the compiler lowers to concatenation with one escape operation per hole (R1, R2).
The template is read by a small HTML tokenizer in C that knows only what the rules need: tags and
their nesting, void elements, attributes and quoting, raw-text elements, comments (R3). Escaping is
three functions of one builtin (R4). `Html` is opaque outside `std.html`: no `new`, no field access,
not `json` (R5). `std.web` gains `html_reply` and reads URL-encoded form bodies into the same request
form as query parameters, so the existing binding code does the rest (R6). The reference is paid for by
shortening other text (R8).

| Part | File | What |
|---|---|---|
| Library | `std/html.vo` (new) | `Html`, `html`, `html_text`, `no_html` |
| Templates | `src/check_html.c` (new) | the template tokenizer, the hole places, E0335–E0339, the pieces stored on the call |
| Wiring | `src/check_expr.c`, `src/compile.c`, `src/rt_call.c`, `src/vo.h`, VM and builtin tables, `Makefile` (`STD_FILES`), `src/doc.c` | the call turned into builtin `B_HTML`; `cformat`'s lowering reused with an escape per hole; `__html_escape` |
| Opaque `Html` | `src/check_expr.c` (new, field access, `set`), `src/check_generic.c` (`json_why`) | E0340 outside `std.html`; not `json` |
| std.web | `std/web.vo`, `src/json_derive.c` | `html_reply`; form bodies; `bool` from presence for form fields |
| Docs | `docs/reference.md`, `docs/errors.md`, `site/docs/guide/apis.md`, `site/docs/guide/pages.md` (new), roadmap, changelog, review row | R8, R9 |
| Example | `examples/notes-pages/` (new) | FR-013 |
| Tests | `tests/cases/html/`, `tests/cases/web/`, `tests/html_safety.py` (new), `tests/html_speed.py` (new) | R7 |

## Technical Context

**Language/Version**: C (C11, freestanding) for the checker and the escape builtin; vo for `std.html` and
`std.web`; Python 3 for test drivers.

**Primary Dependencies**: none new. The safety test uses Python's `html.parser` and, when installed,
`html5lib` (the gate requires it, as `https_interop.py` requires tlslite).

**Storage**: none.

**Testing**: golden cases in `tests/cases/html/` (check and run) and `tests/cases/web/`;
`tests/html_safety.py` (SC-002) in `make test`; `make quick AREA=html` between edits.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler toolchain with its standard library.

**Performance Goals**: SC-005, 1 MiB from 10,000 values under 50 ms: the lowering builds one string from
pieces, and the escape builtin is one pass over each value.

**Constraints**: reference ≤ 15,000 tokens with 1 left today (R8); no static buffer over 64 KiB; zero
warnings; the instrumented build passes.

**Scale/Scope**: about 500 lines in `src/check_html.c`, 80 of wiring, 60 of escaping, 120 of vo in
`std/html.vo` and `std/web.vo`, 40 cases, 400 lines of Python, one example.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | One way to build a page: an HTML literal with `format`'s holes; no second template language; control flow stays in vo. | Pass |
| II. Machine-readable feedback | Five new codes with spans inside the template and catalog entries; fixes where one correction is clear (a hole in an unquoted attribute gets the quotes). | Pass |
| III. Determinism | Rendering is pure; no effect. | Pass |
| IV. Test-first | Cases of each story first; the safety corpus before the escaping code. | Pass |
| V. Single source of truth, minimal surface | Reference must not grow: paid for by shortening (R8). Reuses `format`'s parsing of holes and lowering, and the query binding of `std.web`. | Pass, with R8 a task of its own |
| Implementation constraints | C and vo only; no vendored code; no assembly. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/051-html-templates/
├── plan.md
├── research.md          # R1–R9
├── data-model.md        # Html, hole places, pieces, the form part of a request
├── quickstart.md
├── contracts/
│   └── html.md          # std.html, the std.web additions, diagnostics E0335–E0340
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
std/html.vo                  # new
std/web.vo                   # html_reply, form bodies
src/check_html.c             # new
src/check_expr.c  src/compile.c  src/rt_call.c  src/vo.h  src/check_generic.c  src/json_derive.c  src/doc.c
examples/notes-pages/        # new: main.vo, pages.vo (templates), notes.vo (storage), README.md
tests/cases/html/            # new golden cases
tests/html_safety.py         # new (SC-002)
tests/html_speed.py          # new (SC-005)
site/docs/guide/pages.md     # new
```

**Structure Decision**: one new C file beside `check_format.c`, `check_regex.c` and `check_sqlite.c`,
which do the same for their literals.

## Complexity Tracking

No violations.
