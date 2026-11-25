# Feature Specification: Consistency Pass

**Feature Branch**: `056-consistency-pass`

**Created**: 2026-11-25

**Status**: Implemented (2026-11-25, vo 0.2.0)

**Input**: User description: group 3 of the inconsistency audit (`docs/review-2026-11/inconsistencies.md`,
section 3): the design inconsistencies of the language and the standard library, fixed as breaking changes
before 1.0, each with a diagnostic and a mechanical fix so that applying `vo check`'s fixes migrates a
program (`docs/stability.md`).

Background: the audit of 2026-11-25 found 45 inconsistencies. Groups 1 (bugs) and 2 (docs) are fixed.
Group 3 is what needs a decision: the same idea done two ways in the library, names that collide, and
corners of the language that treat similar things differently. Each one costs an agent writing vo a
wrong guess and a round trip through the checker, and a reviewer a second look.

Decided with the user on 2026-11-25:
1. **Name clashes** between std modules are resolved by renaming one side of each pair. There is no
   `import ... as`.
2. **"Nothing found"** lookups return `Option`, as `std/option.vo` already says. The builtin `find` keeps
   `-1`, and so does `lists.index_of` (decided in implementation: its result is used as a number, so no fix
   could keep a program's behavior).
3. **Builtins cannot be shadowed** by locals or parameters, as the program's own functions cannot.
4. **`bool` and variants become ordered**, so every map key type is ordered and a generic `K: key` can be
   sorted.

The rest of section 3 is decided here, following the convention most of the library already uses. What
this spec must keep:
- **Stability (`docs/stability.md`):** every rename, removal and change of type comes with a stable
  diagnostic code and, wherever one correction exists, a fix. The v1 migration test and every example
  migrate by applying fixes alone.
- **Principle III:** output identical on x86-64, ARM64 and RISC-V, over runs.
- **Principle II:** every new or changed diagnostic has a stable code and a catalog entry.
- **Principle V:** no new syntax. The reference stays within 15,000 tokens (about 13 left), with the
  cost stated and what moves to the site.
- The JSON schema is untouched: fields are only added.

## User Scenarios & Testing *(mandatory)*

The users are AI coding agents writing vo, the people who review their changes, and the owners of
existing vo programs who upgrade the toolchain.

### User Story 1 - Any two library items can be imported together (Priority: P1)

An agent logs a JSON field (`std.json` and `std.log`), checks a list and a text in one file (`std.lists`
and `std.text`), or uses a database and a TCP connection together. Every import works. Today seven pairs
of names collide, and the second import is E0202 "already defined".

**Why this priority**: a collision blocks a correct program outright, and the workaround (a wrapper in
another file) is something no agent should have to invent.

**Independent Test**: a program that imports every exported item of every std module checks without
E0202.

**Acceptance Scenarios**:

1. **Given** a file importing `std.text.contains` and `std.lists.contains` (the old names), **When** it
   is checked, **Then** the lists import gets the library-change diagnostic, and its fix gives the new
   name; after the fix the file checks clean.
2. **Given** the renamed functions, **When** a program uses them, **Then** each behaves exactly as the
   function it replaces.

---

### User Story 2 - One answer for "nothing there" (Priority: P1)

Looking up a header, a neighbour in an ordered collection, an element's index or a regex match returns
`Option[T]`: `none` when there is nothing, `some(v)` otherwise. An agent handles every lookup with the
same two cases, instead of `err("none")` in one module, `-1` in another and `Option` in a third.

**Why this priority**: an agent guesses the failure form from the previous module it used. The guess
is wrong two times out of three today.

**Independent Test**: each changed lookup has a golden case showing `none` and `some`, and an old program
using the result form migrates through the fix with unchanged output.

**Acceptance Scenarios**:

1. **Given** `match header(resp, "X-A") { case ok(v) ... case err(_) ... }` written for the old API,
   **When** it is checked, **Then** it gets the library-change diagnostic with a fix that keeps its
   behavior (the new lookup converted back to a result). The program runs as before.
2. **Given** an empty ordered set, **When** the program asks for its first element, **Then** it gets
   `none`.

---

### User Story 3 - A builtin name is never hidden (Priority: P1)

An agent names a parameter `keys` or a local `len`. The checker reports it at the declaration (E0203,
as for the program's own functions), with a fix that renames it and its uses. Today the declaration is
accepted, and a later `len(xs)` fails with "`len` is not a function".

**Why this priority**: the late error points at the wrong place, and common words (`keys`, `count`,
`find`, `split`, `map`, `min`) are builtins.

**Independent Test**: a golden case with a shadowing parameter and local, each reported with its fix,
and clean after the fixes.

**Acceptance Scenarios**:

1. **Given** `fn total(keys: list[str]) -> int { return len(keys); }`, **When** it is checked, **Then**
   E0203 names `keys` as a builtin. The fix renames the parameter and its uses to a free name, and the
   function then checks.

---

### User Story 4 - Every key type sorts (Priority: P1)

A generic function sorts a map's keys (`K: key`), compares two `bool`s, or orders variant values (a
status enum) with `<`, `sort` and `min`. `false < true`; variant cases order by declaration, and two
values of the same case by their payloads.

**Why this priority**: today no generic function can sort a map's keys, and an enum cannot be sorted
without a hand-written key.

**Independent Test**: golden cases sorting a `list[bool]`, an enum-like variant, a variant with
payloads, and the keys of a `map[K, V]` inside `fn sorted_keys[K: key]`.

**Acceptance Scenarios**:

1. **Given** `variant Level { low; mid; high; }` values, **When** they are sorted, **Then** the result is
   in declaration order.
2. **Given** `fn sorted_keys[K: key](m: map[K, int]) -> list[K]` calling `sort`, **When** it is checked,
   **Then** there is no E0311.

---

### User Story 5 - One convention per concept across the library (Priority: P2)

Across the library:
- a time limit means the same thing everywhere;
- durations are milliseconds with an `_ms` name;
- a function on a text takes the text first;
- parse errors read alike;
- typed reads of JSON fields and database columns are named alike;
- closing a resource needs no effect;
- every error variant has a text form.

An agent who learned one module guesses the next correctly.

**Why this priority**: each convention alone is small; together they decide whether the library can be
guessed.

**Independent Test**: for each convention, a golden case or doc-conformance check over every std module
it covers.

**Acceptance Scenarios**:

1. **Given** `is_match(re, s)` in old code, **When** it is checked, **Then** the library-change
   diagnostic's fix swaps the arguments to `is_match(s, re)`.
2. **Given** a TCP read with `timeout_ms` 0, **When** it runs, **Then** it waits up to the longest
   limit (600,000 ms), as an HTTP request with 0 does.

---

### User Story 6 - Upgrading is a mechanical step (Priority: P2)

An owner upgrades the toolchain, runs `vo check --json`, applies the first fix of every diagnostic, and
the program checks clean and behaves as before. Every example in the repository, and the frozen v1
corpus, goes through this.

**Why this priority**: this is what `docs/stability.md` promises for a breaking change before 1.0.

**Independent Test**: the migration script applies the fixes to every example and every v1 corpus
program, then requires a clean check and the recorded output.

**Acceptance Scenarios**:

1. **Given** a copy of each example as of commit 8c8db3e (before this spec), **When** every first fix
   is applied once (and again if new diagnostics appear, at most 3 rounds), **Then** it checks clean and
   its tests pass with the same results.

---

### User Story 7 - The language's corners behave alike (Priority: P3)

- `[]` takes its type from where it goes, as `ok(1)` and `Shape.empty` do.
- `Shape.empty()` gets a diagnostic with a fix, as `case empty()` does.
- `1E9` is an error with a fix, as `0X1F` is.
- `1e-400` is out of range, as `1e400` is.
- An effect argument has one written form after `vo fmt`.
- Messages name tests, packages and ranges one way.

**Why this priority**: none blocks a program; each is a small surprise.

**Independent Test**: one golden case per corner.

---

### Edge Cases

- **Both the old and the new name** imported in one file: the old one gets the library-change diagnostic
  only, with no E0202.
- **An old name passed as a value** (`log_requests(clock_ms, to_stderr)`): the fix replaces the value. Where no
  replacement value exists, there is no fix and the message says what to write.
- **A program that defines its own function with a new library name** (`fn has_item`): its own
  definitions win as today. The migration fix uses the library name only in files that import it.
- **Shadowing a builtin in a pattern binding** (`case some(len)`) is reported like a local.
- **Variants with a payload that is not ordered** (a map, a function): such a variant is not ordered
  (E0313 where order is needed), as records with such a field are not.
- **A result** stays unordered.
- **An enum-like variant used as a map key and sorted:** the same order everywhere (declaration order).
- **`timeout_ms` above 600,000 or negative:** `err` naming the range, the same text in every module.
- **A cookie's `Max-Age`** is sent in seconds as HTTP requires. `max_age_ms` is rounded down to whole
  seconds.

## Requirements *(mandatory)*

### Functional Requirements

**Migration**
- **FR-001**: A use of a library item that was renamed, removed or changed in type MUST get one new
  diagnostic code ("changed in the library", an error) naming the item, the version that changed it and
  what to write instead. Where one correction keeps the program's behavior, its fix makes that
  correction, at the import and at each use.
- **FR-002**: The diagnostic MUST be reported for imports of std items and their uses only. A program's
  own items with the same names are untouched.
- **FR-003**: Every example in the repository and the v1 migration corpus MUST migrate by applying fixes
  alone (at most 3 rounds), plus `vo effects --update` where a migration changes a function's effects
  (E0504 has no fix). It then checks clean, with test results and output unchanged except for the
  intended output changes the changelog lists: error texts (FR-011, FR-017) and `http.send`'s 0
  (FR-008).

**Decided with the user**
- **FR-004 (clashes)**: each colliding pair is resolved by renaming one side:
  - `lists.contains` → `has_item`;
  - `json.field` → `member`;
  - `tcp.close` → `close_conn` (beside `close_listener`);
  - `tcp.describe` → `tcp_error_text` (FR-016);
  - `metrics.add` → `add_to` (beside `set_to`);
  - `sqlite.get`, `get_int`, `get_float`, `get_str`, `get_bytes` → `value_column`, `int_column`,
    `float_column`, `text_column`, `bytes_column`;
  - `tcp.send` → `write_all` (beside `read`, `read_exact`, `read_until`; `write` is a builtin).

  After this, no two exported std items share a name, and a check in the test suite keeps it so.
- **FR-005 ("nothing found")**: these return `Option[T]` instead of a `result` with `err("none")` or a
  sentinel:
  - `http.header` (renamed `response_header`, FR-013) and `request_header`;
  - in `std.collections`: `first_key`, `last_key`, `key_at_or_above`, `key_at_or_below`, `key_after`,
    `key_before`, `first`, `last`, `at_or_above`, `at_or_below`, `after`, `before`, `peek`, `front`,
    `back`;
  - `regex.first_match`.

  `lists.index_of` keeps `-1`, like `find`. Taking an element (`pop`, `pop_front`) stays a `result` (`err("empty")`). Lookups that can fail in more
  than one way (`json.member`: "not an object" or "no member") stay `result`. Each change's fix wraps the
  new call to give the old `result` (with `to_result` from `std.option`, imported by the fix, and the old
  error text), so the
  program behaves as before.
- **FR-006 (shadowing)**: a local, parameter or pattern binding named like a builtin MUST be E0203. Its
  fix renames the name and its uses in that scope to a free name.
- **FR-007 (order)**: `bool` and variants are ordered:
  - `false < true`;
  - a variant's cases are ordered by declaration, and two values of one case by their payloads;
  - a variant is ordered when every payload type is ordered.

  `<`, `min`, `max`, `sort`, `sort_by` keys, `T: ordered` and ordered collections accept them. Every type
  that satisfies `key` then satisfies `ordered`, and a parameter bounded `key` may be sorted and compared.

**One convention per concept** (following the majority of the library)
- **FR-008 (time limits)**: every `timeout_ms` parameter (http, tcp, workers' `with_deadline`, ...)
  takes 0 to 600,000:
  - 0 means the longest limit, 600,000 ms;
  - other values are `err("invalid: the time limit must be 0 to 600000 ms")`.

  The error case for an expired limit is named `timed_out` in every error variant (`WorkerError.timeout`
  becomes `timed_out`).
- **FR-009 (units)**: library functions take and give milliseconds, and a duration a program sets or reads
  (a parameter or record field) is milliseconds with a name ending `_ms`:
  - `time.fixed_zone` takes milliseconds;
  - `security`'s `lifetime_s` and `hsts_seconds` and web's `Cookie.max_age_s` become `_ms`.

  The tz records (`Zone`, `ZoneRule`, `RuleDay`) mirror the tz database in seconds, which their fields'
  `_s` suffix states. The library converts where a protocol needs seconds.
- **FR-010 (argument order)**: functions on a text take the text first: `regex.is_match`,
  `first_match`, `all_matches`, `split_on` and `replace_matches` (renamed from `replace_all`; `replace`
  would clash with `std.text`, research R2). The fix swaps the arguments.
- **FR-011 (parsing)**:
  - `parse_KIND` reads a value from its text;
  - `from_ENCODING` decodes bytes from an encoding (`bytes.from_hex`, `from_base64`), as today, and the
    reference states this rule;
  - a value's parse error reads `invalid KIND: INPUT`, with `; DETAIL` where the parser knows more (time:
    `invalid time: 2026-1; too short`);
  - a document's (JSON, regex, SQL) reads `invalid KIND at POSITION: DETAIL`;
  - `bits.parse_hex` and `bytes.from_hex` give the same detail for the same bad input.
- **FR-012 (typed reads)**:
  - typed reads are named `KIND_field` (json) and `KIND_column` (sqlite);
  - the text type is called `text` in both (`text_field`, `text_column`);
  - an integer read as a float is converted in both (sqlite refuses today);
  - a missing name reads ``no field `NAME` `` / ``no column `NAME` ``.
- **FR-013 (pairs and spellings)**:
  - `http.header` (a response's) becomes `response_header`, beside `request_header`;
  - `http.serve_bytes_workers` becomes `serve_workers_bytes` (`_bytes` last, as `run_with_bytes`);
  - `random.shuffle` becomes `shuffled` (it returns a copy; verbs change in place, `lists.vo`'s rule).
- **FR-014 (duplicates)**:
  - `web.clock_ms` and `time.now` are removed: their fix writes `now_ms()`, or for a value
    `fn() -> int { return now_ms(); }`;
  - `web.log_requests` stays (spec 055 kept it), and its documentation points at `request_logs`.
- **FR-015 (resources)**: releasing a resource needs no effect: `tcp.close_conn` and `close_listener`,
  like `sqlite.close` and `stop_worker`.
- **FR-016 (error texts)**: every exported error variant `NameError` of a module has
  `name_error_text(e: NameError) -> str`, with one sentence per case: `tcp_error_text`,
  `worker_error_text` and `local_error_text` (time).
- **FR-017 (sqlite)**: sqlite error messages lose the `error: ` prefix, so `try` in `main` prints the
  reason once.

**Language corners**
- **FR-018 (`[]`)**: `[]` is accepted where the expected type is a list (an argument, a typed `let`, a
  return, a field), as `ok(1)` and `Shape.empty` are. E0306 remains where no type is expected.
- **FR-019 (cases without a value)**: `Shape.empty()` is an error with a fix removing `()`, as
  `case empty()` (E0411) is.
- **FR-020 (literals)**: one spelling each, as for `0X1F` today (E0105 with a fix):
  - `1E9`, `1e+9` and leading zeros (`00012`) are E0105 with a fix giving `1e9` and `12`;
  - a float literal that rounds to zero but is not zero (`1e-400`) is E0105, as `1e400` is;
  - `-0x8000000000000000` is E0105 (the negation overflows), with the message pointing at
    `0x8000000000000000`.
- **FR-021 (effect arguments)**: `vo fmt` writes every effect argument as `uses ...`. `Db[D]`, and
  `Db[Storage]` for a named set (E0327 today), are both accepted and become `Db[uses D]` and
  `Db[uses Storage]`.
- **FR-022 (message style)**: diagnostics follow one style, and the catalog's style note states it:
  - a test is named `` `test "NAME"` ``;
  - a package is named `` `NAME` ``;
  - "found" is used, not "got";
  - code is in backticks;
  - ranges are written "A to B";
  - repeated annotations are one code (E0119, with a fix removing the repeat) for every annotation
    (`@size` and `@allow` today report E0329 and W0706);
  - an unreadable base toolchain (`vo build --base`) is T0002, as every unreadable input is.

**Declined or deferred** (recorded in the audit file with the reason)
- **FR-023**: these stay as they are; the guide (`standard-library.md`) states why, and the audit file
  records each:
  - collections' per-type names (`first`/`first_key`/`front`/`peek`, `between`/`keys_between`): vo has
    no overloading, so each type needs its own name;
  - `decimal.total` and `absolute` beside `lists.sum` and `abs`: the same reason, and `abs` is a builtin;
  - `with_cookie` returning a `result`: it validates the cookie, `with_header` does not;
  - `web.bearer_auth`/`api_key_auth` beside the `std.security` ways: deferred to a spec that deprecates
    one, since no mechanical fix exists;
  - explicit type arguments on calls and variant cases (`mk[int]()`): new syntax, deferred.

**Documentation and cost**
- **FR-024**: the reference reflects every change. Its size stays within 15,000 tokens; the spec's
  results state the cost and what moved to the site to pay for it.

### Key Entities

- **Library change**: an old std name, what it became (a new name, a new type, an argument order, or
  removed), the version, and the fix that migrates a use. One table drives the diagnostic, its fix and
  the catalog entry.
- **Ordered type**: `int`, `float`, `str`, `bytes`, `bool`, records whose fields are all ordered, and
  variants whose payloads are all ordered.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A program importing every exported item of every std module checks with no E0202. A test
  keeps every exported name unique across modules.
- **SC-002**: Every example and all 61 programs of the v1 corpus migrate by fixes alone (at most 3
  rounds), then check clean with unchanged test results and output.
- **SC-003**: Every renamed, removed or retyped item (every row of the change table) has a generated check
  showing the diagnostic and its fix, with a golden case for each kind of fix,
  and the fix's result checks clean.
- **SC-004**: Every lookup of FR-005 returns `Option` (checked by doc conformance over std), and no std
  function returns `err("none")`.
- **SC-005**: `sort` on a list of `bool`, of an enum-like variant and of the keys of a generic
  `map[K: key, V]` gives the documented order on x86-64, ARM64 and RISC-V.
- **SC-006**: Each item of the audit's section 3 is marked fixed, declined or deferred, with its reason.
- **SC-007**: The reference stays at or under 15,000 tokens.

## Assumptions

- The toolchain version moves to 0.2.0 with this change (a minor version may break, per
  `docs/stability.md`), and the changelog lists every change with its migration.
- Error strings returned by library functions (`err("...")`) may change in wording without a diagnostic.
  No mechanical correction exists for text a program compares, so the changelog lists each such change.
- New names follow the audit's suggestions where the user did not choose. Planning may adjust a name if a
  better one fits the module, keeping the rules above.
- The frozen v1 corpus is not edited. Its migration may need more than one round of fixes, as it does
  today.
