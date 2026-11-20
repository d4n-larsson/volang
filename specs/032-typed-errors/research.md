# Research: Typed Errors (spec 032)

Starting point (read from the code): a result type is `TY_RESULT` with A the `ok` type and B unused,
built by `ty_result(inner)` (`src/check.c`) and parsed from `result[T]` (`parse_type`, `src/parse.c`).
Every consumer that needs the error type gets it from one function, `case_payload(type, 1)`, which
returns `t_str`: `match` bindings (`src/check.c`), rendering (`rt_render.c`), task results (`task.c`),
generated inputs and shrinking (`prop.c`), the first difference of a failing `expect_eq` (`rt_test.c`).
`try` is checked in `check_try` (`src/check_expr.c`), which returns the `ok` type and never compares
error types. `ok`/`err` take their result type from the expected type (`check_builtin`, E0307 when it is
unknown). Generic inference and substitution (`types.c`, `check_generic.c`) recurse into A only.
`std.web` turns `err("kind: detail")` into a status in `error_answer`. Reference: about 14,255 tokens.
Codes in use: E0401–E0411, E0301–E0333.

## R1 The type: `result[T, E]`, B holding the error type, 0 for `str`

**Decision**: `TY_RESULT` keeps A as the `ok` type and uses B for the error type, with B = 0 meaning
`str`. `result[T, str]` is canonicalized to B = 0 when it is built, so `result[T]` and `result[T, str]`
are the same interned type (FR-001). A new `ty_result_e(T, E)` builds it; `ty_result(T)` is unchanged.
`err_type(t)` returns B or `t_str`; `case_payload(t, 1)` returns `err_type(t)`, which carries the error
type to every consumer listed above at once.

**Rationale**: with B = 0 for `str`, every existing type is interned, named, unified and substituted
exactly as before (FR-003, SC-001): no existing program can see a difference, and the change to each
consumer is "also look at B". Naming prints `result[T]` for B = 0 and `result[T, E]` otherwise.

**Alternatives**: B = `t_str` always (every interned result type changes; every place that builds a
result must pass it); a separate type kind for typed results (two kinds to handle everywhere).

## R2 Syntax and formatting

**Decision**: `parse_type` accepts an optional `, TYPE` after the `ok` type of `result[...]` (into the
type node's C). `vo fmt` prints it as written: `result[T, str]` stays (the guide recommends `result[T]`);
no other spelling exists.

## R3 Valid error types

**Decision**: any type but `void`, a function type or a type containing one (`ty_has_func`). Otherwise
E0334 `bad_error_type` at the error type, no fix.

## R4 `ok`, `err`, `match`

**Decision**: no new rule. `err(e)` checks `e` against `err_type(expected)` (E0301 for a `str` given to a
typed result, with both types in the message); `ok(v)` as today; `case err(e)` binds `err_type`. E0307
stays for an unknown result type.

## R5 `try` across error types

**Decision**: `check_try` compares `err_type` of the operand's type with `err_type(ck_ret)`; when they
differ it is E0412 `try_error_mismatch` at the `try`: "`try` passes the error on, but this `result` fails
with `ParseError` and `load` fails with `LoadError`: convert it with `map_err(..., fn(e: ParseError) ->
LoadError { ... })`." No fix (the conversion is the program's decision). The rule covers `main`
(`result[void]`, a `str` error) with no special case (FR-007).

**Alternatives**: allowing `try` from any typed error into a `str` function by rendering the error (an
implicit conversion, FR-008); `try e else f` (a second form).

## R6 `map_err`

**Decision**: a builtin `map_err(r: result[T, E1], f: fn(E1) -> E2) -> result[T, E2]`, checked with the
higher-order builtins (`check_hof`): the function's effects become the caller's, generic arguments are
inferred from `r`. At run time an `ok` is returned as it is (the same object: values have no identity);
an `err` becomes `err(f(e))`. Builtins are not values, as today. It goes in the reference's table of
functions-as-values builtins.

## R7 Generics

**Decision**: inference (`ty_unify`), substitution (`ty_subst`), `ty_has_params`, the `json` and
`ordered` constraints, task-result checks and effect-variable scans recurse into B as they do into A;
unifying B = 0 with a type parameter binds it to `str`.

## R8 Reports

**Decision**: rendering, task transfer, generation and shrinking, and the first difference already go
through `case_payload`, so typed errors are shown, transferred, generated and shrunk as the payloads of
variants are (FR-009, FR-010). Traces and runtime-error locals use the same rendering. Recordings hold
results of the outside world, which stay `str` (FR-011): nothing changes there.

## R9 std.web

**Decision**: a typed operation is turned into an ordinary one by a library function, so `Route`, `App`
and the routing code keep their types:

```text
export record Problem { status: int; detail: str; }
export fn typed[C, I: json, X, E: effects](op: fn(C, I) -> result[Response, X] uses E,
                                          answer: fn(X) -> Problem) -> fn(C, I) -> result[Response] uses E
```

`on_get("/notes/{id}", typed(show, note_problem))`. An `err(x)` becomes the RFC 9457 answer of
`answer(x)`; the mapping is one function with a `match` over the error variant, so a missing case is
E0401 with its fix (the checked table of FR-012). The prefix form and `App.errors` are untouched.

**Deviation from the spec (US4 acceptance 4)**: the error type is per operation, not per app; an app
normally passes the same mapping to every `typed` call. One type per app would need a type parameter on
`App` and `Route`, changing every existing app's types for no gain in checking.

## R10 std.option

**Decision**: `std/option.vo`: `export variant Option[T] { some(T); none; }`, `or_else(o, default) -> T`,
`is_some(o) -> bool`, `to_result[T, E](o, e: E) -> result[T, E]`, each with an example.

## R11 Diagnostics

| Code | Name | Fix |
|---|---|---|
| E0334 | `bad_error_type` | none |
| E0412 | `try_error_mismatch` | none |

Reused: E0301 (an error value of the wrong type), E0307, E0401, E0406, E0409 (a `main` returning a typed
result: its message lists the two forms).

## R12 Documentation and examples

**Decision**: the reference's "Results and errors" section gets a paragraph and one runnable example
(about +250 tokens); `map_err` joins the functions-as-values table. The guide's results chapter gets a
section on typed errors and when to use them; `std.web`'s page shows `typed`. The notes API moves its
error kinds to `NoteError` with one `note_problem` mapping; the expenses example's `expenses_from`
returns a typed error that its caller matches.

## R13 Tests

Golden cases in `tests/cases/typed_errors/` (one per scenario and code, written first and failing),
`tests/cases/web/typed*`, `tests/cases/prop/typed*`, `tests/cases/tasks/typed*`; the existing suites give
SC-001; a speed comparison of a loop returning typed and `str` errors for SC-005.
