# Research: Standard Library and Content-Pinned Packages

Decisions for spec 007. File references are to `main` at `1ff166a` (spec 006).

## The library

### R1: The standard modules are vo source embedded in the binary

- **Decision:** `std/json.vo`, `std/time.vo`, `std/text.vo`, `std/random.vo` and `std/http.vo` are ordinary vo files in the repository, embedded in the binary with `.incbin` (as `src/data.s` already embeds `docs/reference.md` and `docs/errors.md`) together with a table of {path, text, length}. Everything the library does that vo can express is written in vo: JSON parsing and writing, calendar arithmetic, ISO 8601, the text helpers, shuffling, and the HTTP/1.1 protocol (request text, status line, headers, chunked bodies).
- **What the runtime adds** (R5): one network primitive, one float formatter, and a real-time clock behind the existing `now_ms`.
- **Rationale:** vo code is checked, formatted, tested and traced like any program, is far smaller than the same logic in assembly, and is the best documentation of itself (`vo doc` reads its comments, R7). Only what vo cannot express goes into the runtime.
- **Alternatives:** the library in assembly (several times the code, none of the checking); the library as files installed next to the binary (a second thing to keep in step with the binary; the embedded copy cannot drift from the toolchain version, which is the point of the spec).

### R2: `import std.MODULE.ITEM;` resolves to the embedded files

- **Decision:** in import resolution (`src/loader.s`, `import_rel`), a path whose first segment is `std` names the embedded file `std/MODULE.vo`, registered with path `std/MODULE.vo` (shown as such in diagnostics). Standard files import each other the same way. A local directory called `std` can no longer be imported from (FR-020); the migration corpus is searched for one (none expected).
- **Rationale:** the existing import grammar, one item per import, no new syntax (Principle I).

### R3: The JSON value

- **Decision:**

  ```text
  variant Json { null; boolean(bool); integer(int); number(float); text(str); array(list[Json]); object(map[str, Json]); }
  ```

  - Case names avoid keywords and builtin names (`str`, `int`, `bool` are reserved); a recursive variant with a map payload checks today (verified).
  - Maps keep insertion order, so object keys stay in document order, and a duplicate key updates the value in place (last wins, first position) for free.
  - Integers that fit in `int` are `integer`; anything with a fraction, an exponent or out of range is `number`.
  - Parsing walks `chars(s)` with an index (code points, so line and column come for free); nesting is limited to 512 levels (`err` beyond).
  - Writing uses `str` for integers and the round-trip float formatter (R5) for `number`. `write` and `write_pretty` return `result[str]`: a NaN or infinite `number` cannot be written and is an `err`.
- **Rationale:** a variant is the vo way to represent "one of these kinds", `match` is exhaustive over it, and helpers (`text_field`, `int_field`, …) cover the common reads without `match` chains.

### R4: Time, text and random in vo

