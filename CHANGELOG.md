# Changelog

What changed in each released version of vo, for people and agents who use it. The record of every
decision is in [`specs/`](specs/) and summarized in [`docs/roadmap.md`](docs/roadmap.md); what a version
may and may not change is in [`docs/stability.md`](docs/stability.md).

## Unreleased

- **A generic function's own scopes are enforced (spec 062).** A function with an effect parameter that
  declares scopes, `fn load[E: effects](..., f: fn() uses E) uses E, file.read("cache/")`, was not held to
  them when its caller declared more: its own code could read wherever the caller could. It is now
  stopped with R0010 naming the function, as a function without an effect parameter always was. What it
  is passed (`f`) still does what the caller may do. **This can stop a program that ran before.** If it
  does: widen the function's own scope to what its code touches, or pass that work in as a function.
- **Bug sweep of the C code** ([docs/bug-sweep-2026-11-27.md](docs/bug-sweep-2026-11-27.md)): 48 findings
  fixed, among them `filter` reading outside a list its callback shrank, `set` on a function name passing
  the checker, a server ended by requests that only announce a body, and `vo package --repair` deleting
  a directory a hostile `vo.packages` named. Changes you may notice: `write_file`, `vo fmt`, `vo edit` and
  `vo package` report a write that failed (T0009); a record field given twice is E0202; `--timeout` is at
  most 1,000,000,000; a `sort_by` key may be a `bool` or a variant.

## 0.7.0

- **Versioned packages (spec 061).** `vo package ENTRY NAME https://HOST/REPO[@VERSION]` adds a version of a
  git repository (its tag `v1.2.0` or `1.2.0`; without `@VERSION` the highest) and every package it
  requires, read from the `vo.packages` at the package's root. A program holds one version of each
  package: the highest compatible one that any manifest asks for. Nothing is installed (vo speaks git's
  HTTP protocol), there is no registry, and nothing moves until a command asks: `--outdated ENTRY` lists
  newer versions, `--update ENTRY [NAME]` takes the highest compatible ones, `--repair ENTRY` completes
  a manifest after a merge, `--remove` also removes what only the removed package required, and
  `--dry-run` shows any of these without writing. Each report gives versions, hashes and what the
  packages' effects gained and lost.
- **The manifest** gains `@VERSION` after a source and the word `indirect` for a package that another
  package requires. Existing manifests are unchanged; a toolchain before 0.7.0 reports the new lines
  as malformed (E0703).
- **New codes**: **E0707 package_version_conflict**, **E0708 package_source_clash**,
  **E0709 package_requirement** (also from `vo check`, `run`, `test` and `build` when the manifest
  lacks what a package requires; none of them fetches anything), **E0710 package_version_changed**,
  **E0711 package_version_unknown**.
- **Which URLs are archives**: a URL whose path ends in `.tar`, `.tar.gz` or `.tgz` is fetched as an
  archive, as before. Any other `https://` URL given to `vo package` is now a repository; before, it
  was fetched as an archive whatever its ending.

## 0.6.0

- **Variant cases with fields (spec 060).** A case that carries several values declares them as fields:
  `malformed { path: str; line: int; }`, built with `LoadError.malformed { path: p, line: 3 }` (no `new`)
  and read with `case malformed(m)`, then `m.line`. The fields are read-only (**E0344 case_field_set**).
  Equality, ordering, map keys, JSON (`{"malformed": {...}}`, the format of a record payload), field
  annotations and property tests work as for records. A value prints as it is written,
  `LoadError.malformed { path: "a", line: 3 }`. A case with one field is **E0216 one_field_case**; its fix
  writes `name(T);`. `vo query`/`vo edit` take `Variant.case.field`, and `vo edit add-field` takes
  `Variant.case`.
- **`match` on `int` and `str` (spec 060).** Arms are literals or constants, each once (E0402), and a final
  `else` is required (**E0414 value_match_else**, whose fix adds it). `else` on a variant or `result`
  match is **E0415 match_else_exhaustive** (fix: remove it); `match` on another type is
  **E0345 match_scrutinee**.
- **`std.time.Repeated` removed**: its fields are now those of `time.LocalError.repeated`
  (`time.LocalError.repeated { first: a, second: b }`); a program naming `time.Repeated` gets E0214 saying
  so. The JSON of the value is unchanged.

