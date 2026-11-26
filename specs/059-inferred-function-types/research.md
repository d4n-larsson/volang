# Research: Anonymous Functions Take Their Types From Context

Decisions for spec 059, from reading `src/lex.c`, `src/parse.c`, `src/check.c`, `src/check_expr.c`,
`src/check_func.c`, `src/check_generic.c`, `src/types.c`, `src/compile.c`, `src/vm.c`, `src/fmt.c`,
`src/diag.c`, `src/query.c`, `src/main.c` and the corpora at commit `1e2e375`.

## R1. Syntax: names only, and `=>` as a new token

**Decision**: an anonymous function's parameters are names (`fn(a, b)`); its body is a block or `=> expr`.
`=>` is a new token (`T_FAT_ARROW`), lexed where `=` is today (`lex.c:497`); today `=>` lexes as `=` `>`
and appears nowhere. The parser still accepts `: type` on a parameter and `-> T` after the list (as
`parse_fn_kind(N_LAMBDA)` does today, `parse.c:1206-1252`), so the checker can report them with a fix
(R5); it never reports them itself.

**Rationale**: in expression position `fn` is always a lambda (`parse.c:765`); function *types* are parsed
only by `parse_type` in type positions (`parse.c:542-555`), so names-only parameters and `=>` are
unambiguous. `fn` at statement start stays a parse error (`parse.c:1148`), so "a lambda as a statement"
cannot occur. `vo fmt` refuses any file with a parse error (`main.c:701`); if written types were a parse
error, fmt could not format the code it is meant to migrate.

**Alternatives**: making the types a parse error (fmt would refuse 0.4 files); `->` or `:` instead of `=>`
(`->` already means "returns" in the same header, `:` means "has type").

## R2. The expected type reaches the lambda

**Decision**: `check_lambda(node, expected, open)` replaces `check_lambda(node)`; `check_expr`'s
`case N_LAMBDA` passes its `expected` (`check_expr.c:1278`). Parameter types come from `expected`'s
parameters; the return type from `expected`'s return unless `open` says it is inferred (R4). `p->type` and
`node->type` are set before the parameters are declared, so hover, `type-at`, traces and runtime frames
show them with no change (they read the resolved `p->type`: `query.c:79-95`, `vm.c:897-920`,
`vm.c:1151-1170`).

Positions and what changes:

| Position | Today | Change |
|---|---|---|
| typed `let`/`var`, `return`, `set`, `new` field, variant payload, `ok`/`err`, call of a non-generic function or a function value | `expected` passed (`check.c:592`, `:764`, `:619`; `check_expr.c:1131`, `:1236`, `:481`, `:588`; `check_func.c:179`) | none beyond R2's pass-through |
| list literal | later elements get the first element's type (`check_expr.c:1076`) | every element gets `expected`'s element type when there is one |
| `map`, `filter`, `sort_by`, `fold`, `any`, `all`, `concurrent_map` | function argument checked with 0 (`check_func.c:300-360`) | R3 |
| `map_err` | 0 (`check_func.c:270`) | `fn(E1) -> open`, E1 from the result argument |
| user generic call | function-typed parameters mentioning a type variable get 0 (`check_generic.c:598-602`) | R3 |
| generic variant payload | 0 (`check_generic.c:672`) | substituted payload type, as R3 |
| std primitives `__serve_workers`, `__serve_call`, `__worker`, `__coll` | 0 (`check_expr.c:337-371`) | their callers in `std/http.vo` (649, 692) pass named functions or typed bindings (R9) |

**Alternatives**: global inference across statements (rejected by the spec's assumption of local inference).

## R3. Order of inference in generic calls: non-lambda arguments first

**Decision**: in a generic call, first every argument that is not an anonymous function is checked and
unified as today (including function *values* such as `setup` in `std/web.vo:786`); then each anonymous
function, left to right, is checked against the parameter type with the bindings so far substituted. A
parameter type of the lambda that still mentions an open type variable of the callee
(`ty_mentions_owner`, `check_generic.c:524`) is E0343 naming the variable; an open *return* type is
inferred from the body (R4) and unified back, fixing that variable for later lambdas. For the builtins in
`check_hof` the same order holds by construction: `T` from the list, `A` from `init`, then the lambda with
`U`/`C` open (map, sort_by, concurrent_map) or fixed (`bool`, `A`).

**Spec correction**: the spec's Edge Cases and FR-002 say "non-function arguments first"; that would leave
`C` open in `serve_workers(address, workers, setup, fn(ctx, r) ...)` where `setup` is a named function
value. The rule is "non-lambda arguments first"; spec.md is amended accordingly.

**Rationale**: `ty_unify` (`types.c:336-406`) is one-directional from the callee's pattern to a concrete
type and already handles function types (arity, params, return, effect parameter), and
`ty_subst_bindings` leaves unbound variables as `TY_PARAM`, so "params known, return open" is
representable without a new type kind. The caller's own type parameters (`C` inside a generic std
function) are `TY_PARAM`s of a different owner and count as known.

**Alternatives**: a placeholder type kind for "unknown" (more code paths in every type function);
bidirectional unification (YAGNI for local inference).

## R4. Return type from the body

