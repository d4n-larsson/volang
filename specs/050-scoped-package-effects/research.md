# Research: Scoped Effects Through Package Functions

Code read and probes run on 2026-11-23 (`bin/vo` 0.1.0).

## R1. The check-time rule: an unscoped callee is "needed in any scope"

**Finding**: an effect set has `mask` (declared bits; unscoped unless it has scope pairs), `scopes`,
and `any` ("needed in some scope", from builtin uses). A standard-library call already adds its
effects to the caller as `any` (`fx_call`: `ck_fx->direct->any |= callee->declared->mask`), and
`es_missing(declared, required)` already accepts an `any` bit against a scoped declaration. A user
callee's declaration enters the caller's requirement at three places: `fx_value_call`
(`check_effects.c:246`, used for anonymous functions, function values and generic calls from
`check_generic.c:552`), the edge fixpoint (`:457`, `es_union(callee->declared, need)`), and the
related notes (`:685`).

**Decision**: `es_requirement(set)` returns the set with each of `file.read`, `file.write`,
`file.delete`, `net`, `env`, `serve` that is in `mask` without scope pairs moved to `any`. It is
applied at those three places. `es_missing`, `es_covers`, `es_unused` and the fixes are not changed.
`process` is not in the list (E0512 keeps every declaration scoped).

Consequences that fall out without more code:
- scoped caller, unscoped callee: covered (FR-001);
- caller without the effect: the `any` bit is missing, E0501 with the fix adding the effect (FR-004);
- callee with scopes: its pairs stay in the requirement, E0503 as today (FR-004);
- the E0503 fix is `declared ∪ required`; the requirement no longer holds a plain bit from a callee,
  so the fix adds scopes and never the plain effect (FR-005);
- a function value of type `fn(...) uses net` bound to an effect parameter goes through
  `fx_value_call`, so User Story 5 is the same change;
- W0501: an `any` bit makes the caller's scopes count as needed, as for builtin uses.

**Rejected**: changing `es_missing` itself ("declared scoped, required unscoped: covered"). It is
also used by `effects_record.c:329` to report growth between two records, where `net("a")` becoming
`net` must stay a reported growth, and by `ty_func_fits` for function types written with scopes.

**Found while implementing**: the edge fixpoint already carries what a callee needs on to its caller
(`need` in the same line), so through plain calls `a(net("x")) → helper(net) → b(net("y"))` is E0503
on `a` at check time. Through a function value or an anonymous function only the callee's declaration
travels (a function type holds no more), so there the case is left to R2. `tests/scope_paths.py`
models exactly this.

**Also found**: literal hosts passed to `std.tcp.connect` are not compared with `net` scopes at check
time today (the `std_targets` table has file, `env` and `process` functions only). Not changed here.

## R2. The run-time rule: every scoped set on the frame chain

