# Research: Variant Cases With Fields, and `match` on Values

Decisions for spec 060, from reading `src/parse.c`, `src/check.c`, `src/check_expr.c`,
`src/check_generic.c`, `src/compile.c`, `src/json_derive.c`, `src/prop.c`, `src/rt_render.c`,
`src/fmt.c`, `src/doc.c`, `src/query.c`, `src/edit.c` and the corpora at commit `30bd2a9`.

## R1. A case with fields carries a hidden record

**Decision**: a case declared with fields (`malformed { path: str; line: int; reason: str; }`) gets a
synthetic record declaration, kept on the case node (`N_CASE_DECL.c`, unused today), named for messages
`LoadError.malformed`, with the variant's type parameters as its own. The case's payload type is that
record. Nothing about the runtime changes: a variant is a tag and one payload word (`vo.h:1157`), so the
case carries a pointer to a record exactly as `rect(Size)` does.

**Rationale**: every type-driven path reads the payload's type and so works unchanged: equality
(`interp.c:356`), hashing for keys (`rt_map.c:65`), ordering (`rt_coll.c:57`, `check_generic.c:58/95`),
JSON (`json_derive.c:105-133`, `494-594`: `{"malformed": {...}}`, the wire format of a record payload),
field limits from annotations (`json_derive.c:416/438`), test diffs (`rt_test.c:223`), property
generation and shrinking (`prop.c:115-142`, `325-345`, `658`). FR-004, FR-005 (JSON) and FR-007 follow.

**Not a file item**: the hidden record hangs off the case, not the file's item list, so fmt, doc, lsp
symbols, the structure rules (W0702) and `q_members_all` do not list it as a record; the places that must
know it are named in R7.

**Alternatives**: a new value kind with its own fields (every path above reimplemented); a real record
generated into the file (every item loop would show it).

## R2. Syntax and the `Type.case {` ambiguity

**Decision**: in a variant, `name { field: T; ... }` declares fields (annotations allowed above a field,
as in records); `name(T);` and `name;` are unchanged. A value is `Variant.case { field: v, ... }`, no
`new`. In an expression, after `Type.case` (or `m.Type.case`) a `{` starts a field literal only when the
next two tokens are a name and `:`; otherwise it is the block after the expression, as in
`if e == tcp.TcpError.closed {` (`std/http.vo:718`, `:837`). A field case has at least two fields, so an
empty `{}` is always a block. In a type, `Type.case` and `Type[args].case` name the fields' type.

## R3. Binding, read-only fields, type name

**Decision**: `case malformed(m)` binds the hidden record; `m.line` reads it (`check_field`). `set m.f`
on a value whose type is a case's record is a new error **E0344 case_field_set** (no fix): case fields
are read-only (FR-003). The type prints as `LoadError.malformed`, a generic one as `Tree[int].node`
(`ty_name_into`, `check.c:184`, special-cased for the hidden record). It cannot be built with `new`
(E0201-style message pointing to `Tree.node { ... }`).

## R4. Printing a value (spec correction)

**Decision**: a case with fields prints in the construction syntax:
`LoadError.malformed { path: "a", line: 3, reason: "x" }` (`rt_render.c:137-165`), in test failures,
traces and runtime frames. FR-005's "prints with the same text" cannot hold: today's text names the
payload record (`Shape.rect(new Size { ... })`), which no longer exists. JSON is unchanged. The spec is
amended accordingly.

## R5. One-field cases: E0216

**Decision**: a case declared with exactly one field is **E0216 one_field_case**. Its fix (one edit,
schema 1) rewrites the declaration to the one-value form, `missing { path: str; }` → `missing(str);`.
Uses written for the field form (`V.missing { path: p }`, `b.path`) then report today's errors at each
use (a payload given in braces; a field read on a `str`), which name the one-value form. The form is new,
so such uses exist only in code written against it.

## R6. `match` on `int` and `str`

**Decision**:
- Parsing: an arm is `case V {` where `V` is an int or string literal (with a leading `-` for ints), a
  constant name, or a module constant `m.name`; `case name(...)` and `case name {` stay as today. A final
  `else { ... }` is a new node kind `N_ELSE_ARM` in the arms vector (no free node flag: `vo.h:357-374`).