## 0.5.0

Breaking change first. Applying the first fix of each diagnostic of `vo check --json` (up to four rounds,
`tools/vo_fix.vo`) migrates a program, except an anonymous function nothing gives types to (below): every
example, the standard library, the postgres package, the benchmark solutions and the golden cases
migrated this way (`tests/migrate_059.py`).

- **Anonymous functions take their types from where they are used (spec 059).** They write parameter
  names only: `sort_by(people, fn(p) => p.age)`, `fold(xs, 0, fn(acc, x) => acc + x)`,
  `let shout: fn(str) uses console = fn(s) { print(s); };`. The types come from the function type expected
  at the position (a parameter, a typed `let` or `var`, `return`, `set`, a field, a typed list, a
  payload). In a call, the arguments that are not anonymous functions fix the type parameters first, then
  each anonymous function in turn, its return type from its body where the call leaves it open.
  `let n: int = try transaction(db, fn() { ... })` gives the function its type through `try`.
- **`=> expr`** is the one form of a body that only returns a value; a block is used for every other body.
  `vo fmt` writes it.
- **E0342 lambda_types_written**: a written parameter or return type; the fix removes them, or moves them
  onto an untyped `let`/`var` that binds the function. A written type that differs from the expected one
  is the E0301 it is, so a fix never changes what a program means.
- **E0343 lambda_type_unknown** (no fix): nothing gives the parameter types, as for `let f = fn(x) => x;`
  or a call whose type parameter appears only in the function's own parameters. A `std.web` route
  handler is one: write it as a named function, or bind it with its type.
- **E0413 lambda_single_return**: a block body that only returns a value; the fix writes `=> expr`.
- Hover, `vo query type-at`, traces and runtime error reports show the inferred parameter types;
  `vo query type-at` on a function's, record's or variant's own name gives its type (it gave `?`).

## 0.4.0

Breaking change first. Applying the first fix of each diagnostic of `vo check --json` (up to four rounds,
`tools/vo_fix.vo`) migrates a program: every example, the standard library, the postgres package and the
golden cases migrated this way (`tests/migrate_058.py`).

