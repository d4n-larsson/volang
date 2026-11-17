# Research: Agent-Friendly Testing

Decisions for spec 018, each with its rationale and the alternatives rejected. Code references are to
the tree at `b873b39` (after spec 017).

## R1. Test identifiers

**Decision**: a test's identifier is `FILE::NAME`. FILE is the path exactly as the `file` field of
every report prints it (relative to the working directory when given relative), and NAME is the test's
or property's name. The CLI splits an identifier at the first `::` that follows `.vo`. Test names are
already unique within a file (E0202), so the identifier is unique within a run.

**Rationale**: it is stable under adding, moving and editing other tests (FR-002), and it is made only of
things the agent already sees in reports. No numbering, hashing or position is involved. A name
can contain spaces and `::`, but a file path ending in `.vo::` is unambiguous.

**Alternatives**: `file:line` (changes whenever code above moves); a hash (unreadable); the name alone
(clashes across files, see the spec's edge cases).

## R2. Listing: `vo test --list`

**Decision**: `--list` checks the program exactly as `vo test` does: diagnostics, exit 65 on errors,
warnings first. It then prints one entry per selected test and property in run order (files in path
order, items in source order), without compiling or running anything. Selection options (`--filter`,
`--test`, `--spec`, `--diff`) apply, so the listing shows what a run with the same options would run.
- **Text**: one line per entry: `ID (test|property, FILE:LINE:COL)`. When present, ` spec A, B` and
  ` uses EFFECTS` follow, and then the documentation comment on its own line, indented by two spaces.
  The last line is `N tests` (singular `1 test`).
- **JSON**: a `test_entry` record per entry, then the summary with `listed`.

The documentation comment is every `#` comment line directly above the item (above its `@spec` line,
if any), with `#` and one space removed, joined with `\n`. This is the rule `vo fmt` and `vo doc`
already use for an item's documentation (`doc_comments` in `src/doc.c` reads the same lines; the
listing needs the whole block, not only the first line).

**Rationale**: FR-001. Not compiling keeps listing fast (SC-001) and guarantees no effect runs. Applying
the selection options lets the agent check a selection before running it, which is the proposal's dry run.

**Alternatives**: a separate `vo query tests` command (another command for agents to learn; it would
duplicate test-mode checking); listing without checking (an invalid file would list tests that cannot run).

## R3. Selecting tests: `--test`, `--spec`, `--diff`

**Decision**:
- `--test ID` (repeatable) selects tests by identifier.
- `--spec ID` (repeatable) selects tests that name `ID`, or an ID that starts with `ID/`, in their
  `@spec`. So `--spec US1` selects `US1/AC2`.
- `--diff FILE` (`-` is stdin; until now only allowed with `impact`) selects the tests and properties
  that `vo impact` reports as `test` for that diff.

Several options of one kind form a union; different kinds, and `--filter`, combine as an intersection.

An unknown `--test` identifier, or a `--spec` ID that no test names, is **T0011 `unknown_test`**
(exit 64). Its message lists up to three closest identifiers, ranked by edit distance with ties in run
order, using the existing "did you mean" ranking from name resolution. Nothing runs.

A diff that reaches no test, or an intersection that is empty, is a normal run: the text report says
`0 tests selected`, the JSON summary has zeros, and the exit status is 0. A diff that does not apply
is T0007 `diff_mismatch`, as for `vo impact`.

**Rationale**: FR-003 and FR-004. Reusing `vo impact`'s answer means "affected" has exactly one
definition. `--test` is exact where `--filter` is a substring (US1 scenario 4).

**Alternatives**: a glob or regular-expression language for `--test` (a second pattern syntax to
learn; `--filter` already does substrings); an error for an empty diff selection (CI on a diff that
touches only documentation would fail).

**Implementation note**: `--diff` in `vo test` needs the reference collection that `vo impact` turns
on (`q_collect`) while checking. `cmd_test` sets it when `--diff` is given, then calls the impact
steps (`df_parse`, `im_items`, `im_removed`, `im_callers`) and keeps the entries with role `r_test`.

## R4. Captured output