- Checking (`check_match`, `check.c:877`): an `int`/`str` scrutinee takes value arms; a name arm resolves
  to a constant of the scrutinee's type (else E0201). Repeats (literal or a constant equal to one) are
  E0402 with today's removal fix. A wrong-typed arm is E0301. No `else`: **E0414 value_match_else**, fix
  inserting `else {\n}` before the match's `}`. An `else` on a variant or `result` match: **E0415
  match_else_exhaustive**, fix removing it. Another scrutinee type: **E0345 match_scrutinee**, naming
  "a variant, a `result`, an `int` or a `str`" (replaces today's E0301 there).
- The paths that assume an arm is a name (`check.c:882`, `979`, `1007`; `fmt.c:793`;
  `check_structure.c:443`) branch on the arm's node kind.
- Compiling (`cmatch`, `compile.c:370`): arms tested in order with existing opcodes (int: `OPC_JNE_K` /
  `OPC_JNE`; str: `OP_EQ` + `OPC_JF`); no new opcode, so the three assembly backends are untouched. Each
  arm and the `else` are coverage points (`COV_CASE`; "else" already a coverage word, `testrun.c:728`).
- Termination: a value match whose arms and `else` all terminate ends its path.

## R7. Tools

- **fmt** (`fm_members` `fmt.c:837`, item printer `:970`, expressions): field cases print like record
  fields, annotations included; value arms and `else`.
- **query/edit**: `q_resolve` (`query.c:322`) and `q_resolve_qualified` (`:384`) accept a third level,
  `Variant.case.field`, keyed `(hidden record, field index)`; declarations noted by `q_members` for the
  hidden records. `ed_add_field` (`edit.c:302`) accepts `Variant.case` and inserts before the closing
  `}` (the record path inserts after the last field's line, wrong for a one-line case: fixed for both).
- **doc**: signatures print `malformed { path: str; line: int; reason: str; }` (`doc.c:191`, `:292`).
- **lsp**: document symbols list a field case's fields as children (`lsp.c:925`).
- **Editors**: tree-sitter (`case_declaration`, `match_arm`, `variant_value`, `qualified_type`), TextMate,
  Vim, Zed, Pygments.

## R8. The chain warning (US3): dropped

**Finding**: no `if`/`else if` chain in `std/` or `examples/` meets FR-012 (one local or parameter
compared with `==` to three or more distinct literals or constants); the near misses compare fields or
mix conditions. SC-002 would rewrite nothing.

**Decision**: US3 and FR-012 dropped (decided with the user, 2026-11-26). The value `match` is the one form for new
code; a warning that fires on no code in the repository is surface without a current requirement
(Principle V, YAGNI). If kept, it is **W0404 value_chain** modeled on W0403 (`check.c:667-702`), with
`@allow` extended (`check.c:1416` accepts only W0701-W0705 and W0506 today).

## R9. Migration of the repository

**Decision**: the two records used only as one case's payload become fields: `std/time.vo` `Repeated`
(`LocalError.repeated`, exported) and `examples/expenses/report.vo` `DateProblem`
(`ExpenseError.bad_date`). JSON is unchanged (neither is serialized). `time.Repeated` disappears from an
exported module: a changelog entry, and an E0214 library-change row with its fix (spec 056's machinery,
`libchange.c`) so programs that named it get a message. `tests/cases/library/error_texts.vo` and
`tests/migrate_056.py`'s expected texts follow.

## R10. Reference budget

**Decision**: the reference is at 11,538 words, its maximum (`tests/ref_examples.py`: words × 1.3 ≤
15,000): the change must be word-neutral. The Variants example drops `Size` (about −4 words); the
Variants paragraph and the `match` details line are rewritten to say both new forms in the words they
use now; if still over, other paragraphs of the reference are tightened, stated in `results.md`.

## R11. Codes and version

New: **E0216** (one_field_case, fixable), **E0344** (case_field_set), **E0345** (match_scrutinee),
**E0414** (value_match_else, fixable), **E0415** (match_else_exhaustive, fixable).
Ships as **0.6.0**.