- **Module imports (spec 058).** An import names a module, and its items are written with the module's
  name: `import std.json;`, then `json.parse_json(text)`, `json.Json`, `new geometry.Point { ... }`,
  `json.Json.number(1.0)`, `uses api.Storage`, `file.read(api.data_dir)`. `import a.b;` loads `a/b.vo`
  (`std.b`, a package's `b.vo`) and binds `b`; `import notes;` loads `notes.vo` beside the file. Builtins and
  the file's own items stay unqualified. The 0.3 form `import std.json.parse_json;` reports **E0215** once
  per file, whose fix rewrites the imports and every use as one edit (text output: `fix: rewrite lines A
  to B`); a library or package file in the 0.3 form still checks. An import of a std item renamed in 0.2
  gets its E0214 fixes first, then E0215's in the next round.
- **No local is named like an imported module.** `let text = ...;` beside `import std.text;` is E0203; the
  fix renames it `text_value` (and E0215's fix does that too).

Added:

- **W0203 unused_import**: a module imported and not used, with the fix removing the line.
- E0201 for a module item written bare (`parse_json` after `import std.json;`, fix `json.parse_json`), a
  missing item (`` `std.json` has no item `nope` ``, with the closest name), a module used as a value, and
  a builtin written with a module (`lists.len`, fix `len`); E0204 for a module used without its import
  (fix `import std.json;`); E0202 for two imports of one module name or a module named like an item; E0214
  at a qualified use of a 0.2 name (`lists.contains`, fix `lists.has_item`).
- Types in messages, fixes and `vo query type-at` are written as the file writes them: `json.Json`, or
  `std.time.DateTime` for a type the file reaches only through another module.
- `vo query` and `vo edit` take `module.name` and `module.Type.member`; renaming renames the item part of
  each `notes.add`.

Changed:

- W0506's fix for the only field of a log record is `[]` (it inserted `new list[Field]` and an import).
- An import that finds no file names the module's file: `Cannot find `money/format.vo``.

## 0.3.0

Breaking changes first. Applying the first fix of each diagnostic of `vo check --json` (up to three
rounds) migrates a program, with no wider grant than it needs (`tests/libchange_rows.py`).

- **A started program's working directory (spec 057).** `std.process.run_with`, `run_with_bytes` and
  `run_lines` take a working directory after the variables; `run` and `run_bytes` are unchanged. `"./"`
  is the start directory (where `run` starts programs) and is always allowed; any other directory must lie
  inside the caller's `file.read` scopes (E0503 for a literal, R0010 at run time), and the three functions
  need `file.read`. A program path with `/` is still found from the start directory. A call in the 0.2
  form reports E0214 with a fix inserting `"./"`, and the function then needs `file.read` (E0501, below).
- **E0501's fix scopes file effects needed only for literal paths.** The fix declared `file.read`,
  `file.write` or `file.delete` without scopes; when every need for one comes from literal paths, it now
  declares those paths' scopes: a file's directory, a directory as written (`read_file("data/a.txt")`
  gives `file.read("data/")`, `run_with(..., "./")` gives `file.read("./")`). Callers get their callees'
  scopes. Any other need keeps the unscoped fix. Code, span and message are unchanged.
- **Literal programs checked in every run form.** A literal program outside the `process` scopes is
  E0503 at check time for `run_bytes`, `run_with_bytes` and `run_lines` too (it was R0010 at run time).

Added:

- A directory that is missing, not a directory, or cannot be entered is `err("directory not found")`,
  `err("not a directory")` or `err("directory not accessible")`, and nothing starts. An empty directory
  is R0007.
- Recordings hold a run's directory (`"dir"`, absent for `"./"`), and replay requires the same one
  (R0014). A run in `"./"` is recorded exactly as by 0.2, so 0.2 recordings replay against migrated
  programs.

## 0.2.0

Breaking changes first. Every one comes with a diagnostic and, where one correction exists, a fix:
`vo check --json` and applying the first fix of each diagnostic (up to three rounds) migrates a program.
Every example, the postgres package and the v1 corpus migrate this way (`tests/migrate_056.py`,
`tests/migrate.py`).

- **Library changes (E0214 `library_changed`).** An import or use of a changed std item reports E0214
  and still checks, with a fix giving the new form:
  - renamed so that no two modules share a name: `lists.contains` → `has_item`, `json.field` →
    `member`, `tcp.close` → `close_conn`, `tcp.send` → `write_all`, `tcp.describe` → `tcp_error_text`,
    `metrics.add` → `add_to`, `sqlite.get`, `get_int`, `get_float`, `get_str`, `get_bytes` →
    `value_column`, `int_column`, `float_column`, `text_column`, `bytes_column`, `http.header` →
    `response_header`, `regex.replace_all` → `replace_matches`, `http.serve_bytes_workers` →
    `serve_workers_bytes`, `random.shuffle` → `shuffled`;
  - lookups that may find nothing return an `Option` (`some(value)` or `none`): `http.response_header`
    and `request_header`, `regex.first_match`, and in `std.collections` `first_key`, `last_key`,
    `key_at_or_above`, `key_at_or_below`, `key_after`, `key_before`, `first`, `last`, `at_or_above`,
    `at_or_below`, `after`, `before`, `peek`, `front` and `back`. Where the old `result` is still read
    (`try`, a `match` on `ok`/`err`, a comparison), the fix wraps the call in `to_result(CALL, "OLD
    ERROR")`, which keeps the program's behavior;
  - `std.regex` takes the text first: `is_match(s, re)`, `first_match`, `all_matches`, `split_on`,
    `replace_matches(s, re, replacement)`; the fix swaps the arguments;
  - `WorkerError.timeout` → `timed_out`, the one name for an expired time limit;
  - removed duplicates of the builtin `now_ms()`: `web.clock_ms` and `time.now` (the fix writes
    `now_ms()`, or `fn() -> int { return now_ms(); }` for a function value);
  - durations in milliseconds: `time.fixed_zone(minutes)` → `offset_zone(ms)`, `web.Cookie.max_age_s`
    → `max_age_ms` (sent as whole seconds), `security.Browser.lifetime_s` → `lifetime_ms`; the fix
    converts a literal (`fixed_zone(330)` → `offset_zone(19800000)`), multiplies another value, and
    reads a field as `(c.max_age_ms / 1000)`. The tz records' fields say their unit: `Zone.starts_s`,
    `offsets_s`, `ZoneRule.std_offset_s`, `dst_offset_s`, `RuleDay.time_s`.
- **A local may not take a builtin's name** (E0203, as the program's own functions): a `let`, `var`,
  parameter, loop or `case` variable named `len`, `min`, `lower`, ... The fix renames it to
  `NAME_value`, at its declaration and each use.
- **Literals have one spelling** (E0105 with a fix): `1E9` and `1e+9` are `1e9`, `007` is `7`. A float
  literal that rounds to zero (`1e-400`) is an error, as one too large is, and so is
  `-0x8000000000000000` (the negation overflows).
- **Error texts** (values a program may compare; no diagnostic, so check comparisons with these):
  - parsing is `invalid KIND: TEXT; DETAIL`, a document `invalid KIND at POSITION: DETAIL`:
    `parse_time` (`invalid time: 2026-13-01; month 13 is outside 1-12`), `bytes.from_hex` and
    `from_base64` (`invalid hex: abc; odd number of hex digits`), `parse_json` and everything reading
    JSON through it (`invalid json at line 1, column 7: ...`), `parse_regex` (`invalid regex at byte 0:
    ...`), `parse_sql` (`invalid sql: ...`); std.web's body reason drops its `not JSON: `;
  - sqlite drops the `error: ` prefix of a plain error (`unable to open database file`); `constraint:`,
    `busy:`, `readonly:` and the other kinds stay;
  - sqlite's `no column `NAME`` and `ambiguous column `NAME`` have backticks, and `float_column` converts
    an INTEGER;
  - std.time's `local_error_text`, std.workers' `worker_error_text` (new) and std.tcp's
    `tcp_error_text` give a sentence per case.
- **Time limits:** every `timeout_ms` takes 0 to 600,000; 0 means the longest, 600,000 (10 minutes).
  `http.send` and `send_bytes` took 0 as "only the run's `--timeout`": an HTTP request under
  `--timeout 0` is now bounded at 10 minutes. Other values are `err("invalid: the time limit must be 0
  to 600000 ms")`; `with_deadline` clamps instead.
- **Releasing a resource needs no effect**: `tcp.close_conn` and `close_listener`, like `sqlite.close`.
- **The postgres package** follows std.sqlite: its readers are `value_column`, `int_column`,
  `float_column` (converting an integer), `text_column`, `bytes_column` and `time_column`. The package
  is not in the E0214 table: its users rename by hand.

Additions:

- `bool` and variants are ordered: `false < true`; a variant by case in declaration order, then by
  payload. So every `key` type is ordered, a generic `K: key` can be sorted, and an enum-like variant
  can key an `ordered_map`.
- `[]` takes its type from the context (an annotation, a parameter, a return type, a field), as `ok(1)`
  does; with none it is E0306.
- A named effect set is accepted as an effect argument (`Db[Storage]`); `vo fmt` writes every effect
  argument with `uses` (`Db[uses D]`, `Db[uses Storage]`).
- `Shape.empty()` is E0411 with a fix removing `()`. A repeated `@allow` code or field annotation is
  E0119 with a fix removing the repeat (it was E0329 or W0706). An unreadable `vo build --base` is T0002
  (exit 66).
- Messages follow one style: backticks for code, names and paths, `` `test "NAME"` ``, "found" (not
  "got"), ranges "A to B".

- Operating a service. A new effect `log` covers writing log records and metrics. `std.log` writes one
  JSON record per line on stderr: `log_info("note created", [field("id", 7)])` gives time, level,
  message, the id of the request being answered and the fields; `vo run --log-level LEVEL` sets the
  lowest level; under `vo test`, `logged()` returns the test's records. Fields, nested members and
  headers whose names look like secrets (password, token, key, authorization, cookie, session, ...) are
  written `[redacted]`, and W0506 warns when a log field's value is a secret-named variable (its fix
  removes the field). `std.metrics` has counters, gauges and histograms, shared by `serve_workers`'
  worker processes (E0341 for bad literal names, R0016 for conflicting uses). `std.web` gains the steps
  `request_logs()` and `request_metrics()` (by route pattern, never by path), `metrics_route` (`GET
  /metrics`, Prometheus text format) and `health_routes` (`/health/live`, `/health/ready`). `vo run
  --drain SECONDS` (default 30) bounds the graceful stop: requests still running are answered 503 and
  the server ends with a `stopped` record; plain `serve` now also answers the requests it has read before
  stopping. A generic step carrying its own effects (`Step[C, uses log, clock, E]`) placed in an app now
  infers `E`. `@allow` also accepts W0506. `examples/notes-api` uses all of it.