**Decision**: while a test runs, everything it writes to standard output goes to a per-test capture
buffer instead of the output stream. That covers `print` and `write` (`rt_print` with fd 1) and the
standard output of its tasks, which the parent forwards in `src/task.c`.
- **Size**: the buffer is a fixed static 64 KiB per test. Further output is dropped and counted, and
  the captured text ends with `... N more bytes not shown`.
- **JSON**: the test record gains `"output"`, present only when the test wrote something.
- **Text**: the output of a test that did not pass is printed under its failure report, each line
  indented by four spaces after an `  output:` line. The output of a passing test is discarded.
- **Properties**: the buffer is cleared before the final rerun of the reported counterexample (this
  rerun already exists in `prop_run`), so the captured output belongs to the reported case.
- **Standard error** (`eprint`) is not captured. It never mixes with the JSON on standard output.

**Rationale**: FR-008 and US2 scenarios 6–7. A static buffer survives the per-test heap reset
(`gc_start` before each test) with no rooting, and the limit means one test cannot flood the report.

**Alternatives**: wrapping each printed line as its own JSON record (more records to correlate);
capturing on the heap (it is reset per test, so rooting and copying would be needed); also capturing
stderr (it never corrupts the stream, and diagnostics of the program itself go there).

**Behavior change**: in text mode, passing tests no longer show what they print. Golden cases whose
tests print are listed in the results (FR-019).

## R5. Explaining a failing `expect`

**Decision**: the compiler treats a call `expect(cond)` according to the shape of `cond`, at compile
time. Nothing is evaluated twice and the evaluation order is unchanged.
- **A comparison** (`==`, `!=`, `<`, `<=`, `>`, `>=` on `int`, `float`, `str` and, for `==`/`!=`, any
  type; the generic `OP_LT_PARAM` never occurs, because tests are never generic): the left operand goes into a temporary, then the right one, then the comparison. The
  result is passed with both operand values to a new builtin entry `BU_EXPECT_CMP`
  (`rt_expect_cmp(cond, left, right, node)`). Builtins take three argument slots, so this fits. On
  failure it renders each operand that is not a literal, with the operand's source text and type.
- **`a and b`**: compiled as `expect(a)` followed by `expect(b)`, each again by these rules. This is the
  same short-circuit order: `b` runs only if `a` held. The failure points at the part that was false.
- **`not c`**, where `c` is a comparison: as a comparison whose result is inverted. The message says
  the comparison was true.
- **`a or b`**, and anything else: unchanged (`rt_expect`). For `or`, the message names both parts as
  false; it gives no operand values, because a part that was a comparison has already been overwritten
  by the time the call happens.

Message: "`expect` failed: `len(xs) < limit` is false." The text report adds one line per operand,
`    len(xs): 7`, aligned like `expected:`/`actual:`. JSON adds
`"operands":[{"text":"len(xs)","value":"7"}, ...]` to `failure`. Values are rendered as `expect_eq`
renders them and shortened to 200 bytes.

**Rationale**: FR-005. Doing it at compile time avoids the problem of re-evaluating operands after the
fact (side effects, `random_int`, calls). Splitting `and` is exactly equivalent and is the common case
(`expect(x > 0 and x < 10)`).

**Spec amendment** (planning): FR-005's "reports the part that decided the outcome" applies to `and`
and `not`. For `or`, both parts were false, and the report names both, without values.

**Alternatives**: rewriting at run time from saved temporaries (the compiler reuses temporaries, so
values can already be gone); a separate `expect_lt`/`expect_gt` family (several spellings of one check,
against Principle I).

## R6. The path to the first difference in `expect_eq`

**Decision**: when `expect_eq` fails and the type is not a scalar (`int`, `float`, `bool`, `str` short
enough to be shown whole), a structural walk of both values finds the first difference in a fixed order.

| Type | Path step | First difference |
|---|---|---|
| list, deque, ordered set, priority queue (in iteration order) | `[i]` | first index whose elements differ; else the first index present on one side only |
| record | `.field` | first field in declaration order |
| variant, `result` | `.case` | different cases end the path and show both cases; same case continues into the payload |
| map, ordered map | `[key]` (key in vo syntax) | first key of the expected value, in its order, missing or different in the actual one; else the first extra key of the actual one |
| str | — | character position (1-based) of the first differing character or of the shorter end |

