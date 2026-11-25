# Research: Consistency Pass

These decisions come from reading:
- `src/loader.c` (imports, E0205);
- `src/check.c` (`new_local`, E0203);
- `src/check_expr.c` (`check_list`, E0306; ordering operators);
- `src/check_generic.c` (`ty_ordered`, E0311);
- `src/rt_coll.c` (`value_cmp`, `record_cmp`);
- `src/lex.c` (E0105);
- `src/fmt.c` (effect arguments);
- `src/tcp.c` (`MS_MAX`);
- the `std/*.vo` modules each change touches.

They also draw on the counts of imports in `examples/`, `tests/cases/`, `std/`, `site/docs/` and
`packages/`.

## R1. One diagnostic for library changes: E0214 `library_changed`

**Decision**: a new error code, E0214 `library_changed`, driven by one table of changes (data-model.md).
- **Import:** an import of a renamed or removed std item reports E0214 at the import. The import is then
  resolved to the new item, so uses check as before and no E0205 or E0201 cascades.
- **Uses:** each use of the old name in that file reports E0214 too. Every report carries a fix: the new
  name; for a removed item, its replacement (`now_ms()`); for a swap, the arguments swapped.
- **Retyped items** (a `result` that became an `Option`, under the same name): E0214 replaces the type
  error at the use. That covers a `match` on `ok`/`err`, `try`, a comparison with `ok(...)`, and an
  expected `result` type. The fix wraps the call: `to_result(CALL, "none")`, adding
  `import std.option.to_result;` when it is missing (one edit, from the imports to the call, as W0506's
  import fix does).
- **The message:** "`std.http.header` was renamed `response_header` in vo 0.2.0." or "... now returns
  `Option[str]`: ...".

**Rationale**: one code that an agent learns once, as the user preferred. The table is the single source
for the diagnostic, its fix, the catalog's list and the changelog's list. Resolving the old name keeps
the rest of the file checking, so one round of fixes migrates it.

**Alternatives considered**:
- One code per change: many codes for one idea.
- A warning that keeps the old names working (deprecation): two names for one thing until a later
  removal, against "one way".

## R2. The new names

**Decision**: the renames of spec FR-004 and FR-013. Checked against every std module, they collide with
nothing, and each follows a neighbour in its own module:
- `close_conn` beside `close_listener`;
- `add_to` beside `set_to`;
- `write` beside `read`;
- `KIND_column` beside json's `KIND_field`.

Two choices go beyond the spec's list:
- `sqlite.is_null` stays: it does not clash.
- `regex.replace_all` becomes `replace`, and `std.text`'s `replace` already exists. Both replace every
  occurrence, and the two clash when imported together. So `regex.replace_all` becomes `replace_matches`
  instead: unique, and plain about what it does.

**Rationale**: SC-001 needs unique names across modules. A test (`tests/std_names.py`, new) lists every
exported item of every module and fails on a repeat.

## R3. Option for lookups, and what stays a result

**Decision**: the 20 lookups of FR-005 return `Option[T]`. Taking an element (`pop`, `pop_front`) and
lookups with two failure modes (`json.member`) stay `result`. The builtin `find` stays `-1`: changing a
builtin's type is a language change for every string search, and its `-1` mirrors byte offsets.

**Rationale**: `std/option.vo`'s own rule ("a lookup that finds nothing"). Collections are the bulk, and
their `err("none")` carried no information.

## R4. Shadowing builtins

**Decision**: `new_local` also reports E0203 when the name is a builtin (`builtin_id`), with the message
"`len` is a builtin: give the local another name." The fix renames the declaration and every use in its
scope to `NAME_value` (`keys_value`), or `NAME_value2` and so on when that is taken. Parameters, `for`
variables and case bindings all go through `new_local`, so all are covered.

**Rationale**: the same code as shadowing the program's own names. `NAME_value` keeps the original word
and is always valid snake_case.