- Recorded tests. `vo test --record DIR` runs the tests against the real outside world and saves, for
  each test that received anything (HTTP and HTTPS answers, TCP exchanges, files, environment variables,
  other programs), one readable file `DIR/FILE/NAME.jsonl` with the requests it sent and what it got.
  `vo test --replay DIR` runs each test on its file with no outside access, the same every time. A test
  that asks for something its recording lacks (another URL, method, body, file, variable, more requests,
  or no recording at all) fails with R0014 naming both, and the other tests run on; `--test` re-records
  one test. Credentials are written as `[redacted]`: `Authorization`, `Proxy-Authorization`, `Cookie`,
  `Set-Cookie`, and headers, query parameters and variables named with `--redact NAME`. Plain `vo test`
  is unchanged. `--coverage` and `--mutate` work on replayed tests. New example `examples/rates`.
- vo's target is written down: backend network services written by agents and reviewed by people. vo
  renders no HTML; pages belong to the frontend in front of a service. The catalog holds E0335 to E0340
  as retired codes (from HTML templates that were built and removed before any release): no version
  reports them.
- Sessions and sign-in for browsers. `std.security` signs people in through a frontend's form and a
  session cookie: `browser(b)` is a way of signing in listed in the policy beside `tokens` and `basic`,
  and `browser_routes(b, p)` adds sign-in and sign-out. The frontend serves the sign-in page and posts
  `name`, `password` and `next`; the answer is 303 to `next` with the cookie, or 303 back to the sign-in
  path with `failed=1`. The application keeps the sessions (the `Browser` record takes its functions)
  and stores only a hash of each cookie's value. The cookie is `HttpOnly`, `Secure` and `SameSite=Lax`;
  a cookie-signed request that changes something must come from the application's own origin, or it is
  refused with 403; a browser asking for a page that needs a user is sent to the sign-in path, while
  APIs still get 401. `std.web` gains cookies: `cookie`, `with_cookie`, `read_cookie`. `Way` has two new
  fields, `sign_in_page` and `origin` (`""` for the existing ways): a `Way` written with `new` needs them.
  `review` reports browser sign-in. The secure-notes example signs in programs with tokens (now at
  `POST /token`) and browsers with sessions. A recording of a signed-in application holds the passwords
  and cookies it received.
