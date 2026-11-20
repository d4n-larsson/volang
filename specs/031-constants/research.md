# Research: Constants (spec 031)

Starting point (read from the code): a file's items are parsed by `parse_file` (`src/parse.c`); an item
kind that is not a reserved word already exists (`effects`, recognized by its text at the start of an
item). `check_items` (`src/check.c`) binds imports, defines every item's symbol (`define_item`, kinds
`SYM_FN`, `SYM_RECORD`, `SYM_VARIANT`, `SYM_EFFECTS`), expands effect sets, resolves signatures, then
checks bodies. An identifier in an expression is resolved by `check_ident_as` (`src/check_expr.c`), which
also logs the reference for `vo query` (`q_note_*`). The compiler (`src/compile.c`) emits one `OPC_LOADK`
for a literal node (`N_INT`, `N_FLOAT`, `N_STR`, `N_BOOL`) and reads a local from its slot.

Every place where the checker reads a literal tests the node's kind: `N_STR` in `check_format.c`,
`check_regex.c`, `check_sqlite.c`, `check_web.c` and `fx_literal_at` (`check_effects.c`, the E0503 target
check); annotation arguments in `check_annot.c`. Effect scopes and annotation arguments are parsed as
string tokens only (`parse_effect` and the annotation loop in `parse.c`). Everything about effects
downstream of a clause (the record, E0501/E0503/E0504, `vo effects`, `vo doc`, `vo query`) works on the
`EffSet` built from the clause, never on its text.

Reference: about 14,017 tokens of 15,000. Codes in use: E0101–E0120, E0201–E0211, E0301–E0331, W0201.

## R1 Syntax: `const name = expr;`, a word only where an item starts

**Decision**: a top-level item `const NAME = EXPR;` or `const NAME: TYPE = EXPR;`, optionally after
`export`. `const` is recognized by its text at the start of an item, as `effects` is: it is not added to
the keyword table. `NAME` is `snake_case`. The item is a new node kind `N_CONST` (A name, B type node or
0, C the expression).

**Rationale**: the shape of `let name = expr;` and `let name: type = expr;`, which agents already write.
Not reserving the word means no program that checks today can change (the spec's edge case said
"reserved"; this is stricter on compatibility and the spec is updated). At the start of an item nothing
else can begin with an identifier, so there is no ambiguity.

**Alternatives**: reserving `const` (breaks any program using the name, for nothing); `let` at the top
level (reads like a variable, and `var` would be expected to follow); `def`/`val` (new words with no
precedent in vo).

## R2 A constant is folded into the tree where it is used

**Decision**: when the checker resolves an identifier that names a constant, it logs the reference and
then rewrites the node in place into the literal node of the constant's value (`N_INT`, `N_FLOAT`, `N_STR`
or `N_BOOL`), keeping the node's span, setting a new flag `NF_CONST` and keeping the constant's declaration
in a field the literal kinds do not use. A constant's own value is folded the same way into its
declaration, so a symbol's value is its declaration's expression node.

**Rationale**: every consumer downstream already handles literals. The compiler emits `OPC_LOADK` (no new
instruction, no slot, no run-time cost: SC-006 holds by construction); `check_format`, `check_regex`,
`check_sqlite`, `check_web` and `fx_literal_at` see `N_STR` and work unchanged once the fold has run; an
`EffSet` built from a clause holds the value, so the effects record, E0504 and every report show values
without a change (FR-016). Diagnostics quote the source by span, so they still show the name.

**Alternatives**: a `SYM_CONST` read at run time from a table (a new instruction, a global area, and every
literal position needs its own lookup); textual substitution before parsing (spans and `vo query` break).

## R3 What a value may be, and how it is worked out

**Decision**: a new file `src/check_const.c` with two entry points.

- `const_eval(decl)`: checks the declaration's expression with the ordinary expression checker in a
  "constant" mode, then folds it bottom-up. In that mode a call, a builtin, a hole, `new`, a list, an
  index, a field, `try`, an anonymous function, or a name that is not a constant is E0212 at that node.
  Operators allowed: `+ - * / %`, unary `-`, the six comparisons, `and`, `or`, `not`, parentheses. A stated
  type is checked with `expect_type` (E0301), as for `let`. A value whose type is not `int`, `float`,
  `bool` or `str` is E0332.
- `const_fold(node)`: folds a subtree whose leaves are all literals (folded constants included) and
  returns whether it became one literal. Used at literal positions (R5) and for E0503 targets.

