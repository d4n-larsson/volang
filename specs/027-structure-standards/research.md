# Research: Application Structure Standards (spec 027)

Measurements were taken with an approximate analyzer over sources in canonical format (`vo fmt`: one
statement per line), on four sets: the 13 example files, the 12 standard-library modules, the 1,152
golden-case programs, and the two programs written by agents in the experiments (`voledger` 739 lines,
`vositegen` about 1,350 lines). The analyzer counts a line as effectful when it calls an effectful
builtin, a standard-library function with `uses`, or a program function with `uses`. The real rules use
the checker's exact information; the numbers below choose the thresholds.

## R1 File size: F = 400 code lines

**Decision**: a program file with more than 400 code lines (lines that are not blank and not only a
comment) gets W0702.

**Measured**: examples at most 134; golden cases at most 136 except one deliberate stress test (2,032);
std: `json.vo` 715, `http.vo` 310, the rest at most 227; agents: ledger 672, sitegen 1,282.

**Rationale**: 400 catches both agent programs with a wide margin and nothing written by hand except
`std.json` (one module per import path: it cannot be split without changing `import std.json.NAME`,
so it carries an `@allow`, see R9). 300 would also catch `std.http`; 500 would still catch the ledger
but leaves less room for the agent's next program.

**Alternatives**: counting all lines (blank lines and comments would penalize documentation); counting
declarations (a file of many tiny functions is fine).

## R2 Function size: S = 50 statements

**Decision**: a function, test or property with more than 50 statements (at every depth, the statements
of anonymous functions included) gets W0703.

**Measured**: examples at most 32 (`board.vo` `render`); std: `json.parse_number` 53, `parse_string` 50,
`time.parse_time` 40; golden cases: 60 (a stress test) and the 2,030-statement stress test, everything
else at most 33; agents: sitegen `build_site` 66, `parse_page` 64, `render_md` 59, `parse_tpl` 46,
ledger `main` 44.

**Rationale**: 50 catches the three sitegen functions that do several jobs, and among hand-written code
only `json.parse_number`, a pure scanner that reads best in one piece (`@allow`, R9). 40 would add
`parse_tpl`, ledger's `main` and `time.parse_time`; ledger's `main` is caught better by R3, and the
lower limit buys little.

## R3 Pure core: an effectful function with 15 effect-free statements (not a run)

**Decision**: W0701 counts, in a function with effects (not a test or property), the statements whose
own expressions require no effect (R4), at every depth. With 15 or more it is reported at the function's
name, with the longest runs of effect-free statements (up to three) as related locations.

**Measured**: the spec's first idea, a run of R consecutive effect-free statements, finds almost nothing
where it matters: the longest run in either agent program is 7 (ledger `export_year`); ledger's `main`
has 44 statements, 21 of them effect-free, but interleaved with prints. Counting instead: 15 or more
effect-free statements occur in ledger `main` (21), sitegen `cmd_new` (16) and `cmd_build` (15); in no
example and no std function (`fs.list_dir` has 13); in 4 golden programs, whose `main` holds the whole
test on purpose.

**Rationale**: the count measures what the principle is about, how much decision-making sits in a
function that also does effects, whatever the order. A thin shell that calls pure functions has a few
effect-free statements of its own (`let page = try render(site);`), well under 15. The related
locations tell the agent which statements to move first.

**Alternatives**: a ratio of effect-free to all statements (unstable for short functions); runs (above).

**Spec change**: FR-002 and User Story 1 are updated to the count.

## R4 What an effect-free statement is

**Decision**: a statement is effect-free when evaluating its own expressions (for `if`, `while`, `for`
and `match`: the condition, iterable or scrutinee; for others the whole statement; nested blocks are
statements of their own) requires no effect: no call of an effectful builtin, of a function whose
declared effects are not empty, of an effect-generic function whose effects at that call are not empty,
or of a function value whose type has effects. `todo(...)` is effect-free. Creating an anonymous
function is effect-free; calling it is not, if it has effects.

**Rationale**: the effects checker already computes the effects each call requires (`fx_call`,
`fx_value_call`); the rule reads them per statement instead of per function. No new analysis.

## R5 Duplication: D = 6 sibling statements, local names and literals normalized

**Decision**: W0704 compares runs of 6 consecutive statements of one block (compound statements
compared whole, their contents included), in the non-test functions of one program (the entry and the
files it imports; never across programs, never in std or packages). Two runs are equal when their
token sequences are equal after (a) each local variable and parameter name is replaced by its position
of first appearance in the run, and (b) each literal is replaced by its kind (int, float, string,
bool). Call targets, field names, case names, types and operators are kept. Runs of one block are never compared with each other (a block that repeats a pattern, `let a =
field(x, "a"); let b = field(x, "b"); ...`, is not a copy; found by the corpus check, T024); overlapping
equal windows merge into maximal runs; each later maximal run is reported once, at its first statement, with the
earliest equal run as the related location.

