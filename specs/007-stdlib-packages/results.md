# Results: Standard Library and Content-Pinned Packages

## Success criteria

| SC | Status |
|---|---|
| SC-001 | open |
| SC-002 | open |
| SC-003 | open |
| SC-004 | open |
| SC-005 | open |
| SC-006 | open |
| SC-007 | open |
| SC-008 | open |
| SC-009 | open |

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

## Deviations

- T010: `vo doc` checks each standard file on its own (`check_program` on the embedded file) rather than a synthetic program importing every item: the same declarations, one module at a time, no generated source.
- Renamed during implementation, so names are unique across modules and can be imported together: `parse`/`write`/`write_pretty` (json) are `parse_json`/`write_json`/`write_json_pretty`; `format`/`parse` (time) are `format_time`/`parse_time`. `replace`'s parameter `new` (a keyword) is `replacement`.
- A third primitive, `__text_float(s) -> float` (decimal text of up to 19 significant digits to the nearest double), joins `__float_text` and `__http_exchange`: `parse_float` stops at 15 digits like literals, and JSON needs 17.
- `repeat` returns "" for n <= 0 and `from_ms` is total, since library code cannot raise R0007 with its own message.

## Deferred (merged incomplete, 2026-11-14)

The spec was merged before it was finished so that spec 008 (the toolchain in C) could start from it; this commit is spec 008's `ASM_REV`, the behavioral oracle. The remaining work is done after the port, in C (the networking in particular is cheaper to write there).

- **Packages (US3):** `tests/packages_check.py` with at least 20 tamper variants (SC-004) and the SHA-256 vectors against `hashlib` (T018, T019; the step cases exist and the hash was checked against Python by hand); packages in `vo doc` (T023).
- **Time, text, random (US4):** the modules are written and covered by `vo doc` examples (`tests/doc_conformance.py`); the golden cases and `tests/time_check.py` (T024, T027) are not.
- **HTTP (US5):** `__http_exchange` is registered (checker, effect `net`, runtime table) but returns `err("network access is not available")`; sockets, DNS, recording and replay, the test server and the step cases (T028–T032) are not done. `std.http` parses URLs and refuses `https://` correctly.
- **Evaluation (US6):** T033–T035.
- **Polish:** the reference section (T036), the remaining fuzzing (T038), README (T040).
- **Gate not run:** `make gc-check` (the collector stress suite) and the speed check against spec 006 were skipped at merge time; `make test` and `make lint` passed. Spec 008's T003 measures `build-asm/vo`'s speed; run `make gc-check` on `ASM_REV` before trusting it as the oracle for memory behavior.

## Follow-ups
