# Contract: which declarations bind an effect

This states the run-time rule (security.md S2) after spec 062. It is what `tests/scope_fuzz.py` models.

## The rule

An effectful operation with a target (a path, a host, a variable name, a program, an address) is
checked against:

1. the declaration of the function whose code performs it, and
2. walking outward along the calls that led there, the declaration of each calling function, **except**
   a function that reached the code below it only through its effect parameters, and
3. the root grant (`main`'s, the test's or the property's).

"Reached only through its effect parameters" means one of:

- the function called a value whose type is `fn(...) uses E` (directly, as a field, or by handing it to
  a builtin such as `map`), `E` being effect parameters of that function and nothing else;
- the function called a generic function by name and handed its effect parameter on to it (the callee's
  effect parameters are bound to the caller's and nothing else), and the code below runs as that
  parameter; the callee's own code is bound by the caller as usual;
- the code below is a call of that kind written inside an anonymous function that this activation of the
  function created.

A function without scopes for the effect binds nothing, as before. Anonymous functions and
standard-library functions have no declaration of their own, as before.

## Consequences

| Program shape | Bound by |
|---|---|
| `g[E]`'s body performs the effect | `g`, then `g`'s callers |
| `g[E]` calls `rd(p)` by name, which performs it | `g`, callers |
| `g[E]` calls `f()` with `f: fn() uses E` | `f`'s own path, then `g`'s callers; not `g` |
| `g[E]` calls `apply(f)`; `apply` calls `f` | callers of `g`; not `apply`, not `g` |
| `outer[E]` calls `inner(p, f)`, both with scopes: `inner`'s own read | `inner`, `outer`, callers |
| the same: `f`, called by `inner` | callers of `outer`; not `inner`, not `outer` |
| `g[E]` calls `apply(fn() { own(); f(); })`: the `own()` part | `g`, callers; not `apply` |
| the same: the `f()` part | callers of `g`; not `apply`, not `g` |
| `h` (no effect parameter) calls `f()` with `f: fn() uses file.read("a/")` | `h`, callers |
| a kept anonymous function of `g` called after `g` returned | everything on the path it is called from |

## R0010

Unchanged in form:

```text
FILE:LINE:COL: runtime error[R0010]: file.read("b/y.txt") is not allowed: `g` declares file.read("a/").
```

JSON: `effect`, `target`, `allowed` (the refusing function's own scopes for the effect), `declared_by`
(the refusing function). For a generic function `allowed` lists the scopes it wrote, not its caller's.

## Standard-library functions

A standard-library function that declares effect parameters and nothing else (`sqlite.query(db, ...)`
with `db: Database[uses E]`, `tcp.read(c)`) performs its effect as that parameter. A generic function
that calls it on a value its caller passed in does not bind it; one that calls it on a value its own code
made (a database it opened) does (case `generic_own_library`).

## Refuses more than the rule says

An effect performed directly by an anonymous function written inside such a standard-library function
is still bound by a generic caller's own scopes (research R7a).

## Not part of the contract

Serial numbers, the activation table and its limits. Past a limit the walk binds more functions, never
fewer.
