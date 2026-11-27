# Feature Specification: A Generic Function's Own Scope

**Feature Branch**: `062-generic-own-scope`

**Created**: 2026-11-27

**Status**: Implemented (2026-11-27; not released)

**Input**: User description: "A generic function's own effect scope is enforced at run time (bug sweep
2026-11-27, finding 12). Today a function with an effect parameter, such as
`fn g[E: effects](p: str, f: fn() uses E) -> str uses E, file.read("a/")`, runs with the union of its own
declaration and its caller's grant (spec 013, C3), and the union drops the function's scopes when the
caller's grant is unscoped (or merges them when it is scoped). So `g` called from a `main` that declares
`file.read` can read "secret.txt" in its own body, where a function with the same declaration and no
effect parameter is stopped with R0010. The static checker accepts `read_file(p)` in `g` for a path known
only at run time because spec 050 relies on the run time to enforce the "a/" scope (security.md S2: every
declaration on the call path that has scopes). Wanted: an effect that the generic function's own body
performs (directly, or through functions it calls by name) is checked against the function's own declared
scopes, exactly as for a non-generic function; an effect performed by a function passed in under the
effect parameter E (a callback `f: fn() uses E`, or a value of a type carrying E) keeps what the caller
granted and is not narrowed by the generic function's own scopes. The root grant and every other
declaration on the call path keep applying as now. R0010 names the generic function and its declared
scope. Record and replay, tasks, workers and serve handlers behave the same. No change to the language's
syntax or to what the checker accepts."

Background: [docs/bug-sweep-2026-11-27.md](../../docs/bug-sweep-2026-11-27.md), finding 12. The behavior
was measured on vo 0.7.0 with this program, where `a/x.txt` and `b/y.txt` exist:

```vo
fn rd(p: str) -> str uses file.read { ... read_file(p) ... }

fn g[E: effects](p: str, f: fn() -> str uses E) -> str uses E, file.read("a/") {
    return rd(p) + " " + f();
}

fn h(p: str) -> str uses file.read("a/") {
    return rd(p);
}

fn main() uses console, file.read("a/"), file.read("b/") {
    print(g("a/x.txt", fn() => rd("b/y.txt")));   # inside other        (right)
    print(g("b/y.txt", fn() => "-"));             # other -             (wrong: g declares "a/")
    print(h("b/y.txt"));                          # R0010, `h` declares file.read("a/")   (right)
}
```

`g` and `h` declare the same scope. `h` is held to it; `g` is not, because `g` has an effect parameter.

What this spec must keep:
- **Security (`docs/security.md`, S1 and S2):** a scope a function declares is a promise a reviewer reads
  in the source and in `vo.effects`. S2 says every declaration on the call path that has scopes is
  checked. Spec 050 lets a scoped function call one that is given its target, on the strength of S2.
- **Spec 013 (C3):** a function passed to a generic function keeps the effects its caller granted. That
  was the reason for the union and must stay true.
- **Principle III:** the same program and inputs give the same output on x86-64, ARM64 and RISC-V, and a
  recording replays to the same result.
- **Principle I:** no new syntax and no second way to write a declaration.

## User Scenarios & Testing *(mandatory)*

The users are AI coding agents writing generic helpers (retry, timing, caching, pooling, the functions of
`std.web`, `std.tcp` and `std.workers`), the people who review what a program may touch, and the owners
of existing programs.

### User Story 1 - A generic function keeps its own promise (Priority: P1)

A reviewer reads `fn load[E: effects](name: str, on_miss: fn() -> str uses E) -> str uses E, file.read("cache/")`
and concludes that `load` itself reads only under `cache/`. Today that is false when the caller declares
more. After this change it is true: what `load`'s own code reads is held to `cache/`, whatever the caller
declares.

**Why this priority**: it is the finding. A declared scope that is not enforced is worse than none,
because it is read as a guarantee.

**Independent Test**: golden cases run the program above and variants of it: the generic function's own
code touching a target outside its scope ends with R0010 naming that function, under a caller with no
scopes and under a caller with other scopes.

**Acceptance Scenarios**:

1. **Given** `g` declares `uses E, file.read("a/")` and its caller declares `file.read` without scopes,
   **When** `g`'s own code reads `secret.txt`, **Then** the run ends with R0010: `g` declares
   `file.read("a/")`.
2. **Given** the same `g` and a caller that declares `file.read("a/"), file.read("b/")`, **When** `g`'s
   own code reads `b/y.txt`, **Then** the run ends with R0010 naming `g`.
3. **Given** the same `g`, **When** its own code reads `a/x.txt`, **Then** the read succeeds, as today.
4. **Given** `g` calls a function by name (one without scopes of its own, which is given its target),
   **When** that function reads outside `a/`, **Then** R0010 names `g`, as it names `h` for a function
   without an effect parameter.
