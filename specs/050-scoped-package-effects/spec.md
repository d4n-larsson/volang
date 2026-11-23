# Feature Specification: Scoped Effects Through Package Functions

**Feature Branch**: `050-scoped-package-effects`

**Created**: 2026-11-23

**Status**: Implemented ([results](results.md); SC-005's depth figure is not met, see there)

**Input**: User description: "Scoped effects through package functions: a function outside std that
passes an effect target it is given (a host for `net`, and by the same rule a path for file effects, an
address for `serve`, a name for `env`) on to the standard library must be callable from a caller that
declares that effect with scopes. Prerequisite of spec 049 (the postgres package), found while planning
it (specs/049-postgres-package/research.md R1) and decided 2026-11-23: fix the checker first. Must keep:
the run-time check of every target against the root grant, E0503 for a literal target outside the
caller's scopes wherever the checker can see it, the security properties of docs/security.md, one way to
write each thing, no new syntax if a rule change suffices, the reference under 15,000 tokens, existing
programs keep checking, stable codes, three architectures, record and replay unchanged."

Background: an effect can be declared with scopes (`net("db.internal:5432")`, `file.read("data/")`),
and "a caller's scopes must cover its callees'" (E0503). A function that does not know its target when
it is written, because the target is a parameter, can only declare the effect without scopes, which
today means "everything"; no scoped caller covers that, so every caller up to `main` must declare the
plain effect too:

```vo
fn dial(host: str, port: int) -> result[Conn[uses net], TcpError] uses net {
    return connect(host, port, 1000);
}

fn main() uses console, net("127.0.0.1:5432") {     # E0503: `main` declares too narrow a scope: it needs net.
    ...dial("127.0.0.1", 5432)...                   # fix offered: `uses console, net`
}
```

Only standard-library functions escape this: the checker treats them like builtins, whose effect is
needed "in any scope" and whose target is checked when the program runs. So `std.tcp.connect(host, ...)`
works under a scoped grant and the three-line `dial` around it does not. The consequences:

- **No protocol package can be used by a program with a scoped grant.** Spec 039 added `std.tcp` so
  that Postgres, Redis and SMTP clients could be packages; each has a `connect(host, ...)`.
- **The same holds for any helper**, in a package or in the program, that opens a file, reads a
  variable or listens on an address it is given.
- **The fix the checker offers is to drop the scopes.** A program that follows it declares plain `net`:
  `vo.effects` then names no host, and nothing stops the program from reaching any other.

What this spec must keep:
- **The root grant bounds everything** (security property S2): no function reaches a target outside
  what `main` or the test declared, whatever any callee declares.
- **What the checker can see, it still reports**: a missing effect (E0501), a literal target outside
  the scopes (E0503), a callee whose own scopes are not covered (E0503).
- **A scoped declaration stays true**: a function that declares `net("a")` does not reach `b`, also
  not through a callee.
- **Principle I and V**: no new syntax; the reference stays under 15,000 tokens (1 left: it must not grow).

## User Scenarios & Testing *(mandatory)*

The users are AI coding agents that write vo programs and packages, and the people who review a
program's grant before running it.

### User Story 1 - A package function that is given its target (Priority: P1)

An agent writes a database client package whose `connect` takes the host in an options record, and a
program that uses it and declares `net("db.internal:5432")`. The program checks and runs, and reaches
that host and no other.

**Why this priority**: it is the reason for the spec and what spec 049 waits for.

**Independent Test**: check and run cases with a helper in the same file, in another module and in a
package, the target passed as a parameter, as a record field and as computed text, for each effect
that takes scopes.

**Acceptance Scenarios**:

1. **Given** the `dial` and `main` above, **When** checked, **Then** there is no diagnostic; **When**
   run, **Then** the connection to `127.0.0.1:5432` is attempted.
2. **Given** a package function `connect(o: Options) -> result[...] uses net, entropy` that passes
   `o.host` to `std.tcp.connect` and to `start_tls`, and a `main` declaring `net("db.internal:5432"),
   entropy`, **When** checked and run, **Then** it works as in scenario 1.
3. **Given** the same for a helper that reads a file it is given under a caller declaring
   `file.read("data/")`, one that writes under `file.write("out/")`, one that deletes under
   `file.delete("tmp/")`, one that reads a variable under `env("HOME")`, and one that listens under
   `serve("8000")`, **When** checked and run with a target inside the scopes, **Then** each works.
4. **Given** a test with `uses net("localhost:5432")` calling the package's `connect`, **When** `vo
   test` runs it, **Then** it checks and the test's clause is its grant.
5. **Given** a chain of three functions, the outer two scoped and the innermost without scopes,
   **When** checked, **Then** the outer one must still cover the middle one's scopes (E0503 if not),
   and nothing is reported about the innermost.

---

### User Story 2 - The grant still holds when the program runs (Priority: P1)

A reviewer grants a program `net("db.internal:5432")`. A package it uses tries another host, or the
program passes a host from its input. The program stops with R0010 naming the host, the allowed scopes
and the function whose declaration refused it. The same holds for a function in the middle of the call
path that declared narrower scopes than `main`.

**Why this priority**: the rule that lets User Story 1 check moves a guarantee from check time to run
time; it is acceptable only if the run-time guarantee is complete.

**Independent Test**: run cases where the target is outside the root grant, and where it is inside the
root grant but outside the scopes of a function between the root and the operation; for every effect
with scopes; through plain calls, function values, anonymous functions, generics with effect
parameters, and tasks.

**Acceptance Scenarios**:

1. **Given** `main` with `net("a")` calling a helper without scopes that connects to `b`, **When** run,
   **Then** it stops with R0010 naming `b`, the scopes `a` and `main`, before anything is sent.
2. **Given** `main` with `net("a", "b")`, a function `sync` with `net("a")` and a helper without
   scopes that `sync` calls with `b`, **When** run, **Then** it stops with R0010 naming `b` and `sync`:
   a scoped declaration is true of everything the function calls.
3. **Given** the same through a function value, an anonymous function, a function generic over an
   effect parameter, and a function run by `concurrent_map`, **When** run, **Then** the result is the
   same R0010.
4. **Given** a file helper without scopes called under `file.read("data/")` with `../secret`, an
   absolute path, or a link that leaves `data/`, **When** run, **Then** it is R0010, as for a direct
   read today.
5. **Given** a recorded run, **When** replayed, **Then** the same targets are allowed and refused, with
   no file system or network.

---

### User Story 3 - What the checker saw before, it still sees (Priority: P2)

An agent leaves out an effect, passes a literal target outside its scopes to a standard-library
function, or calls a helper that declares scopes its caller lacks. Each is reported at check time as
today, with its fix.

**Why this priority**: the change must not weaken a diagnostic that exists.

**Independent Test**: the existing golden cases for E0501, E0503, E0506, E0512 and W0501 pass
unchanged, except the cases whose only error was a scoped caller of an unscoped callee.

**Acceptance Scenarios**:

1. **Given** a caller that does not declare `net` at all and calls `dial`, **When** checked, **Then**
   E0501 with the fix adding `net`.
2. **Given** `read_file("secret.txt")` written in a function declaring `file.read("data/")`, beside a
   call of a helper without scopes, **When** checked, **Then** E0503 at check time, as today. (Literal
   hosts are not compared with `net` scopes at check time today; that is unchanged.)
3. **Given** a callee declaring `file.write("out/")` and a caller declaring `file.write("tmp/")`,
   **When** checked, **Then** E0503 with the fix that adds `"out/"`.
4. **Given** any E0503 that remains, **When** its fix is offered, **Then** the fix adds scopes; it
   never replaces a scoped effect by the plain one.
5. **Given** a function that declares `process` without a program, **When** checked, **Then** E0512,
   as today.

---

### User Story 4 - A reviewer reads the same record (Priority: P2)

A reviewer reads `vo.effects` and the output of `vo package` for a program that uses a database
package. The program's functions show their hosts; the package's `connect` shows `net`; the
documentation says what an effect without scopes on such a function means.

**Why this priority**: the effects record is how a grant is reviewed; its reading must stay simple.

**Independent Test**: effects-record cases before and after for a program with helpers and a package;
the `vo package` output for a package with unscoped effects.

**Acceptance Scenarios**:

1. **Given** a program whose `main` declares `net("db.internal:5432")` and uses a package function
   declaring `net`, **When** `vo effects --update` runs, **Then** the record lists `main` with its host
   and the package function with `net`, in the existing format.
2. **Given** a project that checked before this change, **When** `vo effects` compares, **Then** the
   record is unchanged.
3. **Given** `vo package` adding a package whose exported functions declare `net` and `entropy`,
   **When** it prints the package's effects, **Then** the output is as today.
4. **Given** the reference, the security document and the catalog entries of E0503 and R0010, **When**
   read, **Then** each says: an effect without scopes means everything in `main` or a test, and
   "within the callers' scopes" in any other function.

---

### User Story 5 - Standard-library functions as values (Priority: P3)

An agent passes `std.tcp.connect` to a function that takes a dialer as a parameter, in a program with
a scoped grant. It checks, and the target is checked when the program runs.

**Why this priority**: the same rule applied to function values; without it the rule has an exception
an agent must learn.

**Independent Test**: check and run cases passing `connect`, `read_file` and `env` as values to
functions generic over an effect parameter, under scoped callers.

**Acceptance Scenarios**:

1. **Given** a function with an effect parameter `N` that calls a function value of type `fn(str, int,
   int) -> ... uses N`, and a caller declaring `net("db:5432")` that passes `connect`, **When**
   checked, **Then** no diagnostic; **When** run with a host outside the scopes, **Then** R0010.
2. **Given** a function value whose type declares scopes the caller does not cover, **When** checked,
   **Then** E0503, as today.

### Edge Cases

- **`main`, tests and properties**: an effect without scopes there still means everything; nothing
  bounds the root but its own clause.
- **Exported functions of a package**: the rule is the same inside and outside packages; a package
  cannot widen what the program declared, and need not know it.
- **A handle used where its target is not allowed**: a `Database[E]` or `Conn[E]` opened by `main`
  for one target and used in a function that declares scopes without that target is R0010 at the
  use, since that function's declaration says it does not reach it.
- **Function values called later**: the scopes that apply are those of the functions running when the
  operation happens, not of where the value was made.
- **Tasks and workers**: a function run by `concurrent_map` or as a server handler is bounded by the
  path of the function that started it; a worker process is bounded by the root grant it was started
  with.
- **Recursion**: a function that calls itself contributes its scopes once.
- **Effects without scopes** (`console`, `input`, `clock`, `random`, `entropy`): unchanged.
- **`process`**: unchanged. Every declaration of `process` still names its programs (E0512), so a
  package cannot ask to run "whatever it is given".
- **Unused effect** (W0501) and **an effect set** (`effects Storage = ...`): unchanged; a set without
  scopes used in a non-root function follows the new rule.
- **Deep call paths**: the run-time check must not make a call or an effectful operation measurably
  slower for programs that do not use scopes.

## Requirements *(mandatory)*

### Functional Requirements

**The rule**

- **FR-001**: A function, anonymous function or function value that declares a scoped effect
  (`file.read`, `file.write`, `file.delete`, `net`, `env`, `serve`) without scopes MUST be callable
  from a caller that declares the same effect with scopes; no diagnostic is reported for that call.
- **FR-002**: In `main`, a test and a property, an effect without scopes MUST mean every target, as
  today. In every other function it means: the targets its callers allow.
- **FR-003**: The rule MUST be the same for functions in the program, in other modules and in
  packages, for effect arguments of types (`Conn[uses net]`) and for effect parameters bound from
  function values.

**Check time, unchanged**

- **FR-004**: A caller that lacks the effect MUST be E0501. A callee whose declared scopes the caller
  does not cover MUST be E0503. A literal target passed to a standard-library function outside the
  declaring function's scopes MUST be E0503. `process` without a program MUST be E0512.
- **FR-005**: The fix of E0503 MUST add the missing scopes and MUST NOT replace a scoped effect by the
  plain one.
- **FR-006**: Every program that checks today MUST check with the same diagnostics.

**Run time**

- **FR-007**: Every effectful operation with a target MUST be allowed only when the target is inside
  the scopes of every function on the call path that declares scopes for that effect, from the root
  to the function that performs it. Today's check (the running function and the root) is the special
  case of a path of two.
- **FR-008**: A refused target MUST stop the program with R0010 before the operation has any effect,
  naming the target, the scopes and the function whose declaration refused it (the one nearest the
  operation when several do, as today).
- **FR-009**: FR-007 MUST hold through function values, anonymous functions, effect parameters,
  handles that carry an effect (`Database[E]`, `Conn[E]`), tasks and server handlers, and during
  replay.
- **FR-010**: File targets MUST be resolved as today (`..`, absolute paths, links) before the
  comparison with each scope on the path.

**Reporting and documents**

- **FR-011**: `vo.effects`, `vo effects`, `vo package` output, `vo doc` and hover MUST show
  declarations as written, in their existing formats; a project that checked before has an unchanged
  record.
- **FR-012**: No new diagnostic code and no new syntax. The catalog entries of E0503 and R0010, the
  reference, `docs/security.md` (S1, S2 and the limits section) and the site's effects guide MUST
  state the rule; the reference MUST NOT grow (its sentences on scopes are reworded
  within the words they have).
- **FR-013**: The JSON forms of E0503 and R0010 MUST keep their fields.

**Fit**

- **FR-014**: The three architectures behave identically; recordings made before the change replay
  with the same output.
- **FR-015**: The roadmap, changelog, and spec 049's prerequisite note MUST be updated.

### Key Entities

- **Declaration**: a function's `uses` clause: effects, each with or without scopes.
- **Root grant**: the clause of `main`, of the running test or of the running property.
- **Call path**: the functions running, from the root to the one performing an operation.
- **Target**: the host and port, path, variable name or address an operation acts on.
- **Effective scopes**: for one effect at one moment, what every scoped declaration on the call path
  allows.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The helper and package cases of User Story 1 check with no diagnostic and run, for all
  six effects with scopes, with the target as a parameter, a record field and computed text.
- **SC-002**: In a generated corpus of at least 2,000 programs with call paths of depth 1 to 6, scoped
  and unscoped declarations mixed, and targets inside and outside each scope, a target is reached
  exactly when it is inside every scoped declaration on its path; no program reaches a target outside
  its root grant.
- **SC-003**: Every golden case and example that passed before passes unchanged, except the cases
  whose expected output was E0503 for a scoped caller of an unscoped callee; those are listed in the
  change.
- **SC-004**: The effects record of every example in the repository is byte-identical before and
  after.
- **SC-005**: The speed suite's geometric mean is within 2% of before; 100,000 scoped file reads at
  call depth 1,000 take at most twice as long as at depth 1.
- **SC-006**: The reference's token count is not above its count before the change.
- **SC-007**: The suite passes on x86-64, ARM64 and RISC-V; replay cases recorded before the change
  give identical output.
- **SC-008**: The probe programs of spec 049's research R1 (a helper around `connect`; a two-module
  package with `connect(options)`) check and run under `net("127.0.0.1:PORT")`.

## Assumptions

- **A rule change, not new syntax.** The alternative, naming the parameter that is the target
  (`uses net(host)`), would let the checker test literals at call sites of user functions, but needs
  new syntax, reference tokens, a rule for record fields, and a proof that the function passes only
  that parameter on. The targets that matter in practice are computed (an options record,
  configuration), and those are checked at run time today as well.
- **What a reviewer loses**: today, a program that checks cannot contain a scoped function calling
  one that might reach further; after this change that call checks and is stopped when it happens.
  **What is kept**: the root grant bounds everything; literal targets are checked before the program
  runs; scoped callees are covered. **What is gained**: programs using packages keep their hosts and
  directories in the grant instead of declaring the plain effect, and a scoped declaration in the
  middle of a path is enforced on everything beneath it.
- **Function values follow the same rule** (User Story 5); making them an exception would be a second
  rule.
- **`process` is left as it is**: its scopes are required everywhere, and running a program chosen by
  a caller is not something a package should get by default.
- **No warning for "unscoped helper under a scoped caller"**: it is the intended way to write a
  helper.
- Out of scope: the postgres package (spec 049), new effects, target parameters, subtyping between
  effect sets in types, checking literal arguments of user functions at check time.