- **Reports**: the text report adds `    first difference at [412].name: expected "bob", actual "bab"`.
  JSON adds `"difference":{"path":"[412].name","expected":"\"bob\"","actual":"\"bab\""}`. A side that
  has no element there is `null` in JSON and `(none)` in text, followed by both lengths.
- **Strings**: the path is empty, `"character": N` is added, and both values are excerpts of up to 40
  characters starting at that character.
- **Leaves**: compared with `values_equal`, so the walk agrees with `expect_eq`'s own result, floats
  included.

**Rationale**: FR-006 and SC-003. Order is fixed by the values, so the report is deterministic.

**Alternatives**: a full diff of all differences (long reports, unbounded size); a textual diff of the
rendered values (paths are more precise, and long values are shortened anyway).

## R7. Causes

**Decision**: a non-passing test record gains `"cause"` with one of these values:

| cause | When |
|---|---|
| `check_failed` | `expect` or `expect_eq` failed |
| `too_few_inputs` | a property's `assume` discarded too many inputs |
| `error_in_code` | a runtime error (other than those below) whose innermost frame is not the test's body |
| `error_in_test` | the same, in the test's own statements |
| `requires_broken_by_test` | R0012, and the call that broke the `requires` is in the test |
| `requires_broken_by_code` | R0012 broken by a call inside the program |
| `ensures_broken` | R0013 |
| `hole` | R0011 |
| `outside_grant` | R0010 |
| `timeout` | R0009 |
| `out_of_memory` | R0006 |

`capture_trace` (`src/vm.c`) already walks the frames when a runtime error happens in a test. It also
records whether the innermost frame belongs to the test. For R0012, which is reported at the call site,
that frame is the caller, which is exactly the question. A runtime error forwarded from a task
(`concurrent_map`) is `error_in_code` (a task always runs a function). The text report is unchanged:
the cause is in JSON only.

**Rationale**: FR-007. The list is fixed and documented, and additions stay within schema 1.

**Alternatives**: a free-form "explanation" (not machine-readable); a `blame` field of `test`, `code` or
`environment` (coarser, and the agent's next step differs between causes in the same group, such as a
hole and a timeout).

## R8. Coverage

**Decision**: `vo test --coverage` makes the compiler emit a new instruction `OPC_COVER k` at each
coverage point. With no `--coverage`, no instruction is emitted and the code is the same as before. The
points are:
- function entry (after the `requires` checks);
- lambda entry;
- the start of an `if`'s then-block;
- its `else` block;
- for an `if` without `else`, a landing when the condition is false (an extra jump, only in coverage
  mode);
- each `match` arm;
- each `while`/`for` body.

Each point is recorded at compile time in a table `{node, kind, function}`. The handler stores 1 in
`cov_hit[k]`: one byte per point, in an anonymous shared mapping created before the tests run.
- **What counts**: only files that are not standard library (`FileRec.std`) or packages
  (`FileRec.pkg`). Code reached through function values, `map` or tasks counts as run. Tasks are forked
  children, so they write into the same shared bytes. Storing a constant 1 is idempotent, so concurrent
  tasks cannot race it into a wrong answer, and nothing has to be sent back.
- **Report**: after the test records and the `@spec` summary, one entry per point never hit, in file and
  source order, then totals, then the usual summary.
  - Text: `not run: FILE:LINE:COL KIND in NAME` and `coverage: 4 of 5 functions, 9 of 12 branches`.
  - JSON: `coverage` records and a `coverage_summary` record.
- **Kinds**: `function`, `lambda`, `then`, `else`, `if_false`, `case`, `loop`.
- Coverage never changes a test's result or the exit status.

**Rationale**: FR-009 and FR-010. Counts depend only on what ran, so the report is byte-identical across
runs and platforms. One increment per block entry keeps the overhead within SC-004's 1.5×.

**Alternatives**: line coverage (lines are not the unit of behavior, and `vo fmt` puts several on one
statement); short-circuit `and`/`or` branches (more noise than signal at this stage; noted in the spec's
assumptions); sampling with the trace (`--trace` is far slower and limited by `--trace-limit`).

**Ceiling** (`ponytail:`): at most 65,536 points per run; more is T0003 with a message. Raise the
limit or allocate on demand when a program needs it.

## R9. Static warnings for tests that cannot fail

