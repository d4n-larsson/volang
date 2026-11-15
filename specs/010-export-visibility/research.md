# Research: Visibility with `export`

## R1: The keyword and the item

- **Decision:** `export` is keyword token 47 (`T_KW_LAST` 47, still below punctuation at 50). `parse_item`
  reads `export`, then requires `fn`, `record` or `variant`, parses that item and sets `NF_EXPORT` (512)
  on it; the item's span starts at `export`. Anything else after `export` is the usual syntax error
  ("Expected `fn`, `record` or `variant` after `export`"); `export` inside a block or a record body is
  the existing unexpected-token error of that position.
- **Rationale:** a flag on the existing item nodes changes nothing for code that ignores visibility
  (checker, compiler, effects, queries, edits work on names); the first-token rule holds.
- **Alternatives:** a separate `export name;` list (two places to read; rejected, Principle I);
  capitalization or `_` prefixes (names are already constrained by convention; rejected).

## R2: E0209 at the import

- **Decision:** `bind_imports` finds the target item as today; if it lacks `NF_EXPORT`, E0209 at the
  imported name: "`money` is not exported by `report.vo`." with a fix inserting `export ` at the
  declaration's start (a fix may edit another file: `{file, start, end, replacement}`). For a library file
  ("`parse_value` is internal to `std.json`.") or a package file ("... internal to package `money`.")
  there is no fix. The import is still bound, so uses of the name do not cascade.
- **Rationale:** the error is at the line that crosses the boundary; the fix goes where the decision is
  made (the declaring file), which is the one unambiguous correction for a program file.

## R3: E0210 for private types in exported items

- **Decision:** after `resolve_item`, for every exported item walk its type nodes (parameters, result,
  fields, case payloads; through `list`, `map`, `result`, function types and type arguments). A named type
  resolving to a record or variant declared in the same file without `NF_EXPORT` is E0210 at the type
  name: "`export fn load` uses `Url`, which is not exported." with the fix exporting `Url`.
- **Rationale:** an importer could receive values of a type it cannot name; exporting the type is the
  one correction (making the function private is a design decision, not mechanical).

## R4: One diagnostic per declaration

- **Decision:** during one check, the first E0209/E0210 for a declaration carries the fix; any later one
  for the same declaration is not a new diagnostic but a related note ("also imported here") on the first.
- **Rationale:** the agent loop applies the first fix of every diagnostic; two identical insertions would
  produce `export export fn`. One diagnostic per missing `export` is also the honest count of problems.

## R5: The standard library exports its documented items

- **Decision:** `export` is added to the 48 items `vo doc` lists today (those with a doc comment); the 33
  undocumented helpers stay private. `tests/doc_conformance.py` checks that exported and documented
  coincide for `std/`.

## R6: `vo doc` lists exported items

- **Decision:** `doc_file` lists items with `NF_EXPORT` (instead of items with a doc comment); an exported
  package item without a doc comment is listed with an empty description. For `std/` the two sets are
  equal (R5), so `vo doc` output for the library does not change.

## R7: Migrating the repository

- **Decision:** a one-off script (not shipped, not kept) adds `export ` before every declaration named by
  an import of a sibling or package file anywhere in the repository (`bench2/`, `tests/`, `examples/`),
  the same edit as E0209's fix, then every entry program is checked to have no E0209/E0210. Golden outputs
  whose spans sit on a migrated declaration line move by 7 columns; each is re-accepted only after its diff
  is shown to be exactly that. Python tools that write packages (`packages_check.py`, `fuzz_inputs.py`) and
  the reference's multi-file example gain `export`. `tests/speed.py --bench2` strips `export ` for the
  assembly oracle, which predates the keyword.
- **Rationale:** the user chose the immediate rule (spec clarification); applying the fix's own edit keeps
  the migration mechanical and reviewable (`git diff` shows only `export `).

## R8: The IntelliJ plugin

- **Decision:** `export` joins `VoTokens.KEYWORDS`; a lexer test covers it; version 0.1.4.