**Measured before planning the tasks**: the corpus (examples, tests/cases, std, site) will be checked
with the new rule to count the names to migrate. The fixes migrate them; std and the examples are
migrated by applying them.

## R5. Order for bool and variants

**Decision**:
- **Checker:** `ty_ordered_d` accepts `TY_BOOL`; `TY_VARIANT` when every case's payload type is ordered
  (enum-like variants always); and a `TY_PARAM` bounded `key` (every key type is now ordered).
- **Run time:** `value_cmp` compares variants by case index, then by payload (`value_cmp` on the payload
  type), the way `record_cmp` goes field by field. `bool` already compares as 0 and 1 in the default
  branch.
- **Ordering operators** (`<` and the others) on these types take the path records take today (the
  `value_cmp` instruction).
- E0311's message for a `key`-bounded parameter (added in group 1) goes away, since such a parameter is
  now ordered.

**Rationale**: no new syntax and no bound arithmetic. `key` becomes a subset of `ordered`, so the
missing `[K: key + ordered]` is never needed.

**Alternatives considered**: two bounds on a parameter (new syntax, a reference cost; declined by the
user).

## R6. Time limits

**Decision**: every `timeout_ms` (and `with_deadline`'s `ms`) takes 0 to 600,000, and 0 means 600,000.
- **Out of range:** `err("invalid: the time limit must be 0 to 600000 ms")` in tcp's form, the same text
  everywhere. For `with_deadline`, which returns `WorkerError`: `err(WorkerError.invalid(...))`. Its
  error variant gains that case if it lacks it.
- **`http.send`:** its 0 meant "only the run's `--timeout`" and now means 600,000. A program that relied
  on unbounded HTTP requests under `--timeout 0` gets a 10-minute bound; the changelog says so.
- **Renamed case:** `WorkerError.timeout` becomes `timed_out`, through the E0214 table. A case rename has
  its fix at each `case timeout` and `WorkerError.timeout`.

**Rationale**: tcp's range and send_stream's meaning of 0 together cover 3 of the 4 modules. An
unbounded wait is what an agent least expects from 0 in a service built with `--timeout 0`.

## R7. Units

**Decision**:
- **Functions:** functions take and give milliseconds, as `std/time.vo`'s header already says.
  `fixed_zone(east)` changes from minutes to milliseconds, and its fix multiplies a literal argument
  (`fixed_zone(330)` becomes `fixed_zone(19800000)`) or wraps another expression (`minutes(EXPR)`).
- **Names:** every duration a program sets or reads in a record or parameter is milliseconds with
  `_ms`: `lifetime_s` → `lifetime_ms`, `max_age_s` → `max_age_ms`, `hsts_seconds` → `hsts_ms`. Field
  renames go through E0214. The fix of a record literal's field converts a literal value (`* 1000`) or
  wraps the expression in `seconds(...)`.
- **tz records stay in seconds** (`Zone`, `ZoneRule`, `RuleDay`). They mirror the tz database and are
  filled by `std.time` from its data, not by programs. Their duration fields get the `_s` suffix, so the
  unit is in the name. This narrows spec FR-009: updated in the spec.

**Rationale**: the rule that matters to an agent is "a function takes milliseconds, and a name says its
unit". Converting the tz tables to milliseconds is work with no reader.

## R8. Argument order and names in std.regex

**Decision**: the text comes first: `is_match(s, re)`, `first_match(s, re)`, `all_matches(s, re)`,
`split_on(s, re)` and `replace_matches(s, re, replacement)`. The swap is an E0214 change whose fix
swaps the two arguments (their types differ, so the old order is always detectable).

## R9. Parse errors

**Decision**:
- **Values:** `err("invalid KIND: INPUT")`, with `"; DETAIL"` when the parser knows more. This covers
  int, float, decimal, time, hex, bits and base64. bits and bytes share the hex detail text.
- **Documents:** `err("invalid KIND at POSITION: DETAIL")` for json (`line L, column C`), regex
  (`byte N`) and sql.
- **`from_` stays for decoding** an encoding into bytes (`bytes.from_hex`, `from_base64`); `parse_` is
  for reading a value. The reference states both rules.
- **No diagnostic:** error texts are values, so a program comparing them gets no fix. The changelog lists
  every changed text.

## R10. Typed reads

**Decision**:
- **Names:** sqlite's typed reads are `column`, `int_column`, `float_column`, `text_column` and
  `bytes_column`.
- **Integers as floats:** `float_column` converts an INTEGER, as `json.as_float` and `float_field`
  convert an integer.
- **Missing names:** they read ``no column `NAME` `` (backticks, as json's ``no field `k` ``).

## R11. Duplicates and resources

**Decision**:
- `web.clock_ms` and `time.now` are removed through E0214: a call becomes `now_ms()`, a value
  `fn() -> int { return now_ms(); }`.
- `web.log_requests` stays.
- `tcp.close_conn` and `close_listener` drop their effects (`uses E`, `uses serve`).
- sqlite's messages lose `error: `.
- `tcp_error_text`, `worker_error_text` and `local_error_text` give one sentence per case.

## R12. Language corners

**Decision**:
- **`[]`:** `check_list` returns the expected list type for `[]` when there is one, without E0306. E0306
  stays where no type is expected.
- **`Shape.empty()`:** reported as E0411, with a fix removing `()`, as `case empty()` is. This needs a
  parser change: `S.empty()` parses as a variant case with an empty argument list, and is reported at
  check time.
- **Literals:** each non-canonical spelling is E0105 with a fix giving the canonical one, as `0X1F` is
  already E0105 with a fix:
  - `1E9` and `1e+9` give `1e9`;
  - leading zeros (`00012`) give `12`;
  - a nonzero float literal that rounds to 0.0 (`1e-400`) is E0105 "out of range", as `1e400`;
  - `-0x8000000000000000` is E0105, with the message pointing at `0x8000000000000000`.

  This replaces the spec's "accept and let `vo fmt` lower" (FR-020): updated in the spec. vo's rule is
  one spelling per construct, which the hex case already follows.
- **Effect arguments:** `vo fmt` writes every effect argument with `uses`. The checker accepts a named
  set without `uses` (`Db[Storage]`), as it accepts `Db[D]`.

## R13. Message style

**Decision**: the rules of FR-022, applied to every message in `src/`. A test (`tests/message_style.py`,
new) greps the message templates for:
- "got" in place of "found";
- unbackticked `\n`, `@size` and number examples;
- ranges written `A-B`;
- `Test `NAME`` / `test named "NAME"` instead of `` `test "NAME"` ``.

Changes to message texts are not breaking (`docs/stability.md`: codes, spans and fixes are the
interface). E0119 covers repeated `@size` (E0329 then reports only its other causes) and repeated
`@allow`. The base toolchain that cannot be read is T0002.

## R14. Reference cost

**Decision**: the changes touch these reference lines:
- the ordered-types line (`bool` and variants);
- the `std.collections`, `std.regex`, `std.lists`, `std.sqlite`, `std.http`, `std.web` and `std.tcp`
  rows;
- the `[]` rule;
- a short `from_`/`parse_` rule.

Most renames are same-length word swaps. The estimate is +30 to +50 tokens against 13 left. The
additions are written first and measured, then text the guide already has moves to the site, as specs
055 and the audit did. Candidates are std detail rows that `vo doc` prints anyway. The results state
the numbers.

## R15. Version and migration test

**Decision**:
- **Version:** the toolchain becomes 0.2.0 (`vo version`), and the changelog lists every change with its
  migration.
- **Migration test:** `tests/migrate_056.py` (new):
  - copies every example and package as of the commit before this spec (8c8db3e);
  - applies the first fix of every diagnostic (at most 3 rounds);
  - requires a clean check, and test results and outputs equal to the recorded ones.
- **v1 corpus:** `tests/migrate.py` keeps running the frozen v1 corpus, extended to 3 rounds and to the
  E0214 code.
