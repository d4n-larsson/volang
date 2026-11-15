# Results: Standard Library and Content-Pinned Packages

## Success criteria

| SC | Status |
|---|---|
| SC-001 | met: 11 task programs written by an isolated agent from their statements and `vo doc` output only, all on the first round, pass all 32 of their tests (`make stdlib-eval`; see Evaluation) |
| SC-002 | met: every one of the 84 items `vo doc` lists (38 builtins, 46 library items) checks clean when called as documented; all 308 calls with a wrong-typed or missing argument are rejected; all 46 examples print their values |
| SC-003 | met: 132 valid and 116 invalid JSON documents (round trips unchanged, errors with line and column); mutated documents never crash or hang (`fuzz_inputs.py`) |
| SC-004 | met: 22 package tamper variants (changed, added, removed, renamed files) are reported as E0701 before anything runs (`tests/packages_check.py`), plus the 6 tamper step cases |
| SC-005 | met: 10,000 times across years 1-9999, around 1970, leap days and month boundaries agree with Python's `datetime` (`tests/time_check.py`) |
| SC-006 | met: 17 HTTP step cases (plain, chunked, to-close, 404/500, redirect, repeated headers, POST, refused, unknown host, slow with `--timeout`, too large, malformed, scope denied, `https://` and bad URLs) get the expected result; a recorded run replays byte-identically with the server stopped; on x86-64, ARM64 and RISC-V |
| SC-007 | met: the RISC-V build runs every speed program at 0.80-1.00 of the spec 006 binary's time under qemu (geometric mean 0.904); the corpus and the 703 bench2 runs are byte-identical |
| SC-008 | met: the reference grows by about 415 tokens to about 10,415 (budget 15,000); all 34 examples pass |
| SC-009 | met: `make test`, the fuzzers, the determinism check (`--repeat 3`) and the collector stress suite pass on x86-64 with the new cases (455 golden, 109 step); the ARM64 and RISC-V suites and the ASan build pass too (Final gate) |

## Speed baseline

`BASE_REV` is now `1ff166a` (spec 006); the current tree against it before spec 007 code: 0.941 (str_build) to 1.032 (fib).

## Name clashes

No `.vo` file under `tests/`, `bench/`, `bench2/` or `docs/` imports a `std.` path, and there is no directory named `std` in them. `now_ms` is used by 3 replay cases (which print `t - t`), `test/clock_in_test` and `effects/clock_fixed` (fixed clock), `fmt/uses_order` (not run) and the `tests/effect_changes` corpus (effects only): none depends on which clock `now_ms` reads.

## Library effects at run time (T006)

`std_runtime_grants` (src/loader.s, called by `compile_program`) clears `FX_DECLARED` of every function in a `FILE_STD` file. Read-only check of `src/vm.s`: `op_call_fast`, `op_call_generic` and the shared frame setup all take the inherit branch when the callee's declared set is 0, so library frames run under the caller's grant; the compiler emits ordinary calls and `vm.s` is unchanged.

## vo doc (US1)

- `vo doc --json` with no NAME: 0.01 s under qemu (`tests/doc_conformance.py`).
- Conformance: 38 builtin rows and 46 library items; every documented call checks, every wrong-typed or missing argument is rejected, every example prints its value.

## JSON (US2)

- Corpus (`tests/json`, written by `gen_corpus.py` from RFC 8259 and the contract, not from vo): 132 valid documents print their expected text and survive `parse_json(write_json(v)) == v`; 116 invalid ones fail at the expected line and column (the first character that cannot continue a document; a misspelled word, a surrogate escape, an out-of-range number and a 513th level at their first character).
- A generated 123 KB document (1,100 objects, 17-digit floats) is parsed, written and parsed again in 0.60 s under qemu (`tests/json_corpus.py`).
- `tests/fuzz_inputs.py` feeds mutated corpus documents to the driver: no crash, no changed round trip.
- Numbers are converted exactly (src/stdprim.s: big-integer `exact_to_double` and `exact_digits`, ties to even). The 15-digit `text_to_float` was 1 ulp off for 16- and 17-digit input (`-383036.35179613123`, `1.7976931348623157e308`), which broke round trips; the language's own literals and `parse_float` keep the 15-digit path.

## After spec 008 (the remaining work, in C)

The spec was merged incomplete before the C port (spec 008); a convergence pass (tasks.md, Phase 10,
T041-T053) listed what was left, written for the C toolchain. Everything below was done in C on
x86-64 and checked on ARM64 and RISC-V too.

### Packages (US3: T018, T019, T023, T044, T045, T050)

- `vo doc` documents package modules (`src/doc.c`, `src/packages.c`): `vo doc money.format`,
  `vo doc money.format.cents`, nested modules (`money.rates.eur.to_eur`), the module list with the
  package modules after the standard ones, suggestions for misspelled names, and E0701 (exit 65)
  when the package changed. Step case `doc/package_module`.