5. **Given** an anonymous function written inside `g` and handed by `g` to another generic function,
   **When** it reads outside `a/`, **Then** R0010 names `g`: code written in `g` is `g`'s own, wherever it
   is called from.
6. **Given** `g` and a function `h` with the same declaration and no effect parameter, **When** each does
   the same thing in its own code, **Then** the outcomes are the same in every case above.

---

### User Story 2 - What the caller passes in keeps what the caller granted (Priority: P1)

An agent passes a function to a generic helper: a handler to a server, an action to `retry`, a callback
to `load`. The function does what its writer was allowed to do. The helper's own scopes are about the
helper's code and do not narrow the function passed in; the helper's caller, and everything above it on
the call path, still bound it.

**Why this priority**: it is what spec 013 fixed and what every generic library function relies on. A
fix for story 1 that narrows callbacks would break `std.web`, `std.tcp` and user helpers alike.

**Independent Test**: golden cases pass functions that touch targets outside the helper's scope and
inside the caller's grant, directly and through a second generic function; each succeeds. The same
functions touching a target outside the caller's grant end with R0010 naming the caller.

**Acceptance Scenarios**:

1. **Given** `g` declares `file.read("a/")` and its caller declares `a/` and `b/`, **When** the function
   the caller passed reads `b/y.txt`, **Then** the read succeeds, as today.
2. **Given** the function the caller passed is handed on by `g` to another generic function, which calls
   it, **When** it reads `b/y.txt`, **Then** the read succeeds: handing a function on changes nothing.
3. **Given** the caller declares `file.read("b/")` and what `g` needs, **When** the function it passed
   reads outside `b/` and outside every scope the caller declares, **Then** R0010 names the caller's
   declaration, as today.
4. **Given** a value of a type that carries the effect parameter (a connection `Conn[E]`, a record of
   functions `Store[E]`), **When** a generic function uses it, **Then** the effects it performs are bound
   by what the caller granted for `E`, not by the generic function's own scopes.
5. **Given** a generic function with no scopes of its own (`uses E`, or `uses E, file.read`), **When**
   anything runs under it, **Then** nothing changes from today.

---

### User Story 3 - A refusal that says whose promise it was (Priority: P2)

When the run stops, the report names the generic function and the scopes it declares, in the form
R0010 already has, so an agent can tell "this helper may not read there" from "this program may not read
there" and fix the right declaration.

**Why this priority**: the new refusals appear in programs that ran before; the report has to make the
cause plain.

**Independent Test**: the golden cases of story 1 show the report in text and in JSON.

**Acceptance Scenarios**:

1. **Given** a refusal by a generic function's own scope, **When** the report is printed, **Then** it
   names the effect, the target, the function and its declared scopes, and the trace shows the frames as
   for any R0010.
2. **Given** `--json`, **When** the same refusal is reported, **Then** the record has the fields R0010
   has today (`effect`, `target`, `allowed`, `declared_by`), with the generic function in `declared_by`
   and its own scopes in `allowed`.

---

### User Story 4 - Existing programs keep working, or are told why not (Priority: P2)

The owner of a program upgrades. If no generic function in it touched a target outside its own scope,
nothing changes: same output, same recordings, same `vo.effects`. If one did, the program now stops with
R0010 at that point, and the changelog says what changed and how to fix the declaration.

**Why this priority**: this is a change in what runs, made for security. It must be narrow and announced.

**Independent Test**: the whole test suite, the examples and the standard library's own tests pass
unchanged except for the cases this spec adds; recordings made before the change replay.

**Acceptance Scenarios**:

1. **Given** every program of the test suite and `examples/`, **When** they run on the new version,
   **Then** their output is unchanged.
2. **Given** a recording made on the version before, of a program the change does not stop, **When** it
   is replayed, **Then** it replays as before.
3. **Given** a program the change stops, **When** its owner reads the changelog, **Then** they find the
   rule and the two ways out: widen the generic function's own scope, or pass the work in as a function.

### Edge Cases

- **Every scoped effect, not only files:** the same rule for `file.read`, `file.write`, `file.delete`,
  `net`, `env`, `process`, `serve` and any other effect that takes scopes.
- **Several scopes and several effect parameters:** a function with `uses E, F, file.read("a/"), net("x")`
  holds its own code to both of its scopes; what comes in under `E` or `F` is the caller's.
- **A generic function used as a value** (spec 028) behaves as when it is called by name.
- **A generic function calling itself, or another generic function, by name:** each one's own code is
  held to its own scopes; the effect parameter passed along still stands for the first caller's grant.
