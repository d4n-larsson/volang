# Research: Typed Holes, Property Tests and Contracts

Decisions for spec 005. Every open point from the spec's "left to the plan" list is settled here.

## Holes

### R1: `todo` is a builtin, not a keyword

- **Decision:** `todo(desc)` is builtin 48 in `check_expr.s`'s table, with special typing.
  - Its one argument must be a `str` (E0301 otherwise). Any `str` expression is allowed; the W0601 message shows the argument's source text.
  - At run time, R0011's message carries the evaluated string.
  - Its type is the `expected` type passed to `check_expr(node, expected)`, which is already threaded through for `ok`/`err` (E0307) and generic inference.
- **Rationale:** a call needs no grammar change, `vo fmt` already formats it, and agents already know `todo!()`/`TODO()` from Rust and Kotlin.
- **Alternatives:**
  - `?name` hole syntax (Agda, Idris): new token, new grammar, unfamiliar.
  - A keyword `todo`: breaks the "every statement is a keyword or a call" table for nothing.

### R2: Expected type unknown is E0601

- **Decision:** when `expected` is 0 (`let x = todo("...");`, an operand of `==`, the argument of `str(...)`), report E0601 `hole_type_unknown`: "Cannot tell what type this hole must have; annotate the binding: `let x: T = todo(...)`." It has no fix, because the type is what is unknown.
  - The checker treats the hole as type error afterwards, so no cascade follows.
  - Where the context has a type but `check_expr` passes none (binary operands), the left operand's type is passed to the right operand when the left is not a hole. So `n + todo("x")` gets `int`. `todo("a") + todo("b")` is E0601.
- **Rationale:** E0307 means "ok/err without a known result type". Reusing it for a different construct would redefine a code (Principle II).

### R3: Holes as statements end the path

- **Decision:** `todo("...");` as an `N_EXPR_STMT` gets expected type `void` and sets `NF_TERMINATES`, as `return` does. Consequences:
  - E0403 (missing return) is satisfied.
  - The call-statement rule E0405 does not apply.
  - Statements after it get W0401, as after `return`.
- **Rationale:** the natural way to stub a function, `fn f(x: int) -> int { todo("f"); }`, must check clean apart from the hole warning.

### R4: W0601 carries the scope, and some warnings are suppressed

- **Decision:**
  - W0601 `hole` has the message ``Hole: `<desc source text>` needs a value of type `T`.``
    - `expected` is set to the type, using the existing `expected` field.
    - New field `scope`: an array of `{"name","type"}` for the parameters and locals visible at the hole, innermost first. For a property or contract it also includes the parameters (and `ret`).
    - Text output adds one note line per name, after the snippet: `    in scope: line: str, count: int`.
  - Visible top-level functions are not listed. They are in the file the agent is editing, and listing them would bloat every hole report.
  - A function, lambda, test or property containing at least one hole gets `FX_HOLES` = 1. While that is set:
    - W0201 (unused local) is not reported in its body;
    - W0501 (unneeded effect) is not reported for its clause.
  - `check`'s summary gains `"holes": N` in JSON and ` (N holes)` in text, both only when N > 0, so existing outputs are unchanged.
- **Rationale:** the expected type and the names in scope are what an agent needs to fill the gap. The suppressed warnings fire on every stub ("parameter never read", "console not needed") and would drown the hole report.
- **Alternative:** a new severity `"hole"`. It would change the meaning of an existing enum-like field and break consumers that switch on severity.

## Property tests

### R5: `prop` is an `N_TEST` with a flag

- **Decision:** the parser builds an `N_TEST` node with `NF_PROP` (flag 64). Its parameters (`N_PARAM` vec) go in the FX record's new `FX_PARAMS` field.
  - Every place that handles tests (compile, effects, effects record, fmt, filter, report) handles properties for free.
  - Only the runner and the checker's parameter binding look at the flag.
  - Grammar: `prop STRING ( param {, param} ) uses? block`, with at least one parameter (zero is the existing "expected a name" syntax error). Names share the test name space (E0202 on duplicates).
  - `vo.effects` lists them as `main.vo:prop "name" uses ...`.
