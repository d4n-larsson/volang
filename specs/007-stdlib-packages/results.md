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