- **A function passed in that was itself written inside another scoped function:** it is bound by the
  function it was written in, and by the call path, as today.
- **A scope directory swapped for a link** (bug sweep finding 9): the open still starts at the outermost
  scope and every scoped declaration that applies must hold the path. The generic function's own scope is
  one of them for its own code.
- **Replay:** the check is made from the recorded targets, without looking at the file system, as S2
  requires. A recorded `denied` for a call the generic function's scope now refuses matches a replay
  that refuses it.
- **Tasks, workers, serve handlers, tests and properties:** the function they run is bound the same way
  inside them as outside.
- **The standard library:** its generic functions with scopes of their own, if any, are held to them. A
  library function that only passes on what it is given is unaffected.
- **The static checker's answer does not change:** no program that checks today is rejected, and none
  that is rejected today checks.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: An effect performed by the code of a function that has effect parameters MUST be checked at
  run time against the scopes that function declares for that effect, as it is for a function without
  effect parameters.
- **FR-002**: "The code of a function" MUST include its body, the anonymous functions written in it, and
  everything those reach by calling functions by name, wherever an anonymous function written in it is
  later called from.
- **FR-003**: An effect performed by a function value that reached the generic function under one of its
  effect parameters, or through a value of a type that carries one, MUST NOT be checked against the
  generic function's own scopes.
- **FR-004**: Such a function value MUST be bound by the grant of the code that supplied it and by every
  declaration above that code on the call path, as today. Passing it through any number of generic
  functions MUST NOT narrow or widen what it may do.
- **FR-005**: The caller's grant MUST NOT add to what a generic function's own code may do: scopes are no
  longer dropped when the caller is unscoped, nor merged when it is scoped.
- **FR-006**: Every other declaration on the call path that has scopes, up to the root grant, MUST keep
  applying to every effect, as S2 states.
- **FR-007**: A generic function without scopes of its own MUST behave exactly as today.
- **FR-008**: The rule MUST hold for every effect that takes scopes.
- **FR-009**: A refusal MUST be R0010 in its present form, naming the generic function and its own
  declared scopes, in text and in JSON.
- **FR-010**: The check MUST give the same result when recording, when replaying, in a task, in a worker,
  in a serve handler, in a test and in a property, and on the three architectures.
- **FR-011**: What the checker accepts and rejects, the syntax, and the contents of `vo.effects` MUST NOT
  change.
- **FR-012**: `docs/security.md` (S2) and the reference MUST state the rule; the changelog MUST name the
  change as one that can stop a program that ran before, with how to fix it.
- **FR-013**: The cost of an effect check MUST stay within the project's existing speed limits; programs
  that perform no scoped effect under a generic function MUST NOT slow down.

### Key Entities

- **Own scopes**: the scopes a function writes in its `uses` clause, apart from its effect parameters.
- **Effect parameter**: a name such as `E` standing for whatever the caller grants.
- **Own code**: what a function's body and its anonymous functions do, and what they reach by name.
- **Passed-in code**: a function value, or a value of a type carrying an effect parameter, that the
  caller supplied.
- **Call path**: the functions from the running one up to `main` (or the test or property).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For each scoped effect, a generic function and a non-generic one with the same declaration
  give the same outcome for the same action in their own code: 0 differences over the cases of this spec.
- **SC-002**: Every case in which passed-in code works today under a generic function still works: 0
  changed outputs in the existing test suite, the examples and the standard library's tests.
- **SC-003**: The program in the Background prints `inside other` for its first line and ends with R0010
  naming `g` at its second.
- **SC-004**: The effects-as-a-boundary scripts (`make security-check`) and the scope fuzzer pass, with
  cases added for generic functions.
- **SC-005**: Recordings of the replay test cases made before the change replay unchanged.
- **SC-006**: The speed checks of the gate pass within their present limits.
- **SC-007**: Finding 12 is marked fixed in the bug sweep report, with its cases.

## Assumptions

- This is a security fix to behavior the documents already promise (S2), so it ships as a bug fix with a
  changelog entry, not as a language change with a migration. No mechanical fix exists: the right
  correction (a wider scope, or passing the work in) is the author's decision.
- No program in the repository depends on the leak; if the standard library does, that is a finding of
  its own and is fixed in the library's declarations.
- "By name" means a call whose target the checker knows. A call of a function value with fixed effects
  in its type (not an effect parameter) is the generic function's own code too: the checker already
  required the function's declaration to cover those effects.
- Builtins that take functions (`map`, `filter`, `sort_by`, `concurrent_map`) have no declaration of
  their own and stay transparent: the function they call is bound as if called directly.
- How the run time tells own code from passed-in code is a matter for the plan.