- **Rationale:** a property *is* a test whose body runs many times. A new node kind would touch every `li t1, N_TEST` site (8 of them) for no behavioural difference.

### R6: What can be generated (E0602)

- **Decision:** a parameter type is generatable when:
  - it contains no function type (`TY_FUNC`) and no type parameter;
  - and a finite value exists.

  The check computes finiteness as a least fixpoint over records and variants:
  - `int float bool str`, lists and maps are finite (an empty list is finite);
  - `result[T]` is finite (`err("")`);
  - a record is finite when all its fields are finite;
  - a variant is finite when at least one case has no payload or a finite payload.

  Otherwise it reports E0602 `ungeneratable_type` on the parameter, naming the type and the reason ("contains a function type" or "has no finite value").
- **Rationale:** these are exactly the types the generator can build, and the check runs once per property.

### R7: The generator

- **Decision:** `src/prop.s` holds `prop_gen(type, size)`, which builds a heap value. It uses its own PCG-XSH-RR state (`pg_state`), separate from `random_int`'s, so `random_int` inside a property behaves as in a test.
- **Seeding:** case `i` of the property at item index `k` is seeded with `mix(seed, k, i)`. Any case can be regenerated alone, and adding a property does not change the others' inputs.
- **Size:** `size = 1 + i` for cases 0..99, so early cases are tiny and later ones reach about 100. The type decides how the size is used:

  | Type | Generation |
  |---|---|
  | `int` | 1 in 8: an edge from {0, 1, −1, 2⁶³−1, −2⁶³, 2⁶³−2, −2⁶³+1}. Otherwise uniform in [−size, size]. |
  | `float` | 1 in 8: an edge from {0.0, −0.0, 1.0, −1.0, 0.5, 1e15, −1e15}. Otherwise an integer in [−size, size] divided by a power of two from 1 to 64. Always finite. |
  | `bool` | Uniform. |
  | `str` | Length uniform in [0, size/4 + 1], at most 32. Characters, 1 in 16 multi-byte (`é`, `ß`, `日`, `😀`); the rest weighted to `a`–`e`, digits, space and punctuation `,;:-_#`. Small alphabets make repeated characters and the separators that parsers trip on common. |
  | `list[T]` | Length uniform in [0, size/8 + 1], at most 16. Elements use size/2. |
  | `map[K,V]` | The same lengths. Duplicate keys collapse, which keeps maps small. |
  | `result[T]` | 1 in 4 `err` with a generated `str`, else `ok`. |
  | Record | Each field in declaration order, with size − 1. |
  | Variant | At depth budget 0, a uniform choice among cases with no payload or with a non-recursive payload. Otherwise uniform over all cases; the payload gets size/2 and depth − 1. The depth starts at 6. |

- **Rationale:** bounded sizes keep 100 cases fast (SC-004: under 2 s under qemu). Edges and small alphabets find the bugs the bug set models: off-by-one, empty input, duplicates, overflow, separators and non-ASCII.
- **Alternative:** Hypothesis-style choice sequences (shrinking by shortening the byte stream). It is more general but harder to build in assembly. Type-directed shrinking (R8) is enough for vo's closed set of types.

### R8: Shrinking

- **Decision:** greedy and type-directed, in the classic QuickCheck style:
  - Parameters are shrunk left to right.
  - For the current value, candidates are produced in a fixed order. The first that still fails *the same way* replaces it, and the search restarts on that parameter. A parameter is done when no candidate fails.
  - "The same way" means the same failure kind: either the same check (same `expect`/`expect_eq` node), or the same runtime error code.

  Candidates per type:

  | Type | Candidates |
  |---|---|
  | `int` | 0; then x/2 toward 0; then x − sign(x) |
  | `float` | 0.0; then the truncated integer value; then x/2 |
  | `bool` | `false` |
  | `str` and `list` | Empty; the first half; the second half; each element removed (from the end); then each element shrunk in place (lists), or each character replaced by `a` (strings) |
  | `map` | Empty; each entry removed; then each value shrunk |
  | `result` | `err("")` → `ok` of a shrunk payload, or a shrunk payload of the same case |
  | Record | Each field shrunk in place |
  | Variant | Each earlier case without a payload; then a recursive payload of the same variant type (subtree replacement); then the payload shrunk |

