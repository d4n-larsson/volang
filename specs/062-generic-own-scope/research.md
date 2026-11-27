# Research: A Generic Function's Own Scope

Read from the source at `ab41b22` (vo 0.7.0 plus the bug sweep fixes).

## R1. Where the scope check gets its scopes

- Every scoped check (`rt_check_path`, `rt_check_name`, the `net` check, `rt_path_allowed`,
  `rt_bit_scoped`, `open_parent`) iterates with `path_scoped(after, bit)` in `src/rt_effects.c`. It
  follows `parent` links and returns each frame whose `effects` set scopes the effect, skipping a frame
  that holds the same set as the frame below it.
- A frame's `effects` is set in `vm_push` (`src/vm.c:100`) and in the inlined generic entry (`:721`):
  the function's declared set; for a function without one (anonymous functions, standard-library
  functions, whose declarations are cleared before a run) the caller's set; for a generic call
  `rt_es_grant(declared, caller's)`, the union that loses scopes (`src/types.c:548`).

**Decision**: the walk reads a frame's *own declaration* (`fn->a.n->b.fx->declared`, or the test's), not
`effects`. A frame without a declaration has no scopes of its own. `effects` and the union stay as they
are and answer only "has this code the effect" (`grant_has_bit`).

**Rationale**: for a function without effect parameters the two are the same object, so nothing changes.
For a generic function the declaration has exactly the scopes the author wrote: FR-001 and FR-005 follow
without touching how frames are built. The root grant is still added by `rt_root()`.

**Alternatives**: keep the scopes in the union (a "both must hold" set) — it narrows callbacks, which is
what spec 013 (C3) fixed; a second effects field in the frame — 8 bytes on every frame, see R5.

## R2. Telling passed-in code from own code

The spec speaks of code "passed in under the effect parameter". At run time a closure does not say where
it came from, and it does not need to. The checker already knows the effects of every call as the
enclosing function sees them.

**Decision**: a call is *transparent* when its effects, as required of the function it is written in,
consist of that function's effect parameters and nothing else. Three shapes:
- a call of a function value whose type is `fn(...) uses E` (`N_CALL_VALUE`, and a field call
  `s.get()` on a `Store[E]`);
- a builtin that calls a function argument of such a type (`map(xs, f)`, `concurrent_map`, `sort_by`);
- a call by name of a generic function whose effects after substitution are only the caller's effect
  parameters (`tcp.read(c)` with `c: Conn[E]`, `apply(f)`).

Walking outward from the callee's frame to the caller's, a transparent call means the caller's own
scopes are not applied.

**Rationale**: it is exact for what the caller promised. A function's own scopes are about the concrete
effects its code performs; a call typed with effect parameters only performs none of its own as far as
the caller's declaration goes, whoever wrote the callee. A function's own anonymous function of type
`fn() uses E` can only do `E` things, so treating it as passed-in is right. One with a mixed type
(`uses E, file.read`) is not transparent, so the function's scopes apply to what it does itself
(FR-002), and the `f()` inside it is transparent in turn.

**Alternatives**: recording in each closure the frame that created it and running it under that frame's
path — breaks S2 for a function with fixed effects (`h(f: fn() uses file.read("a/"))` must narrow `f`);
deciding by who created the closure — the same closure must be narrowed by a fixed type and not by `E`.