**Finding**: a frame's `effects` is its function's declared set; anonymous functions and
standard-library functions (their declarations are cleared before the run, `std_runtime_grants`) take
the caller's pointer; a generic frame takes `declared ∪ caller's` (`rt_es_grant`). The scope checks
compare the target with two sets: `rt_frame_effects()` (the innermost frame) and the root frame's.
The same pair is used to open files beneath a scope directory (`rt_open_parent`, `rt_open_file`,
`rt_bit_scoped`, `rt_path_allowed`). With R1 alone, the innermost frame of a helper is unscoped, so a
scoped function between the helper and the root would not be enforced.

**Decision**: one iterator in `src/rt_effects.c` yields the frames from `cur_frame` outwards whose set
scopes the bit, skipping a frame whose `effects` pointer equals the previous one's (std frames,
anonymous functions, recursion), and then the root frame if it was not reached. The seven functions
above loop over it instead of over two sets. The first refusing set, from the operation outwards, is
reported (today's order: the running function before the root), and `grant_frame` names the function
that declared it. Frame layout, `vm_push` and the inline call paths are untouched.

Tasks are forks and keep their frame chain; server handlers run on the server's chain; replay calls
the same functions.

`ponytail:` the walk is linear in call depth for each scoped operation (not for calls, `print`, or
effects without scopes). At depth 10,000 that is 10,000 pointer comparisons per file or network
operation. Upgrade path: a frame word linking to the nearest caller with a different scoped set.

**Rejected**: computing an effective set at every call (intersection of scope lists on the hot path,
memoized: slower calls for every program, SC-005); checking only the nearest scoped frame and the root
(a scoped function further out is then not enforced when an unscoped function or function value sits
between them).

## R3. Standard-library frames today (probe)

`main` with `net("127.0.0.1:1", "127.0.0.1:2")`, `reach` with `net("127.0.0.1:1")` calling
`std.tcp.connect` on port 2: R0010 "`reach` declares net("127.0.0.1:1")". The same for a builtin
(`read_file` under `file.read("a/")`). So a scoped function is already enforced when it calls the
library directly; R2 extends that to calls through helpers.

## R4. Existing behaviour that can change

- **Check time**: only programs that failed with E0503 because a scoped caller called an unscoped
  callee now check. Cases naming E0503 today: `const/scope_target`, `effect_sets/use_narrow`,
  `effects/env_denied`, `effects/net_port_narrow`, `effects/net_scope`, `effects/scope_too_narrow`,
  `fs/literal_outside`, `fs/rename_literals`, `process/literal_outside`, `security/l1_literal_scope`,
  `tasks/scope`. Each is run first; those whose error was that case are rewritten to a scoped callee
  and listed in the change (SC-003).
- **Run time**: a program in which a function with scopes uses, through an unscoped or generic callee,
  a target outside its own scopes but inside the root's. Today that is unreachable in a checked
  program except through a handle (`Database[E]` opened elsewhere and used in a function scoped to
  another directory). The suite and the examples show whether any exists.
- **`tests/diff_asm.py`** compares with the last assembly release: if `tests/gen_programs.py`
  generates scoped callers of unscoped callees, those seeds differ and are excluded as earlier
  checker changes were.

## R5. Tests

**Decision**:
- **Check cases** (`tests/cases/effects/`): helper in the file, in a module, in a package
  (`tests/cases/packages/`), for each of the six effects; the options-record form of spec 049; a
  three-function chain; the caller without the effect (E0501); the scoped callee (E0503 and its fix);
  the function value bound to an effect parameter; `process` (E0512).
- **Run cases**: target inside; outside the root; inside the root and outside a middle function,
  through a plain call, an anonymous function, a function value, a generic function, a
  `concurrent_map` task, a file link, and under replay.
- **`tests/scope_paths.py`** (SC-002): 2,000 generated programs, depth 1 to 6, each function scoped
  or not at random, a target chosen at random; a Python model of the rule says whether it must be
  E0503 (a literal passed straight to the library), R0010 (and which function is named) or reached.
  300 in `make test`.
- **`tests/scope_fuzz.py`**: its reads also go through an unscoped helper.
- **Spec 049's probes** (SC-008) become two cases.
- **Speed** (SC-005): the existing speed suite, plus one timing of 100,000 scoped file reads at depth
  1 and depth 1,000.

## R6. The reference at net zero

`tests/ref_examples.py` counts about 14,999 of 15,000 tokens (words × 1.3). The two sentences that
state the rule are reworded with the same number of words in total:

| | Now | After |
|---|---|---|
| Scopes | "A caller's scopes must cover its callees' (E0503);" (8 words) | "A caller's scopes must cover its callees' scopes (E0503); a callee's effect without scopes takes its callers';" (17) |
| Run time | "...against both the running function's declaration and that root grant: anything outside either is R0010 (exit 70), so no callee widens the root." (23) | "...against every scoped declaration on the call path: one outside is R0010 (exit 70)." (14) |

Net: 0 words. The spec's SC-006 is corrected from "at most 30 tokens" to "does not grow".

## R7. Documents

- `docs/errors.md`: E0503 says what is covered and that a callee without scopes needs none; R0010
  says "every scoped declaration on the call path" in place of "two declarations apply".
- `docs/security.md`: S1 keeps "an effect or a scope outside the grant does not check" for what the
  checker sees and names the case moved to run time; S2 states the path rule; the limits section
  notes that a scoped function may call a helper that is stopped only when it runs.
- `site/docs/guide/effects.md` and `modules-and-packages.md`: how to write a helper or package
  function that is given its target.
- `docs/roadmap.md`, `CHANGELOG.md`; spec 049's status line and research R1.

## R8. What `vo.effects` and `vo package` show

Both print declarations as written and do not use requirement sets, so nothing changes
(`effects_record.c` compares declared sets with the unchanged `es_missing`). SC-004 confirms it on
every example.
