# Research: vo-lang v2 Core

Phase 0 decisions. No NEEDS CLARIFICATION items remain. Grants and scopes were decided by the user on 2026-11-13: grants are automatic from declarations, and file, environment and network effects can be scoped.

## 1. Effect syntax

- **Decision**: An effect clause goes after the return type: `fn save(r: Report) -> result[void] uses console, file.write("out/") { … }`.
  - The effect names are `console`, `input`, `file.read`, `file.write`, `net`, `clock`, `random`, `env`.
  - Scopes are string literals in parentheses: directories for file effects, variable names for `env`, hosts for `net`.
  - The canonical order is the order listed above. Scope strings are sorted. `vo fmt` enforces both.
  - No clause means pure. Tests use the same clause: `test "loads" uses file.read("fixtures/") { … }`.
- **Rationale**: The clause comes last because the signature reads as "what it takes, what it gives, what it touches". It starts with a keyword (`uses`) and has one spelling. Scopes as literals keep the reviewed text equal to the grant.
- **Alternatives**: Effect rows inside the return type (Koka style; harder for agents to read). Attributes such as `@effects(...)` (a second syntax family). Inferring effects for named functions (rejected by the spec's assumption; reviewers need them written).

## 2. Grants and runtime enforcement

- **Decision**: A run's grant is exactly `main`'s `uses` clause, and a test's grant is its clause. Static checking already guarantees that every function's effects are covered by `main`. At run time, every effectful builtin is checked against two things:
  - the **innermost user function frame's declaration** (scope check for file and environment, effect bit for the rest), and
  - the grant.

  A violation is R0010 `capability_denied`, naming the path or variable and the allowed scope. File paths are normalized lexically (`.` and `..` resolved, and absolute paths compared as given), then checked with `realpath`-equivalent resolution via `openat` with `O_NOFOLLOW` on each component. That way symbolic links cannot escape.
- **Rationale**: Checking the innermost declaration is the strongest guarantee that is still cheap. Because callers cover callees, it implies the grant check. Lambdas run under the declaration of the frame that calls them, which the static check guarantees covers the lambda's effects. That stays correct when a lambda is returned out of the function that created it.
- **Alternatives**: Checking only against the grant (weaker: a function promising `file.write("out/")` could still write elsewhere if `main` is broader). Tracking a dynamic intersection of all frames (the same result, more work).

## 3. Coverage between declarations

- **Decision**: Declaration D covers requirement R when every effect in R is in D, and for scoped effects every scope in R lies under some scope in D. That is a path-prefix match on normalized directories; environment names must be equal. Unscoped covers everything.
  - **Builtins** need their effect with no particular scope: `write_file` needs `file.write`, and any declared scope satisfies it, because the path is checked at run time.
  - **User functions** contribute their declared scopes.
  - A shortfall is E0501 (missing effect) or E0503 (scope too narrow), each with a fix that adds exactly the missing effects or scopes.
  - An effect or scope never needed is W0501, with a removal fix.

## 4. Effects of function values and higher-order functions

- **Decision**:
  - Function types carry effect sets: `fn(int) -> int uses console`.
  - A function value can be used where its effects are a subset of the expected type's (contravariance is not needed: parameters must match exactly).
  - Anonymous functions never write `uses`; their effects are inferred from their bodies.
  - Effect polymorphism uses effect parameters: `fn each[T, E: effects](xs: list[T], f: fn(T) uses E) uses E`. At a call, `E` becomes the argument function's effects, and the caller must cover them.
  - The builtins `map`, `filter`, `fold`, `sort_by`, `any` and `all` have the same rule built in.
- **Alternatives**: Implicit effect propagation for every function parameter, which hides effects from reviewers and contradicts "explicit". An `uses any` escape, which defeats checking.

## 5. Anonymous functions and closures

- **Decision**:
  - Syntax: the `fn` keyword in expression position, `fn(x: int) -> int { return x * 2; }`, with the same rules as named functions (types written out; return type required when a value is returned).
  - Captured `let` bindings and parameters are copied into a closure object when the expression is evaluated.
  - Capturing a `var` is E0505, with a fix that inserts `let <name>_now = <name>;` before the statement and renames the uses inside the closure.
  - At run time a closure is `{fn node, captured slots}`. A call copies the captures into frame slots after the parameters.
  - A named function used as a value is a closure with no captures.
- **Rationale**: One syntax shared with named functions costs no extra reference text. By-value capture avoids the action-at-a-distance bugs agents create with shared mutable state.
- **Alternatives**: A short arrow form `x => x * 2` (a second spelling, ambiguous parameter types). Capture by reference (shared mutable state, which conflicts with determinism of reasoning).

## 6. Calls on any expression; field calls versus the v1 method diagnostic

- **Decision**: Postfix `(args)` may follow any expression (`make_adder(1)(2)`, `handlers[0](x)`, `r.on_save(x)`). The parser produces a call-value node. The checker decides:
  - if the callee is a record field of function type, it's a field call;
  - if the "object" has no such field, it's the v1 E0110 "vo has no methods" with its rewrite fix (that logic moves from the parser into the checker).

## 7. Generics

- **Decision**:
  - Declarations: `fn swap[A, B](p: Pair[A, B]) -> Pair[B, A]`, `record Pair[A, B] { first: A; second: B; }`, `variant Tree[T] { leaf; node(Node[T]); }`.
  - Parameters: `[T]`, `[T: ordered]`, `[E: effects]`.
  - Type arguments are never written at call sites. They are inferred by unifying parameter types with argument types and with the expected type (annotations, return types, field types). If inference fails, E0310 names the parameter, with a fix that adds an annotation (`let s: Stack[int] = …`) when the statement is a `let` and the first later use of the variable in the same block determines the type.
  - Types are written with arguments everywhere in type position: `Stack[int]`, `new Stack[int] { … }`.
  - Variant cases infer from the expected type (`let t: Tree[int] = Tree.leaf;`).
  - Generic bodies are checked once with type parameters as opaque types. Only `==`/`!=` (all types) and, for `ordered` parameters, `< <= > >=` and key-less `sort` are allowed. Comparing an unconstrained parameter is E0311, with a fix that adds `: ordered`.
  - Runtime: values stay untagged. Calls to generic functions push their inferred type arguments into the frame. Record and variant types carry argument lists. `values_equal`, `render_value`, `sort` and map hashing resolve type parameters through those, with a ponytail marker.
- **Rationale**: Inference-only keeps one spelling and removes the `f[int](x)` versus indexing ambiguity. Erasure with type-argument frames fits the tree-walker and keeps memory layouts unchanged.
- **Alternatives**:
  - Explicit call-site arguments (`f::[int]()`): a second spelling.
  - Monomorphization: duplicate checking and memory; maybe later for speed.
  - User-defined interfaces: out of scope by spec.

## 8. The effects record (`vo.effects`)

- **Decision**: Plain text at the project root, which is the nearest ancestor directory of the entry file that contains a `vo.effects` file (or the entry's directory when creating one). One line per top-level function and test, sorted by path and then name: `<path>:<name> uses <effects>` (or `pure`). For the full format see contracts/effects-record.md.
  - `vo check` compares the current program (entry plus imports) with the record.
  - Growth is E0504, one per grown function, with a `related` note per call path that introduced it.
  - Shrinking is W0502. Renames (one removed and one added in the same file with identical effects) are W0503.
  - With no record, there is one W0504 explaining `vo effects --update`.
  - `vo effects` prints the capability summary. `vo effects --update` rewrites the record and prints the same changes it wrote.
- **Rationale**: A line-per-function text file gives exactly the review diff the spec asks for, with no git dependency.
- **Alternatives**: Comparing with a git ref (ties vo to git). Embedding the record in source comments (noisy diffs).

## 9. Deterministic clock and randomness

- **Decision**:
  - `random_int(lo, hi)` (effect `random`) uses a 64-bit PCG generator. The seed comes from `--seed N`; otherwise from the clock for `run` and fixed at 1 for `test`.
  - `now_ms()` (effect `clock`) with `--clock-ms N` returns N, then N+1, N+2, … on successive calls (deterministic and monotonic). `vo test` uses `--clock-ms 0` unless told otherwise.
  - `env(name) -> result[str]` (effect `env`): a missing variable is `err("not set")`.

## 10. Migration (SC-005)

- **Decision**: v1 programs fail E0501 wherever effects are missing. The fixes add `uses` clauses to every function in one round. To get there, the checker computes the transitive requirement first and reports, for each function, the full missing set (including what its callees will need once they're fixed). That way a single application of every first fix converges. `tests/migrate.py` runs this over every v1 golden program and benchmark solution and compares outputs.

## 11. Benchmark v2

- **Decision**: `bench2/tasks/` has four categories, at least 10 each (contracts/benchmark-v2.md).
  - **Multi-file tasks** provide a starter tree; the agent replies with `# file: path` blocks.
  - **Refactor tasks** give an existing codebase plus an instruction, and are graded by hidden cases plus "the rest still behaves the same" checks.
  - **Hidden-property tasks** grade with cases the agent never sees.
  - **Capability tasks** state the allowed effects:
    - vo is graded by `vo effects` on the solution (main's grant within the allowance) plus the run;
    - Python is graded by running under a wrapper that installs `sys.addaudithook` (open with write modes, `socket.*`, `subprocess`) and replaces `os.environ` with a recording mapping.
  - A separate 20-task feature set in `bench2/features/` measures SC-006 (vo only).
  - The harness reuses `bench/agent.py` logic, extended for multi-file replies and hidden grading.
- **Alternatives**: Running Python under strace or seccomp (more precise, not installed by default; recorded as the upgrade path).