- **Budget:** at most 2,000 candidate runs per property (`ponytail:` marker; raise it if real properties need more).
- **Rationale:** deterministic, simple, and locally minimal, which is what SC-004's "no single shrink step still fails" asks for.

### R9: The case loop, failures and the timeout

- **Decision:** `prop_run(test)` takes over `run_file_tests`' per-test body for `NF_PROP` items:
  - **Setup:** `gc_start` once per property, then a deadline from `--timeout` for the whole property.
  - **Per attempt:**
    1. `interp_reset`, then `rt_effects_start(1)`, so every case has the same seed and clock;
    2. generate the arguments;
    3. under `jmp_save`, call the compiled body through `vm_push` with the arguments as the first slots.
  - **Outcomes:**
    - **Pass:** count the case.
    - **`assume` false:** count a discard (R10).
    - **Failure:** an `expect` failure, or any runtime error except R0009 and R0006. Start shrinking.
    - **R0009 or R0006 outside shrinking:** report as for a test (`timeout` or `error`), with the input that was running as the counterexample, not shrunk.
  - **During shrinking:** a candidate that times out or runs out of memory counts as "not the same failure". When the deadline itself is reached, shrinking stops and the report keeps the smallest failing input so far, with `"shrink_stopped": true`.
- **Final run:** the failure fields (`tf_*`, `rt_err_*`) are taken from re-running the final counterexample, so the report's location and values belong to the shrunk input.
- **Memory:** `--mem-stats` covers the whole property (FR-016).

### R10: `assume`

- **Decision:** `assume(cond: bool)` is builtin 49.
  - It is allowed only in a property body; elsewhere it is E0604 `assume_outside_prop`.
  - When false, it raises an internal discard (`rt_raise` with the reserved code `ASSUME`, never shown) that the case loop recognises.
  - Each property runs until 100 cases pass or 1,000 attempts are made. With fewer than 100 accepted, the property fails with ``too many inputs were discarded by `assume` (N of 1000 accepted)``. The status is `fail`, with no code, no counterexample and no shrinking.
- **Rationale:** an early `return;` would make a property pass vacuously. The 10× budget is the usual QuickCheck default.

## Contracts

### R11: Syntax and the returned-value name

- **Decision:** `fn name(params) -> T uses ... requires COND ensures COND ... { body }`.
  - There are zero or more clauses, each keyword followed by one expression. A condition ends at the next `requires`/`ensures` or the body's `{`. That is unambiguous, because vo has no brace-literal expressions; `if c {` relies on the same.
  - The keywords may appear in any order and are checked in source order. `vo fmt` puts each clause on its own line, indented four spaces, with `{` after the last one:

    ```vo
    fn isqrt(n: int) -> int
        requires n >= 0
        ensures ret * ret <= n {
    ```

  - The returned value is `ret`. It is in scope only inside `ensures` of a function with a return type. A parameter named `ret` on a function with `ensures` is E0203 (shadowing).
- **Rationale:**
  - `result` is already a type keyword.
  - `out` was considered, but it is a parameter name in several benchmark reference solutions (m01, m02, m07); `ret` appears in none.
  - A binding form (`ensures(r) ...`) adds syntax for one name.
- **Restrictions:**
  - Clauses are not allowed on anonymous functions (they have no `uses` either). The parser reports the existing unexpected-token error there.
  - Clauses are not allowed on tests and properties.

### R12: Checking happens in the callee