- Fixed: in a type's effect argument, an effect parameter or a named effect set can now follow the
  effects written out (`Job[uses clock, D]`, `Db[uses clock, Storage]`), as in a function header.
  Before, only the other order parsed, and `vo fmt`, which writes that order, made such a program fail
  to check (E0327). A type written after the effects is still E0327.
- HTML form bodies: `std.web` reads `application/x-www-form-urlencoded` bodies into input records like
  query parameters (a missing `bool` is `false`, `on` is `true`); a 415 now names both encodings.
- A PostgreSQL client, the `postgres` package (`packages/postgres` in the repository; add it with
  `vo package`). Its items have the names, parameters, results and error texts of `std.sqlite`
  (`sql`, `exec`, `query`, `transaction`, the getters), with `?` parameters on both, so a program moves
  between the two by changing its imports, the code that opens the database, its effects and the SQL
  the two spell differently. `connect(options)` needs `net` with the server in its scopes and
  `entropy`; the connection uses TLS unless `plain_text` is set, signs in with SCRAM-SHA-256, and has a
  time limit on every wait. `get_time` reads dates and timestamps as milliseconds since 1970 UTC.
- The notes API example adds a note with `insert ... returning id` (the form that works on both
  databases) instead of `last_id`.
- Scopes work through functions that are given their target. A function that declares `net`,
  `file.read`, `file.write`, `file.delete`, `env` or `serve` without scopes (a helper, a package's
  `connect(host, ...)`) can now be called from a function that declares the effect with scopes; before,
  that was E0503 and the fix was to drop the caller's scopes. Such a function has no limit of its own:
  when the program runs, a target is checked against every scoped declaration on the call path
  (R0010), so `main`'s scopes and those of every function in between still hold. In `main` and in
  tests an effect without scopes means every target, as before. Programs that checked before check
  the same way.