- `tests/packages_check.py` (SC-004): 22 tamper variants (changed, appended, removed and swapped bytes
  at the start, middle and end of files and nested files; added, emptied, removed, renamed and moved
  `.vo` files; a renamed directory; a `.vo` file renamed to another extension) each stop `vo run`
  with E0701 before the program runs; a changed or added non-`.vo` file changes nothing.
- SHA-256 (T019): `vo package` hashes of single-file packages of every content length 0-130 bytes (every
  padding case of the 64-byte block) and of 1,000,000 x "a" equal Python's `hashlib` over the same
  framing: 132 of 132.

### Time, text and random (US4: T024, T027, T046, T049)

- Golden cases `time/` (5), `text/` (3 new) and `random/` (3); expected output recorded with
  `build-asm/vo` (`std/` is unchanged since it) and checked by hand against a calendar and the contract;
  they pass on every build, three runs byte-identical. `from_ms` has no R0007 case: the contract makes
  it total.
- `tests/time_check.py` (SC-005): 10,000 seeded times (uniform over years 1-9999, within a century of
  1970, around leap days, and at month boundaries +-1 ms) through `from_ms`, `format_time`,
  `parse_time` and `to_ms`: 0 differences from Python's `datetime`.

### HTTP (US5: T028-T032, T041-T043)

- `src/net.c`: `rt_http_exchange` resolves an IPv4 literal, then `/etc/hosts`, then one UDP DNS A query
  to the first `nameserver` of `/etc/resolv.conf` (3 tries of 2 s); connects without blocking, sends
  with `MSG_NOSIGNAL` (no `SIGPIPE`: the toolchain installs no handlers) and reads until the server
  closes, every wait bounded by the `--timeout` deadline (R0009 at the call) and the response by
  16 MiB. Socket call numbers are in the three platform headers; `sys_wait_fd` generalizes the stdin
  wait.
- `src/replay.c`: `io_http_exchange` checks the host against the grant (R0010) in every mode, records
  `{"kind":"http","host","port","result","value"}` and replays it without the network (R0014 for
  another host or port); spec 006's `contracts/recording.md` lists the kind.
- `tests/http_server.py` (raw replies: plain, JSON, chunked, to-close, 404/500, redirect, repeated
  headers, slow, 17 MiB, malformed, short body, echo) and `server`/`stop-server` steps in
  `tests/steps.py` (`{port}` substituted and masked). 17 step cases in `tests/cases/http/` and the
  golden `http/missing_effect`; they failed on the stub, pass natively, under ARM64 and RISC-V (qemu
  translates the socket calls), under the UB-trap build and under ASan, three runs identical.
  `unknown_host` prints only `err`: whether the reply is NXDOMAIN or no answer depends on the resolver.
- `std/http.vo` needed no change: its `Content-Length` is in bytes (`len`), checked by a POST of
  multi-byte text to a server that reads exactly that many bytes.

### Fuzzing (T038, T051)

`tests/fuzz_inputs.py` also mutates a pinned `vo.packages` (then `vo check`, `vo doc money.format`,
`vo doc`) and the bytes of HTTP replies (served by a one-reply server to a `std.http` client): 600 runs,
0 problems.

### Reference (T036, T047)

A section "Standard library and packages" (the five modules, the effect rule, `vo.packages`,
`vo package`, an executed example and a `vo doc` console block), `net` in the effects table, `doc` and
`package` in the command list, E07xx in the code ranges: the reference grows from about 10,000 to
about 10,415 tokens (words x 1.3) since spec 006 (SC-008: at most 2,000 more, under 15,000).

### Evaluation (US6: T033-T035, T048)

- 11 task statements in `tests/stdlib_eval/tNN-*/task.md`, written before any solution, with visible
  tests (flags, args, stdin, expected stdout and exit status; expected output from the maintainer's
  `reference.vo`, checked by hand) and fixtures (a config file; a package `money` pinned by the harness).
  They cover JSON (round trip, a config update, a report), time (date arithmetic, UTC conversion, the
  clock), text (a padded table, word frequencies), random (a seeded shuffle and pick), HTTP against the
  test server, and a package.