**Corrected during implementation.** The third shape was first "a named generic call whose effects after
substitution are only the caller's effect parameters". That fails when the generic function called has
scopes of its own: `outer[E]` (own `a/`) calls `inner[E]` (own `a/`) by name and hands `f` on; the call's
effects are `E` and `file.read("a/")`, so it was not transparent and `outer` bound `f` (case
`generic_own_p6`, FR-004). The mark on a call by name is now about the *binding* only (its effect
parameters are bound to the caller's effect parameters and nothing else), and the walk keeps one bit of
state, `carried`: what is being checked runs as an effect parameter of the frame the walk is at. It is
set by a transparent call of a function value, kept through a marked call by name, and cleared by any
other step. A marked call by name without it binds as usual: the callee's own code is the caller's own
code.

**Confirmed in the source (T004)**: every call of a function value records its effects through
`fx_value_call` (`src/check_func.c` at the call of a value and the two builtin shapes,
`src/check_expr.c` for `__serve_call`); a generic call by name substitutes its declaration in
`check_generic_call` (`src/check_generic.c`). `fx_mark_param` (`src/check_effects.c`) sets the mark from
the same sets: no mask, no any-scope bits, no scopes, at least one effect parameter.

## R3. Where the mark lives

`Node.flags` is 16 bits and the 16 values 1 to 32768 are all defined. Flags are read per node kind.

**Decision**: `NF_FX_PARAM`, set by `src/check_effects.c` on `N_CALL` and `N_CALL_VALUE`, reusing the
value of a flag that only another kind carries (`NF_PAIR`, `N_FOR` only).

**Confirmed (T005)**: the readers of `NF_PAIR` are `src/parse.c:1079-1092`, `src/check.c:694-775`,
`src/compile.c:535`, `src/vm.c:1115`, `src/fmt.c:770` and `src/check_structure.c:433`, each in the
handling of a `for` statement.

**Alternatives**: a side set of node pointers (a lookup on every step of the walk); widening `flags`
(the node is 64 bytes with checked offsets; `kind` would have to shrink).

## R4. Leaks the simple rules allow, and the rule that closes them

Three candidate rules were worked through on paper against call paths. `g[E]` declares
`uses E, file.read("a/")`; `L` is an anonymous function written in `g` that does a read of its own and
calls `f`; `apply[E2]` only calls what it is given; `h_scoped` declares `file.read("c/")` and takes a
function with fixed effects.

| Case | Path (the effect is in the last frame) | Must apply | Rule A: transparent calls only | Rule B: A + exempt the nearest frame of the lexical function | Rule C: A + exempt the creating activation, by serial |
|---|---|---|---|---|---|
| P1 | main → g → `f` | main | main ✓ | ✓ | ✓ |
| P2 | main → g → apply(f) → `f` | main | main ✓ | ✓ | ✓ |
| P3 | main → g → apply(L) → L's own read | g, main | g, main ✓ | ✓ | ✓ |
| P4 | main → g → apply(L) → L → `f` | main | **g**, main (refuses a callback) | ✓ | ✓ |
| P5 | main → g → L (called directly) → `f` | main | **g**, main (refuses) | ✓ | ✓ |
| P6 | main → outer → inner → `f` (both generic, both with own scopes) | main | **outer**, main (refuses) with the first form of the mark; ✓ with `carried` (R2) | ✓ | ✓ |
| L1 | main → g1 → (E) main's function → h_scoped(L) → L → `f` | h_scoped, main | ✓ | ✓ (h_scoped is walked, not jumped) | ✓ |
| L2 | L kept after g1 returned; main → h_scoped(L) → L → `f` | h_scoped, main | ✓ | ✓ (no frame of g) | ✓ |
| L3 | L kept after g1 returned; main → g2(k = L, a fixed type `uses file.read("a/")`) → L → `f` | g2, main | ✓ | **main only: g2 is exempted as if it were g1** | ✓ (g1's serial is not live) |

Rule A never leaks but breaks FR-004 for a function's own wrapper around a callback (P4, P5), which is
the common `retry(fn() { note(); return f(); })`. Rule B leaks in L3. **Decision: rule C.**

- A transparent call written in a named function's body: the caller's frame is that function's
  activation; its scopes are skipped at the next step (rule A).
- A transparent call written in an anonymous function: besides the step, the activation that created the
  anonymous function is *exempt* when the walk reaches it. Frames in between are walked one by one, so a
  scoped function entered by a call that is not transparent still applies (L1).
- When the walk reaches an exempt activation, its own call is judged like any other: transparent (it
  passed `E` on) or not.

**Why exemption by exact activation is sound**: the anonymous function's `f()` is typed with the effect
parameters of the activation that created it; that activation's declaration says nothing about them.
Every other frame on the path is judged by the call that entered the frame below it.

## R5. Identity of an activation

A frame address is reused, so "the frame at address X" is not an identity (L3).

**Decision**:
- A table `fx_acts[]` of `{frame, fn, serial}` in `src/rt_effects.c`. A frame is entered in it when it is
  a named function with effect parameters (a new serial), or the frame of an anonymous function whose
  closure has an owner (the owner's serial). Entering first drops every entry whose frame address is at
  or above the new frame's (those frames have returned).
- `Closure` gets `owner`: the serial of the activation it was created in (0 outside generic functions).
  `make_closure` reads it from the creating frame's entry.
- An entry is live for a frame when the address and `fn` match. A function that registers does so on
  every entry, so a returned frame's entry cannot survive a new frame of the same function at the same
  address.
- Tasks are forked processes and inherit the table; a worker starts with an empty one.

**Cost**: nothing on a call that is not a generic call with effect parameters or a closure with an
owner. `ponytail:` 256 entries and 8 pending exemptions; past either, the walk exempts nothing more
(refuses more, never less). Upgrade path: a frame field, if a profile ever shows generic recursion deeper
than that around scoped operations.

**Alternatives**: a serial in the frame (`FR_SLOTS` 64 → 72: every frame of every program, three
platform files, the speed gate); a hidden captured variable in every anonymous function of a generic
function (the compiler, the closure layout and `vo query` all see it).

## R6. Compatibility

- The checker and `vo.effects` are untouched (FR-011): the mark is not part of any output.
- A program whose generic function touched a target outside its own scopes now stops with R0010. No
  mechanical fix: the author either widens the scope or passes the work in. Changelog entry with both.
- The standard library: its declarations are cleared before a run, so its frames have no scopes of their
  own; only its calls' marks matter. `std/` is searched for generic functions with scoped declarations
  as a task, and the examples and packages too.
- Recordings: the events and their order do not change for a program the change does not stop.

## R7. The remaining gap, stated

A generic function that calls itself, with an anonymous function created in an outer activation and
called under an inner one, has the *outer* activation exempted (the creator), and each inner activation
applies its own scopes unless the call that entered the frame below it is transparent. That is the
correct reading of the rule, and it is covered by a case (`generic_own_recursive`), so that the behavior
is pinned rather than assumed.

## R7a. A known conservative case

An effect performed directly by a standard-library function that declares only an effect parameter
(`tcp.read(c)` with `c: Conn[E]`) starts the walk without `carried`: library declarations are cleared
before a run, so the run time cannot see that the effect is the parameter's. A generic function with
scopes of its own for that effect, calling such a library function on a value its caller passed in,
therefore binds it. That refuses more, never less; no case in the repository meets it.

## R8. Proof by tests

- Golden cases for P1 to P5 and L1 to L3, each with the same program for a function without an effect
  parameter where one exists (SC-001).
- `tests/scope_fuzz.py` fuzzes paths, not call shapes, so a new script does this: `tests/scope_calls.py`
  builds random chains of functions with and without an effect parameter, with own scopes or none,
  ending in an own read, a call of what was passed in, or an own wrapper around it. The expected outcome
  is written from `contracts/call-path.md` (which functions bind which read), not from the C.
- The stress build runs the new cases (closures gain a field).