- Time zones in `std.time`. The IANA tz database (release 2026f) is built into `vo`, so
  `load_zone("Europe/Stockholm")` needs no effect and no file on the machine and answers the same
  everywhere. `in_zone`, `offset_at`, `abbreviation_at` and `format_in` show an instant in a zone;
  `from_local` turns a local calendar time into an instant and returns an error value, never a guess,
  for a time the zone's clocks skip or show twice; `fixed_zone(minutes)`; `zone_from_bytes` for a
  program that ships newer zone data; `local_zone()` (needs `clock`) is the machine's zone, from `TZ`
  or `/etc/localtime`, UTC with a fixed clock. `parse_time` also reads offsets with seconds
  (`+00:53:28`). `vo version` prints a second line, `tzdata 2026f`, and its JSON record has a `tzdata`
  field. The binary is 460 KB larger.
- `std.workers`: processes that run beside the program and exchange typed messages with it.
  `start_worker(f)` returns at once with a handle; `tell` and `receive` on the program's side,
  `next_message` and `emit` in the worker; `receive_first` waits on several workers; `stop_worker` ends
  one whatever it is doing; `with_deadline(ms, f)` gives one call a time limit. A worker's runtime error
  is a value (`failed`), not the program's end; one nobody received is reported when the program ends,
  with exit status 70. No worker outlives the program. What a worker prints is written by the program
  in order with the worker's messages, so the output is the same on every run; recordings hold what the
  program got from its workers, and a replay starts none. At most 64 workers, 16 MiB a message.