**Measured** (approximate, normalizing all lowercase names, so an upper bound): at 6 statements,
examples 3 groups, all between `url-check/main.vo` and `url-check/sequential.vo`, two separate programs
(never compared, by the per-program rule); std 3 groups in `std.json`'s `as_text`, `as_int`, ...,
which differ in case names and so do not match under the real normalization; agents: ledger
`parse_nordbank` and `parse_cardco`, a real duplicate. Golden cases: 85 groups across functions at an
upper bound; the exact count is taken during implementation against SC-002's 5% limit (only `vo check`
cases can change, see R8).

**Rationale**: renaming locals and literals catches copy-paste-edit, the common case; keeping calls,
fields and case names keeps code that does different things apart. 6 statements is below the size of
a typical copied parsing loop (the ledger's is 9) and above two- or three-line idioms.

**Effect-only copies**: when the two runs are the whole bodies of two functions whose signatures are
also equal after erasing their `uses` clauses and the effect arguments of their parameter and return
types, the message recommends one function with an effect parameter (`[E: effects]`).

**Alternatives**: token-window clone detection across arbitrary spans (finds sub-statement clones,
noisier, harder to explain); hashing whole functions only (misses copied blocks).

## R6 Tests next to the code: W0705

**Decision**: a test or property whose calls to program functions (not builtins, std or packages) all
go to functions declared in one other file of the program, at least one call, gets W0705 at its name,
naming that file.

**Rationale**: precise and cheap from the call graph the checker already has (`call->c.n` gives the
declaration and its file). It only makes sense once `vo test main.vo` runs the other files' tests (R7).

## R7 `vo test ENTRY` runs every program file's tests (FR-014)

**Decision**: `cmd_test` runs the tests of every file the entries load that is not std or a package, each
file once, in load order (the entry first, then its imports as loaded); `--list`, `--coverage`,
`--mutate`, `--test`, `--spec` and `--diff` work on that set. `vo test .` keeps its meaning (every file
under the directory, each an entry), and with the de-duplication a file is never run twice.

**Measured**: `vo test main.vo` with tests in an imported `money.vo` runs only `main.vo`'s today; `vo test
.` and `vo test main.vo money.vo` run both. Both experiment agents put everything in `main.vo`; the
second named this as the reason.

**Impact**: golden `test` cases of multi-file programs whose imported files have tests change (counted
during implementation).

## R8 Where the rules run

**Decision**: in `vo check` only (and so in editors through it), after a check with no errors. Not in
`vo run` (which shows no warnings today), and not in `vo test` (whose warnings are printed before the
tests: structure advice is not about a test run).

**Rationale**: keeps every `run` and `test` golden case unchanged; agents run `vo check` in their loop.

## R9 Accepting a warning: `@allow("W07NN", "reason")` (FR-013)

**Decision**: the annotation syntax of spec 012 (`@spec`) with a second name, `allow`: two string
arguments, a structure code and a non-empty reason, on the line right above a `fn`, `test` or `prop`.
It silences that code for that declaration; for W0702 (file size), an `@allow` on the file's first
declaration. Errors: an unknown code or one that is not W0701–W0705, a missing or empty reason, or a
wrong number of arguments is the new E0121 `invalid_allow` (E0118 is `invalid_spec_id` and is not
redefined); `@allow` anywhere but above a `fn`, `test` or `prop` is E0117; E0116's message and fix learn
the second name. An `@allow` that silences nothing is W0706 with the fix that removes its line. `vo check`'s
summary counts accepted warnings: text `0 errors, 0 warnings (2 accepted)`, JSON `"accepted": 2`.

**Uses in this repository**: `std/json.vo` (W0702 on its first declaration: "one module per import
path"; W0703 on `parse_number`: "one scanner, read top to bottom"). Checked by running the rules on
std in the test suite (FR-012), since std is never reported to programs.

**Alternatives**: suppression comments (`# vo: allow`), rejected: comments carry no meaning in vo and
`vo fmt` would have to keep them attached; a project file (rejected in clarification).

## R10 Fixes

**Decision**: W0706 carries a fix (remove the annotation line). W0701–W0705 carry precise instructions
and related locations, no fix: moving statements into a new function needs an extract-function edit,
which `vo edit` does not have (a later feature). This satisfies FR-011: no single unambiguous mechanical
correction exists.

## R11 Determinism and cost

**Decision**: the rules walk each function once (statement and effect counts), and once more for
duplicate windows with a hash table keyed by the normalized run (FNV-1a over the token kinds and
normalized names), confirmed by comparing tokens. Diagnostics are sorted like all others. Expected cost:
linear in statements, under 10% of `vo check` on the largest example and a 5,000-line generated program
(SC-004, measured in `tests/structure_speed.py`).

## R12 Codes

| Code | Name | Rule |
|---|---|---|
| W0701 | `effectful_logic` | R3 |
| W0702 | `file_too_long` | R1 |
| W0703 | `function_too_long` | R2 |
| W0704 | `duplicate_code` | R5 |
| W0705 | `test_elsewhere` | R6 |
| W0706 | `unused_allow` | R9 |
| E0121 | `invalid_allow` | R9 |