- Solved by `claude -p` (the CLI's default model) with no tools, an empty working directory and no user
  settings (bench2's protocol), given the statement, the visible tests, `vo reference` and `vo doc`
  output (the module list, every standard item in full with its example, the task's package module),
  nothing from `std/` or `tests/`; up to 3 rounds with `vo check` and test failures fed back
  (`tests/stdlib_eval.py --solve`, log `tests/stdlib_eval/solve.jsonl`).
- **SC-001**: all 11 solved on the first round; `make stdlib-eval` (`tests/stdlib_eval.py`) passes
  11 of 11 tasks, 32 tests. No solution hard-codes an expected output; each imports the library items
  its task needs (the package task imports `money.format.cents`).

| Task | Round | Tests |
|---|---|---|
| t01-json-pretty | 1 | 3/3 |
| t02-config-update | 1 | 5/5 |
| t03-orders-json | 1 | 3/3 |
| t04-date-math | 1 | 6/6 |
| t05-utc-times | 1 | 3/3 |
| t06-score-table | 1 | 2/2 |
| t07-word-count | 1 | 2/2 |
| t08-raffle | 1 | 3/3 |
| t09-http-client | 1 | 1/1 |
| t10-money-package | 1 | 2/2 |
| t11-clock | 1 | 2/2 |

- Notes: five of the eleven agent calls took 820-883 s (the others 10-256 s), close to bench2's 900 s
  limit per call; a reply checked by hand before the run had no hook or tool text, but its prose claimed
  it had "run the tests locally", which it could not do without tools.
- **SC-002** (`tests/doc_conformance.py`): 38 builtin rows and 46 library items; 308 checked calls (each
  documented call checks clean, each wrong-typed or missing argument is rejected); 46 examples print their
  values.

## Deviations

- T010: `vo doc` checks each standard file on its own (`check_program` on the embedded file) rather than a synthetic program importing every item: the same declarations, one module at a time, no generated source.
- Renamed during implementation, so names are unique across modules and can be imported together: `parse`/`write`/`write_pretty` (json) are `parse_json`/`write_json`/`write_json_pretty`; `format`/`parse` (time) are `format_time`/`parse_time`. `replace`'s parameter `new` (a keyword) is `replacement`.
- A third primitive, `__text_float(s) -> float` (decimal text of up to 19 significant digits to the nearest double), joins `__float_text` and `__http_exchange`: `parse_float` stops at 15 digits like literals, and JSON needs 17.
- `repeat` returns "" for n <= 0 and `from_ms` is total, since library code cannot raise R0007 with its own message.

## Deferred (merged incomplete, 2026-11-14), now done

The spec was merged before it was finished so that spec 008 (the toolchain in C) could start from it; that
commit is spec 008's `ASM_REV`, the behavioral oracle. Everything deferred (packages in `vo doc`, the
tamper and SHA-256 checks, the US4 cases and time check, HTTP, the evaluation, the reference section, the
fuzzing and the gate) was done after the port, in C: see "After spec 008" above and "Final gate" below.

## Final gate (T039, T052)

| Step | Result |
|---|---|
| `make gate` (x86-64, native) | passed: lint; 455/455 golden cases, repeat, every checker and fuzzer (packages_check, time_check, fuzz_inputs with manifests and HTTP replies), 109 + 35 step cases, bench2 703/703, 1,000 programs vs the oracle; ubtrap 455/455; 2,000 programs and 2,000 mutants vs `build-asm/vo`, 0 disagreements; gc-check (stress 455/455, step cases under stress, bench2 703/703, 3,000 fuzz iterations, every fault kind caught); speed 0.896 |
| `make ARCH=aarch64 test`, `make ARCH=riscv64 test` (qemu) | passed (HTTP step cases included) |
| `make instrumented` (ASan + UBSan) | 455/455, 0 reports |
| `make speed-check` | spec 004 targets; 10,000 programs vs the oracle, 0 disagreements; SC-003 of spec 008 6.92 times; SC-004 0.887 |
| SC-007: `speed.py --baseline build-base/vo --new build-riscv64/vo` | closures_generics 0.800, fib 0.917, int_loop 0.840, list_work 0.953, map_count 0.998, str_build 0.929 (geometric mean 0.904) |
| `make stdlib-eval` | 11 of 11 tasks |

The first run of the gate found one mutant disagreement with `build-asm/vo`: a mutated `http/unknown_host`
source whose host became `no-such-ho)st.invalid` stops with R0010 in the C build (the host is outside the
`net` scope) but returned "network access is not available" in the assembly release, whose
`__http_exchange` was a stub without the host check. That difference is this spec's feature, so
`tests/diff_asm.py` leaves sources that use `std.http` out of its mutation corpus; the rerun passed.

## Follow-ups

- The agent benchmark run with and without `vo doc` (the spec's deferred agent run; SC-001 used fixed task
  programs instead).
- HTTPS (TLS), downloading packages (a registry or fetch command), IPv6 and resolver options.
- `ponytail:` markers left by this spec: `std/json.vo` (parsing over `chars(s)` with an index, at most 512
  levels); `src/net.c` (IPv4 and the first nameserver only, one request per connection; the 16 MiB response
  cap); `std/http.vo` (one request per connection, redirects returned, not followed).