- `sleep` writes out what was printed before it waits.
- `for` with two variables: `for k, v in m` gives a map's keys with their values, `for i, x in xs` a list's
  (or deque's) indexes with their elements; `_` for either. Such a loop visits what was there when it
  began: entries added in the body are not visited, removed ones are skipped. New warning W0403 with a
  fix for the loops this replaces (`for k in keys(m)` then `let v = m[k];`, and `for i in 0..len(xs)`
  then `let x = xs[i];`).
- `std.lists`: `contains`, `index_of`, `reversed`, `sort_by_desc`, `min_by`, `max_by`, `sum`, `group_by`.
- `sort_by` accepts a key whose type is an `ordered` type parameter, so generic functions can sort.
- `std.security`: access control for `std.web` applications. A policy is one table of rules (who may call
  which method and path: anyone, signed-in users, users with a role, nobody), and `secure(policy)` enforces
  it: a request no rule matches is refused, 401 without credentials and 403 with. Ways of signing in:
  signed tokens (JWT, HMAC-SHA256) with a ready sign-in route, HTTP Basic, and keys checked by the
  application. Operations get the user with `require_user`, `require_role` and `current_user`. `decide` is
  the decision as a pure function; `describe` and `review` apply it to an application's routes.
  `safe_headers` adds the usual response headers. Also new: `std.web.problem_reply` and
  `std.crypto.password_outdated`.
- `std.decimal`: exact decimal numbers for money and other amounts. A `Decimal` is units and places (0 to 18);
  `parse_decimal`, `decimal_text`, `grouped_text`, exact `add`, `sub`, `mul` and `total`, `div` and `round_to`
  with a stated `Rounding`, `compare`, `equal`, `normalize`, `to_int`, `to_float`, and `split_evenly` and
  `allocate`, whose parts sum to the amount. A result that does not fit stops with R0001.
- `std.bits`: `bit_and`, `bit_or`, `bit_xor`, `bit_not`, `shift_left`, `shift_right`, `shift_right_logical`,
  `rotate_left`, `rotate_right`, `wrapping_add`, `wrapping_sub`, `wrapping_mul`, `parse_hex`, `parse_binary`,
  on the 64-bit form of an `int`. No new operators: `&`, `|`, `^`, `~`, `<<` and `>>` now name the module.
- Integer literals in hex and binary: `0xFF`, `0b1010`, each a 64-bit pattern (`0xFFFFFFFFFFFFFFFF` is -1).
  `vo fmt` keeps the base and writes hex digits in upper case.
- **Breaking:** `upper` and `lower` now map Latin, Greek and Cyrillic letters, not only ASCII: `upper("åäö")`
  is `"ÅÄÖ"`. A program that upper- or lower-cased such text prints something else. Letters whose mapping
  is several characters (`ß`) stay as they are.
- Strings: raw strings between backticks (`` `\d+` ``, any text, any lines); the escapes `\r` and `\u{HEX}`;
  `for c in s` over a string's characters; `std.text.char_count` and `equal_fold`. `vo fmt` writes a raw
  string that needs no backticks in quotes (W0102), and control characters in shown values as escapes.
- Float literals and `parse_float` accept an exponent: `1e9`, `1.5e-3`.
- An effect scope cannot hold a control character (E0506).
- `vo mcp`: the toolchain as a Model Context Protocol server for agent hosts (ten commands as tools, the
  reference as the resource `vo://reference`).
- `vo lsp`: the toolchain as a language server for editors (diagnostics on unsaved text, quick fixes, hover,
  definition, references, rename, formatting, outline).
- `std.tcp`: TCP connections (`connect`, `connect_tls`, `start_tls`, `send`, `read`, `read_exact`,
  `read_until`, `listen`, `accept`) under the `net` and `serve` effects, with typed errors and record and
  replay.
- `net` scopes can name a port: `net("db.internal:5432")`. A bare host still means any port. `std.http`
  obeys ported scopes too. `serve` scopes accept port `0` (a free port, for `std.tcp.listen`).
- Streaming: `std.fs.each_line` and `each_chunk`, `std.http.send_stream` and `std.process.run_lines` read
  files, response bodies and a program's output one piece at a time, in memory that does not grow.
- `vo package ENTRY NAME URL` fetches a package (a `.tar.gz` or `.tar` archive over HTTPS), pins it by content
  and prints the effects it declares; `vo package --restore ENTRY` fetches missing packages again at their
  pinned hashes. A manifest line may end with the URL: toolchains before this change reject such lines.
  The `package` JSON record gains `url`, `effects`, `gained`, `lost`, `note`, and two values of `change`,
  `restored` and `present`. New code E0706.
- Editor grammars in `editors/`: VS Code (TextMate), Neovim, Helix and Zed (tree-sitter), Vim.
- Fixed: `vo impact --diff -` and `vo test --diff -` read a pipe (`git diff | vo impact main.vo --diff -`);
  before, standard input had to be a regular file.

## 0.1.0

The first release: the toolchain as one static binary for Linux on x86-64, ARM64 and RISC-V 64.

**Language**

- Statically typed, with records, variants and exhaustive `match`, generics with `ordered`, `key`, `json`
  and effect parameters, functions as values, constants, and no implicit conversions.
- Checked effects with scopes (`uses file.read("data")`), an effects record (`vo.effects`) that makes
  every change in what a program may touch a reviewed change, and enforcement at run time.
- `result[T]` and typed errors `result[T, E]` with `try` and `map_err`; no exceptions.
- Lists, maps keyed by any value type without floats or collections, ordered maps and sets, priority
  queues, deques, `bytes`.
- Typed holes, property tests, contracts, `@spec` annotations.

**Toolchain**

- `vo check` with stable error codes and mechanical fixes, `vo run`, `vo test` (selection, coverage,
  mutation), `vo fmt`, `vo effects`, `vo doc`, `vo explain`, `vo reference`, `vo package`.
- `vo query`, `vo impact` and `vo edit` for questions and changes by structure, not by text.
- Deterministic runs: `--seed`, `--clock-ms`, `--record` and `vo replay`, `--trace`, time and memory
  limits.
- `vo build`: a program as one executable that prints what it may touch (`--vo-effects`).
- JSON output for every command (`--json`, schema 1).

**Standard library**

- `std.json`, `std.text`, `std.time`, `std.random`, `std.math`, `std.bytes`, `std.collections`,
  `std.option`, `std.regex`, `std.fs`, `std.process`, `std.http` (client with HTTPS, server with worker
  processes), `std.sqlite`, `std.web`, `std.crypto`.
