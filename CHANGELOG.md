# Changelog

What changed in each released version of vo, for people and agents who use it. The record of every
decision is in [`specs/`](specs/) and summarized in [`docs/roadmap.md`](docs/roadmap.md); what a version
may and may not change is in [`docs/stability.md`](docs/stability.md).

## Unreleased

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
