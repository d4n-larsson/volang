# Research: Readable Signatures and Named Effect Sets (spec 030)

Starting point (read from the code): an effect clause is an `N_USES` node of `N_EFFECT`s parsed by
`parse_effect` (`src/parse.c`); a capitalized name is an effect parameter (`N_EFFECT` with C = name).
`fx_build_set` (`src/check_effects.c`) turns a clause into an `EffSet` (mask, scopes, evars, any), reporting
E0509 for an unknown name and E0506/E0512 for bad scopes; a type's effect argument goes through the same
function and then marks its bits "any scope" (spec 026, E0327 for written scopes). Everything downstream —
the effects record, E0501/E0503/W0501/E0504, `vo effects`, `vo doc` (`doc_signature` renders
`fx->declared`), `vo query` — works on the resulting `EffSet`, never on the clause's text, except the fixes
of E0501/E0503/W0501 (`fx_fix_clause`), which rewrite the clause from a rendered set. `vo fmt`
(`src/fmt.c`) prints item headers on one line and, since the previous change, breaks statements over 100
characters. Reference: ~13,825 tokens of 15,000.

## R1 Syntax: `effects Name = ...;`

**Decision**: a top-level item `effects Name = e1, e2, ...;` (optionally `export effects ...`), where `Name`
is capitalized (a type-like identifier) and each `e` is written as in a `uses` clause: an effect with or
without scopes, or another set's name. `effects` is a word only at the start of an item (no new reserved
word: programs using `effects` as a variable keep working). One declaration per name; an empty list is a
syntax error (E0110, "expected an effect").

**Rationale**: the form of a `uses` clause, so nothing new is learned for the effects themselves; the
`name = value;` shape matches `let`. Capitalized names cannot collide with effects (lower case) and read
like effect parameters, which they behave like.

**Alternatives**: `uses Storage = ...` (reads like a clause, not a declaration); a record-like block
`effects Storage { ... }` (two forms for a list); lower-case names (collide with effects, E0509 ambiguity).

## R2 Resolution: names expand to plain effects when a clause is built

**Decision**: in `fx_build_set`, a capitalized name is an effect parameter in scope or a set in scope
(declared in the file or imported), whose expanded `EffSet` is unioned in (they cannot share a name: E0203
when a type parameter list declares one, since vo has no shadowing); else E0509 with
"neither an effect parameter nor an effect set" and the nearest known name as a fix. Each set is expanded
once, when declarations are resolved (before function bodies), with a depth-first walk that reports a cycle
as E0513 naming the chain (`Storage -> Files -> Storage`). Sets are symbols of a new kind in the item scope:
exported and imported like records (`import notes.Storage;`), renamed by `vo edit rename`, listed by `vo
query` references.

**Rationale**: expanding at the one place clauses become sets means every consumer (records, diagnostics,
doc, query, the runtime grant) sees exactly the same effects as if they were written out (FR-011, FR-012,
SC-003) with no other change.

## R3 Sets in types

**Decision**: in a function type or a type's effect argument, a set expands like any name; for an effect
argument its scopes are dropped silently (the bits become "any scope", R2 of spec 026), since the set was
written for declarations and types name no scopes. Written scopes in a type stay E0327.

## R4 Diagnostics

**Decision**: E0509 (unknown effect) gains the set case and the nearest-name fix over effects, parameters
and sets; new E0513 `effect_set_cycle`; new W0505 `unused_effect_set` (a set that is not exported and that no clause
of the program uses; fix: remove the declaration); clashes with another item's name are E0202 as for any item; invalid effects
inside a set are the existing E0506/E0509/E0512 at the set's declaration, once.

## R5 Fixes and unused effects next to sets

**Decision**: when a clause names a set, the E0501/E0503 fix rewrites the clause as its written entries plus
the missing effects, in R6's canonical order (`uses console, Storage`), instead of rewriting it from the
expanded set (which would erase the name); a clause without set names is fixed as today. W0501 (declared but not needed) reports only effects
written directly in the clause: a set is a deliberate group, so a function that only reads with `uses
Storage` gets no warning for the set's `file.write`. A set none of whose effects a function needs is still
reported (the name, with a fix removing it from the clause).

**Rationale**: FR-013; and without the W0501 rule every set would warn in half the functions using it, which
would push programs back to writing effects out.

## R6 Formatting effect clauses

**Decision**: `vo fmt` keeps set names as written, after the plain effects (which keep their canonical
order), set names in alphabetical order: `uses console, file.read("logs/"), Network, Storage`.

## R7 Signature layout

**Decision**: an item header (function, test, property) longer than 100 characters is printed:
1. with its effect clause on the next line, indented one level, followed by ` {`;
2. if the header without its clause is still over 100 characters: `fn name[T](` then each parameter on its
   own line one level deeper (comma after all but the last), then `) -> Type` on its own line, then the
   effect clause by rule 1 if that line is still too long.
Function types are never broken (a parameter whose type is long goes on its own line by rule 2). An
anonymous function's header (never a `uses`, E0507) follows rule 2 inside the statement fixpoint of the
previous change. Comments between parameters are kept on their own lines.

**Rationale**: the clause first, because that is what makes most headers long (the reported problem);
parameters next, as in the statement rule (one item per line, closing bracket on its own line).

## R8 Records and reports stay exact

**Decision**: nothing changes in the effects record's format or content (expanded effects, R2), so records
written before stay valid and a set's definition change shows up as E0504/W0502 for each function, as an
edit of each clause would. `vo doc` renders the clause as written, then the expanded effects in parentheses
when it names a set (`uses Storage (file.read("data/"), file.write("data/"))`); `vo query` lists expanded
effects. `vo doc` of a module lists its exported sets as items (`effects Storage =
file.read, file.write`).

## R9 Documentation and examples

**Decision**: reference (Effects section: one paragraph for sets; Command line: the header rule, ≤ 150
tokens together), catalog (E0509 updated, E0513, W0505), guide (effects chapter: when to name a set),
examples: notes-api (`Storage`), bench/web reference (10 operations, `Storage`), every example formatted.

## R10 Verification

- Golden cases `tests/cases/effect_sets/` (each acceptance scenario, each diagnostic), `tests/cases/fmt/`
  (signatures: clause-only, parameters, function-type parameter, anonymous function, test header, comments).
- `tests/effect_sets_records.py` (SC-003): for each example and a generated corpus, replacing repeated
  clauses by sets leaves `vo.effects` byte-identical and behavior unchanged.
- `fmt_corpus.py` (idempotence, behavior), `migrate.py`, `doc_conformance.py`, `site_examples.py`.
- SC-004: benchmark v2's effect-related tasks with isolated agents before and after (run scheduled with the
  user; the kit exists).