- **Time:** days from civil and civil from days (Howard Hinnant's algorithms, integer only, exact for years 1–9999 and negative epochs); `DateTime` record with ISO weekday (1 = Monday); ISO 8601 `YYYY-MM-DDTHH:MM:SS[.mmm](Z|±HH:MM)`; `now()` is `now_ms()`.
- **Text:** written over byte offsets with `find`/`slice` for search and replace, and `chars` for widths (code points).
- **Random:** `unit()` is `random_int(0, 2^53 - 1)` scaled by 2^-53; `pick` and `shuffle` (Fisher–Yates on a copy) use `random_int`. All fixed by `--seed`.

### R5: Runtime primitives, reachable only from `std`

- **Decision:** three internal builtins whose names start with `__`, callable only from files under `std/` (elsewhere they are undefined names, E0201):
  - `__http_exchange(host: str, port: int, request: str) -> result[str]` (effect `net`): checks `host` against the running grant (R6), resolves it (R9), connects with the `--timeout` deadline, sends the request, reads until the server closes (HTTP/1.0-style `Connection: close`), and returns the raw response; a response over 16 MiB is `err`.
  - `__float_text(f: float) -> str` (pure): 17 significant digits, the shortest that always round-trips a double (the existing `str(f)` keeps its 15 digits for compatibility).
  - None for the clock: `now_ms` becomes the real-time clock (CLOCK_REALTIME), which is what the reference already promises ("wall-clock milliseconds"); timeouts keep using the monotonic clock internally.
- **Rationale:** the smallest runtime surface that makes the vo library possible; `__` names keep it out of user programs without a new visibility rule.
- **Alternative:** public builtins for sockets: a much larger, unsafe-to-misuse surface for agents.

### R6: Effects of library functions

- **Decision:**
  - Library functions declare their effects as unscoped effects (`uses net`, `uses random`, `uses clock`).
  - **Statically**, a call to a library function counts like a call of a builtin: the caller must declare the effect, with any scope (E0501 when it does not; E0503's "caller's scopes cover the callee's" does not apply to `std` callees).
  - **At run time**, a library function's frame runs with its caller's grant (as anonymous functions already do: `FR_EFFECTS` is inherited when a callee declares nothing, vm.s around 1530). Before a program runs, the declared effect sets of `std` functions are cleared for the runtime (the checker has already used them), so every frame-setup path inherits. `__http_exchange` then checks the host against the user function's `net` scope, exactly as `read_file` checks paths (`rt_check_name` with `EF_NET`).
  - The effects record does not list `std` functions; their effects show at their callers.
- **Rationale:** library functions behave like builtins for capabilities, which is what reviewers and agents already understand; scopes stay meaningful (a program granted `net("localhost")` cannot reach another host through the library).
- **To verify in implementation:** every frame-setup path (`op_call`, `op_call_fast`, `op_call_generic`, `op_call_value`, `vm_push`) takes the inherit branch when the declared set is 0; `tests/cases/http/` includes a scope-denial case through `std.http.get`.

### R7: `vo doc` reads the library itself

- **Decision:**
  - `vo doc` checks a synthetic program that imports every standard module (and, with a manifest found from the working directory like `vo.effects`, every package), then prints items from the checked declarations: name, type parameters, parameters with types, return type, effects (`ty_name`, `fx_item_name`, the FX record), so the signature is the checker's by construction (FR-002).
  - **Descriptions** come from the comment lines immediately above each item in the library source (`# Parses JSON text ...`): the first line is the one-line description, lines starting with `#     ` (four spaces) are the example, written `EXPR is VALUE` so the conformance check can run it. The parser already keeps comments as items (`N_COMMENT`, used by `vo fmt`).
  - **Builtins** are listed from the reference's Builtins table (the table the checker's behaviour is documented and tested against since spec 001); the conformance check (R12) covers them too.
  - **Forms** (one form, R3 of spec 006): `vo doc [--json] [NAME]` where NAME is absent (all modules and builtins), a module (`std.json`, `money.format`), or an item (`std.json.parse`, `print`). Package names are resolved through the nearest manifest from the working directory, so no entry file is needed (deviation from FR-003's `vo doc ENTRY PACKAGE`, recorded in results).
- **Rationale:** generated from the checker's own data, the docs cannot drift (the spec's core requirement).

## Packages

### R8: Manifest, hash and resolution

- **Decision:**
  - **Manifest** `vo.packages`, found like `vo.effects` (nearest in the entry's directory or above), one line per package: `NAME DIRECTORY sha256:HEX`, the directory relative to the manifest; `#` comments; malformed lines are E0703.
  - **Hash:** SHA-256 (new `src/sha256.s`, FIPS 180-4, tested against the standard vectors) over, for every `*.vo` file under the directory in byte-sorted relative-path order: the relative path, a NUL byte, the decimal length, a NUL byte, the bytes. Any changed byte, added, removed or renamed file changes it.
  - **Resolution:** an import whose first segment is a manifest name resolves to `DIRECTORY/REST.vo` (the rest of the path below the package); files inside a package import each other, `std` and other manifest packages the same way. The hash is computed when a package is first used in a check; a mismatch is E0701 (both hashes in the message), before any item is checked. A first segment that is both a package and a local directory next to the importer is E0705.
  - **Errors:** E0701 hash mismatch, E0702 package directory missing, E0703 malformed manifest, E0704 a name listed twice, E0705 package name clashes with a local directory; an import of a name that is neither is the existing E0205.
  - **Command:** `vo package [--json] ENTRY NAME DIRECTORY` adds or updates an entry (computing the hash) and prints `added money ../shared/money sha256:…` or `updated money: sha256:old -> sha256:new`; `vo package --remove ENTRY NAME` removes one. It writes the manifest with the existing safe-write helper.
  - **Run time:** nothing: packages are loaded and verified during the check that precedes every run (Principle III; FR-014).
- **Effects record:** package functions are listed with a package-qualified path, `@money/format.vo:cents uses net("api.local")`, so the record does not depend on where the package directory is (`rec_relpath` learns the prefix).
- **Rationale:** the same "nearest file" rule as `vo.effects`, a human-readable manifest a reviewer can diff, and a standard hash.

## Networking

### R9: Name resolution and sockets without libc

- **Decision:** `__http_exchange` resolves a host by: a dotted IPv4 literal; else `/etc/hosts`; else one UDP DNS query (A record) to the first `nameserver` in `/etc/resolv.conf`, with retries until the `--timeout` deadline. Sockets use the raw Linux calls (`socket`, `connect` non-blocking with `ppoll` against the deadline, `sendto`/`recvfrom`, `close`). IPv6 is out of scope (spec edge case).
- **Rationale:** what a static, libc-free binary can do with system calls alone; the test server is on `localhost`, which `/etc/hosts` resolves.

### R10: HTTP in vo

- **Decision:** `std/http.vo` builds `METHOD PATH HTTP/1.1\r\nHost: …\r\nConnection: close\r\n[Content-Type, Content-Length]\r\n\r\nBODY`, sends it through `__http_exchange`, and parses the status line, headers (order kept, names compared case-insensitively by `header`) and the body (`Content-Length`, chunked, or until close). `https://` and URLs it cannot parse are `err` before anything is sent.

### R11: Recording HTTP

- **Decision:** a new recording event kind `http` (`host`, `port`, `result`, `value` = the raw response or the error text); replay checks the host and port against the next event (R0014 otherwise) and returns the recorded response without the network. Writing it reuses spec 006's allocation-free writer.

## Evaluation and tests

### R12: Conformance and the task programs

- **Conformance** (`tests/doc_conformance.py`): for every item `vo doc --json` lists, build a program that calls it with arguments of the documented types (generated values per type) and requires a clean check; then one program per parameter with a wrong-typed argument and one with a missing argument, each of which must be rejected. Builtins with special typing (generic over `str | list | map`) use the alternatives the table lists.
- **Task programs** (`tests/stdlib_eval/tNN-*/`): at least 10 task statements with visible tests (`main.vo` + inputs + expected outputs), solved by a separate agent that is given only the statements, the reference and `vo doc` output (not the library source); `tests/stdlib_eval.py` runs them (starting the HTTP test server where needed).
- **JSON corpus** (`tests/json/valid/*.json`, `tests/json/invalid/*.json`, at least 100 each, written for this spec from RFC 8259's grammar): `tests/json_corpus.py` parses each with a small vo driver and checks the result, the round trip, and the error position.
- **Time** (`tests/time_check.py`): 10,000 generated times compared with Python's `datetime` (proleptic Gregorian, UTC).
- **HTTP test server** (`tests/http_server.py`): fixed routes (plain, chunked, status 404/500, redirect, slow, large, malformed) on a port the step runner passes as `{port}`; `tests/steps.py` gains a `server` step.

### R13: Speed

- **Decision:** the runtime changes are a new builtin table entry pair, the real-time `now_ms`, and the `http` record/replay hook; the speed baseline moves to the spec 006 binary (`BASE_REV := 1ff166a`), and the page pins of `vm.s`, `rt_map.s` and `rt_list.s` stay. If a runtime file that links before a pinned one grows enough to move a hot function across a page, the same pin technique applies.