Folding uses the VM's own arithmetic: the overflow builtins and the `div`/`rem` rules of `src/vm.c` for
`int` (moved to shared inline helpers in `vo.h` so the two cannot drift), C `double` operations for
`float` (the build already fixes `-ffp-contract=off`, so results are bit-identical to the VM on all three
platforms), `str_cmp` and `values_equal` for comparisons, and a concatenation allocated in the permanent
arena for `str +`. Overflow, `MIN / -1` and division or remainder by zero are E0333 at the operator in a
constant's value; `const_fold` at an ordinary expression simply does not fold them (the run reports
R0001/R0002 as today).

**Rationale**: FR-004 asks for the run-time results exactly; sharing the helpers is the only way to keep
that true as the VM changes. Checking with the ordinary checker first means types, E0301 and operand
errors (E0308) are the existing ones.

**Alternatives**: running the constant's expression on the VM at check time (drags the runtime into `vo
check`, and effects and time limits with it); allowing pure builtins (`len`, `str`, `format`): the rule
"which builtins are pure enough" is something to learn, and `str(float)` in a constant would make the
checker's output depend on float formatting; a later spec can add them.

## R4 Order of evaluation, cycles, files

**Decision**: constants are evaluated on demand with a three-state mark on the declaration (not started,
in progress, done). `check_items` runs a pass over the file's constants after `define_item` and before
`fx_expand_sets`, so effect sets and clauses can use them. Reaching a constant that is in progress is
E0213 at the first constant of the cycle, with the path (`a` → `b` → `a`) and related notes at the others;
every constant on the cycle becomes an error value so its uses report nothing more (FR-010). An imported
constant is already evaluated: files are checked in dependency order and import cycles are E0206. The
recursion is bounded by the number of constants; the fold walks expressions with the parser's nesting
bound (5,000), as the rest of the checker does.

**Rationale**: items may be used before they are declared, so a single ordered pass is not enough;
on-demand evaluation with marks is the same scheme `fx_set_of` uses for effect sets (E0513).

## R5 Literal positions take a constant expression

**Decision**: one rule for every position where the checker reads a value: it takes a constant
expression, that is a literal, a constant, or operators over them (R3).

| Position | Change |
|---|---|
| effect scope (`uses`, test clauses, `effects`) | `parse_effect` parses an expression where it required a string token; `fx_build_set` folds it; not a `str` constant expression: E0506 (the existing bad-scope code) with the new wording |
| field annotations `@size`, `@range`, `@pattern`, `@default`, `@header` | the annotation's arguments are parsed as expressions and folded before `check_annot.c` reads them (E0329 unchanged) |
| `format` template, `regex`, `sql`, `std.web` route patterns | the argument is folded before the kind test; E0315, E0321, E0324 and E0330 keep their codes, and their messages say "a literal or a constant" |
| E0503 target (`write_file(out_dir + "x", ...)`) | `fx_literal_at` folds the argument first |
| `@spec`, `@allow` | unchanged: string literals only (spec, Assumptions) |

A problem inside a value that came from a constant is reported on the whole use, with the value in the
message (FR-015); position-exact spans and fixes inside the text are given only when the node is a
written literal (`NF_CONST` clear and no fold happened), because offsets inside a folded value do not map
to source characters.

**Rationale**: "a literal or a constant expression" is one sentence in the reference; a list of places
that take names but not `a + b` would be a rule to learn. The kind tests stay where they are.

**Alternatives**: constants only, no operators (then `out_dir + "report.txt"` as an E0503 target needs a
second rule); scopes and annotations only (the choice offered in the spec review, not taken).

## R6 Names, duplicates, shadowing, unused

**Decision**: `define_item` gives a constant a symbol of a new kind `SYM_CONST` through the path items
already take, so E0202 (duplicate, builtin name), E0207 (naming, with `snake_case` as the fix: the
existing lower-case rule) and E0203 (a local with the name of something visible) need no new code. `set
max_items = 5;` is E0208 with a third wording next to the `let` and parameter ones ("`max_items` is a
constant and cannot be changed."), no fix. A constant that is not exported and has no reference in the
program is W0202 `unused_constant`, found from the reference log after the file's bodies are checked, with
a fix that removes the declaration and its comment (as W0505 does for sets). A reference from a scope, an
annotation or another constant counts.

## R7 `const` inside a function

**Decision**: a statement that starts with the identifier `const` followed by an identifier is E0122
`const_in_function`, with the fix that replaces `const` by `let`. The parser then reads the rest as a
`let`, so the body is checked without a cascade.

**Rationale**: an agent that knows `const` from other languages will write it there; the fix is the one
correction. A call statement `const(x);` of a user function named `const` still parses as a call.

## R8 Import and export

**Decision**: nothing new. `bind_imports` copies the target file's symbol by name whatever its kind;
`check_exported` (E0209 and its `export` fix), E0204's sibling-file search (`file_defines`, which reads
tokens: it learns the `const NAME` shape) and `export` parsing (`NF_EXPORT`) apply. E0210 (an exported
item using a private type) cannot arise: a constant's type is a builtin type.

## R9 Tools

**Decision**:

- `vo fmt` (`src/fmt.c`): `const name = expr;` / `const name: type = expr;` after an optional `export `,
  expression printed by the existing expression printer and broken by the statement rule over 100
  characters; blank-line rule as for other items (consecutive constants stay together without a blank line,
  like imports and effect sets). The formatter works on the parsed tree, before any fold, so it prints
  names. Scopes in a clause are sorted by their source text; a quoted literal sorts before a name.
- `vo query`: `N_CONST` joins the item kinds (`QK_ITEMS`); references are logged before the fold, so
  `refs` and `edit rename` work as for functions, including uses in scopes and annotations. `type-at`
  on a use or a declaration adds the value (text: `max_items: int = 100 (declared at ...)`; JSON: an
  optional `value` field). `callers` on a constant is T0004-style "not a function" with the existing code
  for a target of the wrong kind.
- `vo impact`: a diff line inside a constant's declaration marks the functions and tests that reference
  it, directly or through other constants, from the same reference log.
- `vo doc`: constants are listed with `const name: type = value` and their comment (`item: "const"` in
  JSON), for programs, packages and the standard library's own files.
- Reports with operand values (a failing `expect`, `expect_eq`): an operand that is a folded constant is
  listed with its name and value, where a written literal is left out as today.
- `vo test --mutate`: a node with `NF_CONST` is not a mutation site (`src/mutate.c` tests the flag).
- W0603 (a check that cannot depend on the program): a check made only of literals and constants still
  gets it; constants are literals to that rule.

## R10 Effects review

**Decision**: no change to the record or to E0504/W0502: they compare `EffSet`s, which hold values. One
addition: when a function's scope changed and the clause's scope node has `NF_CONST`, the diagnostic gets
a related note at the constant's declaration ("`out_dir` is `"./"` here"), so the reviewer sees why a
function whose text did not change gained a capability.

## R11 Diagnostics

**Decision**: five new codes and one new warning, each with a catalog entry and golden cases; four
existing messages reworded. Exact texts and fixes are in [contracts/diagnostics.md](contracts/diagnostics.md).

| Code | Name | Fix |
|---|---|---|
| E0122 | `const_in_function` | `let` |
| E0212 | `const_not_constant` | none |
| E0213 | `const_cycle` | none |
| E0332 | `const_type` | none |
| E0333 | `const_arithmetic` | none |
| W0202 | `unused_constant` | removes the declaration |

Reused: E0208 (assignment), E0202, E0203, E0207, E0204, E0209, E0301, E0506, E0329. Reworded: E0315,
E0321, E0324, E0330.

## R12 Documentation budget and examples

**Decision**: the reference gets a "Constants" subsection under Declarations of at most 350 tokens with one
runnable example, one sentence in Effects (scopes), one in Records (annotations), one clause in the
`format` paragraph, and "no global variables" replaces "no global variables or constants": at most 450
tokens in all, leaving about 500 under the limit. The guide gets a section in its declarations chapter and
a paragraph in the effects chapter; the catalog gets six entries.

Examples rewritten (FR-022): each example whose `main` declares a directory scope and builds paths under
it (`expenses`, `notes-api`, `helpdesk`, `release-check`, `status-board`) gets a `const` for the directory;
repeated limits and messages where they occur three or more times. `tests/const_records.py` checks that
every example's `vo.effects` and output are byte-identical before and after (SC-001, SC-003).

## R13 Tests

**Decision**: golden cases in `tests/cases/const/` (one per acceptance scenario and per diagnostic, written
first and failing), `tests/cases/fmt/const*`, `tests/cases/query/const*`, multi-file cases under
`tests/cases/const/`. `tests/const_equiv.py` generates 1,000 programs that use literals at every position
of R5 and in expressions, and the same programs with each literal replaced by a constant (or a constant
expression) of the same value, and compares `vo check --json`, `vo run` and `vo effects` byte for byte
(SC-005). The existing suites give SC-002: corpus, examples, `gc/baseline.py` (731 reference runs),
`fmt_corpus.py`, `doc_conformance.py`, `apply_fixes.py` (fix coverage, SC-004), `structure_speed.py` and
`tests/perf.sh` (SC-006).