**Decision**: two new warnings, in the testing group next to W0601.
- **W0602 `test_checks_nothing`**: a `test` or `prop` whose body (nested blocks and lambdas included)
  contains no `expect` or `expect_eq` call. A body with a hole is exempt, since W0601 already reports
  it. The span is the test's name.
- **W0603 `constant_check`**: an `expect` whose condition is a literal, `not` of a literal, or a
  comparison of two literals; or an `expect_eq` whose two arguments are both literals. A literal is a
  number, string or boolean constant, or a list, record or variant case built only of literals. The
  span is the check.

Neither has a fix: deleting the check or the test would lose the intent.

**Spec amendment** (planning): FR-011's "no call to a function of the program that contains one" can
never apply, because `expect` is allowed only inside tests (E0410). The rule is therefore the body alone.

**Rationale**: they catch the proposal's "tests without meaningful assertions" at check time, before
anything runs. Warnings never change an exit status.

**Alternatives**: running the test to see whether a check executes (that is coverage's job, R8);
flagging `expect_eq(f(x), f(x))` (needs equivalence of expressions; it would have false positives).

## R10. Mutation testing: which changes

**Decision**: `vo test --mutate` changes the AST of functions in the program's own files, one change at
a time. Only functions that at least one selected test reaches are changed, and the changes never
touch code inside `requires`/`ensures`. Each change keeps the program well-typed:

| Original | Changed to |
|---|---|
| `<` / `<=` | `<=` / `<` |
| `>` / `>=` | `>=` / `>` |
| `==` / `!=` | `!=` / `==` |
| `+` / `-` on `int` or `float` | `-` / `+` |
| `*` / `/` on `int` or `float` | `/` / `*` |
| `and` / `or` | `or` / `and` |
| `true` / `false` | `false` / `true` |
| `not e` | `e` |
| integer literal `n` | `n + 1` (not for the largest integer) |

- **Order**: changes are made in file order, then source position, then the table's row order, so a
  run's set of changes is deterministic.
- **Cap**: `--max-mutants N` (default 500) caps the number tried. The rest are counted as `not_tried`.
- **`--diff`**: restricts the functions to those the diff changes or adds.
- **Shown text**: the original is the expression's source text; the changed text is the same with the
  operator token (found between the operands' spans) or the literal replaced. Changed code is never
  written to a file (FR-015).

**Rationale**: FR-012 and FR-014. These operators find the boundary and sign errors that the agent and
its tests share, the roadmap's problem 2. One `+1` change per literal halves the count of the usual
`±1` pair, and most boundary errors show up with either direction.

**Alternatives**: deleting statements (often produces type errors or "missing return" changes that
need re-checking); bytecode-level changes (no faithful source text to show); string-literal changes (rarely
informative and plentiful).

## R11. Mutation testing: how changes run

**Decision**:
1. **Baseline**: the selected tests run normally in the `vo` process, with coverage on, recording for
   each test which functions it reached. The function-entry handler stores 1 in a shared byte per test
   and function, so tasks are counted too (R8). Their records print as usual.
   `ponytail:` one byte per test and function, for example 10 MB for 2,000 tests × 5,000 functions,
   mapped without reserving memory up front; a sparse set if programs grow past that.
   If any of them does not pass, the run prints **T0012 `mutation_needs_passing_tests`** and exits 1.
2. **Each change**: `vo` flushes its output and forks. The child applies the change to the AST node
   (operator code, literal value, or a flag that makes the compiler skip a `not`), resets the compiler
   state, turns coverage points off and drops its view of the shared hit and reach bytes, and compiles
   the whole program again, which takes milliseconds. The children never write the shared bytes, so the
   baseline's reach, which decides the tests for every later change, stays as the baseline recorded it. It then runs, in run order and
   with output discarded, only the tests that reached the changed function. It exits 0 if all of them
   pass (the change survived), or 1 with the first test that did not pass (the change was caught).
3. **Limits**: the parent waits with a limit of `10 × (baseline ms of those tests) + 1000 ms`. When it
   expires, it kills the child: a timeout counts as caught (FR-013). A runtime error or a broken contract
   in the child is a failing test, so it is caught too.
4. **Report**: one `mutant` record per surviving change, in order:
   - JSON: `file`, `start`, `end`, `function`, `text`, `mutated`, `tests` (identifiers that ran).
   - Text: `SURVIVED FILE:LINE:COL in NAME: \`age >= 18\` -> \`age > 18\` (2 tests ran)`.
   - Then a `mutation_summary`: `mutants`, `caught`, `survived`, `not_tried`. Text:
     `mutation: 12 changes, 11 caught, 1 survived, 0 not tried`, followed by "survivors are candidates:
     a change can behave like the original".
   - The exit status is the baseline's: survivors are evidence, not failures (FR-015).
5. **`--coverage`, `--list`**: `--mutate` cannot be combined with either (T0001).

**Rationale**: forking gives each change a pristine heap, interpreter and compile state. A crash or an
infinite loop in a changed program cannot hurt the run. Tests reach a function by the baseline's
recorded set, which is exact, unlike the static call graph. A full recompile in the child avoids
reasoning about stale generic call-site records.

**Determinism**: the set and order of changes and which tests run are fixed. A change that makes a
test much slower but still finishing could time out on a slow machine and finish on a fast one. The
large limit makes this unlikely, and the reference says so. Tests that write files run once per
change, as they would on repeated runs.

**Alternatives**: running changes in-process with re-compilation (compile and interpreter state would
need resetting, and a crash ends everything); running all tests for every change (slower by the number of
tests that do not reach the function); parallel children (deterministic, but not needed for SC-005; a
`ponytail:` note marks it as the upgrade).

## R12. CLI and schema summary

**New options of `vo test`**:
- `--list`
- `--test ID` (repeatable)
- `--spec ID` (repeatable)
- `--diff FILE` (now also for `test`)
- `--coverage`
- `--mutate`
- `--max-mutants N` (1 to 100000; only with `--mutate`)

The existing `only_with` checks (`src/main.c`) gain these rules, each T0001. `--list`, `--coverage` and
`--mutate` are exclusive.

**JSON within schema 1** (additions only):
- New kinds: `test_entry`, `coverage`, `coverage_summary`, `mutant`, `mutation_summary`.
- `test` gains `id` on every record, plus `cause`, `output`, `failure.operands` and `failure.difference`.
  Adding `id` changes every golden case that has a JSON test record; those cases are regenerated and
  listed in the results (spec FR-019, amended).
- The `test` summary gains `listed` and `selected`.

## R13. Reference budget

**Estimate**: about 550 tokens, within the 700 of FR-018. The additions are:
- the new options in the command-line block and one bullet each;
- three sentences in Tests (operands, difference, captured output);
- the cause list (one line);
- the coverage and mutation paragraph with one example;
- the new kinds in Machine-readable output;
- W0602, W0603, T0011 and T0012 in the catalog (outside the reference).

The reference is now about 11,687 tokens.

## R14. Testing this spec

- **Golden cases**:
  - `tests/cases/test/`: listing in text and JSON, selection, T0011, captured output, each cause,
    operands for each operator, each difference kind;
  - `tests/cases/coverage/`: each kind, std and packages excluded, totals;
  - `tests/cases/mutate/`: the `is_adult` survivor and its fix, T0012, timeout counted as caught,
    `--max-mutants`, `--diff`;
  - W0602 and W0603 in `tests/cases/test/`.
- **Step cases** (`tests/cases/test_steps/`): `--diff` with a diff file, `--list` not writing a file.
- **`tests/test_scale.py`** (SC-001, SC-004 timing): generates 200 tests in 20 files, checks listing
  under 1 s and that every identifier selects exactly its test, and measures the overhead of coverage
  over the speed corpus's tests.
- **`tests/mutate_eval.py`** (SC-005): for each fixture in `tests/mutate_eval/`, a small program with a
  known boundary, tests that miss it and the missing test. The boundary change must survive with the
  weak tests and be caught with the full ones. It also times `--mutate` on every `examples/` program
  (under 60 s each).
- **The determinism step of `make gate`** runs the new golden and step cases 20 times.

**Spec amendment** (planning): SC-005's "on the benchmark v2 reference solutions" becomes this fixture
set. The bench2 tasks keep their logic in `main` or have no vo tests (spec 005 had to copy them for the
same reason), and "no survivors after restoring" is replaced by "that change is caught": equivalent
changes can survive a complete suite.