**Decision**: when the expected return type is open, `check_lambda` checks a `=> expr` body with no
expected type and takes its type; for a block body, `ck_ret` is set to a sentinel meaning "collect":
`check_return` takes the first `return expr;` type as the return type and checks later ones against it
(E0301 at the second differing one); a body with no value-returning `return` gives `void`. `ck_ret == 0`
keeps its meaning ("no value", tests: `check.c:766`).

**Alternatives**: joining types across returns (no implicit conversions exist to join).

## R5. Written types: E0342, with one-edit fixes

**Decision**: a parameter type or `-> T` written on an anonymous function is **E0342
lambda_types_written** (checker). When the lambda has an expected type, its written types are checked
against it first: a difference is today's mismatch (E0301 or E0502, with their fixes) and E0342 is not
reported for that lambda in that round (FR-005: fixes never change meaning). When they are equal, E0342's
fix is one edit replacing the header from `(` to the end of the return type with `(a, b)`. Without an
expected type and when the lambda is the whole initializer of an untyped `let`/`var`, the fix is one edit
from the end of the binding's name to the end of the header: `f: fn(int) -> int = fn(x)`, the type printed
by `ty_name_in` with the lambda's inferred effects. Otherwise there is no fix.

**Rationale**: one edit per fix (schema 1; `tools/vo_fix.vo:90` applies `fixes[0]`; spec 058 R6). The
header and the body (R6) are separate, non-touching spans, so both fixes apply in one round.

## R6. Single-return bodies: E0413, and `vo fmt` rewrites them

**Decision**: a block body whose only statement is `return expr;` (no comments inside) is **E0413
lambda_single_return** (checker), with a fix replacing `{ ... }` by `=> expr`. `vo fmt` does the same
rewrite syntactically (`fm_lambda`, `fmt.c:171`), since fmt only parses (`main.c:690-724`) and the rewrite
needs no types. `=> expr` prints on one line when it fits the 100-column rule, else breaks inside the
expression as any expression does.

**Alternatives**: a warning (two forms would stay valid, against Principle I); a parser error (fmt would
refuse the files).

## R7. Lambdas with no expected type: E0343

**Decision**: **E0343 lambda_type_unknown**, no fix: "Cannot tell the parameter types of this anonymous
function: give `f` a function type, `let f: fn(...) -> ... = fn(x) ...`", or naming the open type variable
in a generic call ("`I` of `on_get` is known only from this function's parameters: pass a named
function"). Reported once per lambda; the body is still checked with the parameters as error types, so no
cascades (Principle II).

## R8. Formatting and editors

**Decision**: `fm_params` prints a parameter without a type node as its name (`fmt.c:125-146`); `fm_lambda`
prints `fn(a, b) => expr` or `fn(a, b) {` block. `=>` is added to the TextMate, tree-sitter and Vim
grammars, the site's Pygments lexer, and `editors/check.js`'s corpus check.

## R9. The standard library and packages

**Decision**: migrated by fixes (`tools/vo_fix.vo`, as in specs 056 and 058). Sites without an expected
type (12 of 34 lambdas in std code) become **named private functions** where they capture nothing (the
five `on_get` handlers in `std/web.vo` (915, 924, 927, 957, 1096), the two `__serve_workers` callers in
`std/http.vo`), and typed bindings only where they capture (decided per site; target: fewer than 5% of the
lambdas in `examples/` and `std/` need a typed binding, SC-001). The six `on_get` doc tests in `std/web.vo`
pass named functions, as user code does in `examples/` (none of whose 25 lambdas lacks an expected type).

## R10. Counts and targets

| Corpus | lambdas | typed params | single `return` body | no expected type |
|---|---|---|---|---|
| `std/` (code) | 34 | — | 13 | 12 (R9) |
| `std/` (doc tests) | 69 | — | 53 | 6 (`on_get`) |
| `examples/` | 25 | 18 | 18 | 0 |
| `packages/postgres` | 2 | 1 | 2 | 0 |
| `tests/cases/` | 280 | 238 | 164 | 29 untyped lets + 5 other |
| docs (`reference.md`, `errors.md`, site, README) | 74 | 60 | 47 | 8 |

Single-return lambdas take 54 of 83 lambda lines in `examples/`; the `=>` form puts each on one line
(SC-003).

## R11. Migration proof

**Decision**: `tests/migrate_059.py`, as `migrate_058.py`: every golden-case source with a lambda is migrated
by applying fixes, then must check, run and test with byte-identical output; the 29 untyped-let cases get
typed bindings by E0342's fix. `apply_fixes.py` covers E0342 and E0413 fix results.

## R12. Reference budget

**Decision**: the reference is at ~15,000 of 15,000 tokens today. The rule takes one paragraph in
"Functions as values", replacing the sentence "Anonymous functions write parameter and return types like
named functions" and the line-510 advice to wrap a builtin in a typed lambda (now `fn(s) => len(s)`); its
8 lambdas become `=>` forms (7 are single-return). The net change must be ≤ 0 (SC-005), measured by
`ref_examples.py` and stated in the change.

## R13. Version and codes

**Decision**: ships as **0.5.0** (058 shipped as 0.4.0). New codes: **E0342** (types, fixable), **E0343**
(types, not fixable), **E0413** (control flow, fixable). E0335–E0340 stay retired; E0508 stays unassigned.