- **Decision:** the compiler emits contract code in the function's own code.
  - **Prologue:** for each `requires`, it emits `ccond(cond)` then `JT ok`, then a call of builtin `rt_requires_fail(clause)`.
  - `rt_requires_fail` reports R0012:
    - its span is the frame's `FR_CALL` node (the call expression in the caller);
    - its trace starts at the caller, because the current frame is popped first;
    - the message is ``Precondition `n >= 0` of `isqrt` does not hold.``
- **Rationale:** every way of calling a function ends in `vm_push` of its code:
  - direct calls;
  - `CALL_FAST`;
  - `CALL_GENERIC`;
  - closures;
  - builtins such as `map` and `sort_by`, which go through `call_value`.

  Checking in the callee covers all of them (FR-023) without touching any call path. Functions without clauses get no extra instructions (FR-024).
- **`FR_CALL` for builtin callbacks:** for builtin callbacks it is the builtin's call node, so a precondition broken inside `map(xs, isqrt)` points at the `map(...)` call. That is the caller's line.

### R13: `ensures` and every exit

- **Decision:** when a function has `ensures`, the compiler allocates a slot for `ret` (when there is a return type) and an epilogue label.
  - Every `RET` site is compiled as a move into the `ret` slot plus `JMP epilogue`. These sites are `return e`, `return`, `try` propagation of an `err`, and the implicit return at the end of a void function.
  - The epilogue evaluates each `ensures` in order, with `JT` past `rt_ensures_fail(clause, exit_node)`, then `RET ret`.
  - The exit node, the span of R0013, is kept in a second hidden slot as the node's address (a constant, not a heap value).
  - R0013's message is ``Postcondition `ret * ret <= n` of `isqrt` does not hold.`` Its trace starts at the function itself.
- **Rationale:** one copy of the checks, and the exit span reports which `return` produced the bad value.

### R14: Purity and effects

- **Decision:** the effects checker computes the effect requirement of each clause expression separately.
  - Any requirement, including effect-parameter calls, is E0603 `impure_contract` on the clause, naming the effect.
  - Clause requirements are not added to the function's `FX_REQUIRED`, so contracts never change `vo.effects`.
- **Rationale:**
  - A contract that prints or reads files would change behaviour when it is checked.
  - The Principle II fix for E0501 (add the effect) is wrong here, which is why there is a separate code.

### R15: Clause scope and types

- **Decision:** clauses are checked after the parameters are bound and before the body.
  - Each clause is `check_expr(cond, bool)`, so a non-`bool` condition is E0301.
  - A `requires` clause sees the parameters only.
  - An `ensures` clause sees the parameters plus `ret` (when there is a return type), bound in a scope that is popped before the body. The body cannot see `ret`.
  - Type parameters and `ordered` work as in the body.
  - Holes in a clause are allowed, typed `bool`, and reported like any hole.

## Evaluation

### R16: The stand-in for the agent benchmark

- **Layout:** for each hidden-property task h01–h10:
  - `tests/prop_eval/hNN-name/props.vo`: properties that import the reference solution's functions, by their public names in `bench2/tasks/hNN/ref/vo`. They are written from `task.md` and those signatures only. The hidden cases are never read while writing them (spec assumption).
  - `tests/prop_eval/hNN-name/bugs/NN.vo`: at least 5 mutants of the reference's function file. Each is one realistic mistake: off-by-one, a missed empty or edge case, a wrong comparison, a lost duplicate, or overflow or sign handling.
- **The script:** `tests/prop_eval.py` runs every mutant in a temporary copy of the reference:
  1. through the visible cases, where it must pass all of them;
  2. through the hidden cases, where it must fail at least one, or the mutant is invalid and the script fails;
  3. through `vo test props.vo` with the mutant in place: caught when the exit status is 1.

  It also runs the properties against the unmodified reference, where all must pass, three times byte-identically (SC-002). It prints a per-task table and the total, and fails below 70% overall or with any task at 0.
- **Rationale:** reproducible, cheap, no agent runs, and it measures what the roadmap asks for: properties catch mistakes that the example checks do not.
- **Isolation:** `bench2/` is not modified, so the 703-run baseline is untouched.
