# Results: Binary data

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | Every byte value 0–255 round-trips unchanged through files (`read_all256`, `write_ints`: the written file is byte-identical to `tests/bytes/all256.bin`), standard input and output (`stdin_stdout`), process output (`run_bytes`: 256 bytes on stdout, the reverse on stderr; `run_with_bytes`), `list[int]` (`ops`, `hex`), hex (`hex`) and base64 (`base64_rfc`), and HTTP and HTTPS (`get_bytes`, `get_bytes_https`, `post_bytes`, `serve_bytes`). Three platforms: Verification. |
| SC-002 | met | `base64_rfc`: the 7 RFC 4648 vectors both ways; `hex`: every byte value, uppercase accepted; `decode_errors`: each invalid input names its position (or the odd count / length), never a partial value. |
| SC-003 | met | `read_png` prints the PNG signature `137 80 78 71` and the size `3 2` of `tests/bytes/png-3x2.png`; `read_gzip` tells gzip (`1f 8b`) from PNG and text. Both written only with the reference's and `vo doc`'s functions. |
| SC-004 | met | `tests/bytes_scale.py`: 100 MB read, sliced and written: 0.20 s as bytes, 0.40 s as text (no UTF-8 check on bytes; limit 2×), within the default memory limit; `to_hex` 0.20 s and `to_base64` 0.16 s for 100 MB (limit 1 s). |
| SC-005 | met | A golden case per new error path: `index_range`, `slice_range` (R0003), `from_ints_range` (R0007), `utf8_forms` and `to_text` (invalid UTF-8), `decode_errors` (hex, base64), `type_errors`, `set_index` (check errors), and the strict-text cases. |
| SC-006 | met | Reference: about 12,459 tokens, +99 (estimate +170; budget 15,000). |
| SC-007 | met | `tests/bytes_fuzz.py`: 43,000 seeded inputs (10,000 each through `read_file`, `read_line`, process text output and an HTTP text body; 1,000 each through `read_all`, `args`, `env`), 0 failures: every result is either the text Python's strict decoder gives or an error at the offset where its error starts. `make test` runs 8,600 of them, `make gate` all. |
| SC-008 | met | `tests/bytes_scale.py`: a 10 MB download takes 0.05 s with `get_bytes` and 0.08 s with `get` (limit `get` + 10%). |

## Verification

| Check | Result |
|---|---|
| `make test` (x86-64) | passed: golden 741, step 211 + 228 + 35, `tls-selftest`, `math-selftest`, catalog 110 sections, reference 37 examples (about 12,459 tokens), site 44 pages / 193 examples, doc conformance (128 items, 0 failures), `json_corpus` (132 valid, 116 invalid), `math_vectors` 109,517 rows, `bytes_fuzz` 8,600 inputs, examples, diff-engines 1,000 (0 disagreements) |
| `make instrumented` | 741 of 741 under ASan and UBSan; with `VO_BIN=build-instrumented/vo`: `tests/cases/bytes_steps` 16 of 16, `bytes_fuzz.py` 8,600 inputs, 0 failures |
| ARM64, RISC-V | `make ARCH=aarch64 test` and `make ARCH=riscv64 test` (qemu): the same counts as x86-64, all passed, including every bytes case, the fuzz sample and the HTTP/HTTPS bytes step cases |
| `make gate` | not run (by request: only when asked); of its new bytes step, the full fuzz (43,000 inputs, 0 failures) and `bytes_scale.py` (all within limits) were run; the 20-fold repetition of the bytes cases was not |

## Behavior changes

Existing programs keep their results, except:

- **Text that is not UTF-8 is refused where it is read** (spec clarification, research R5): `read_file`,
  `read_line` and `env` return `err("not UTF-8 at byte N ...")` instead of a `str` holding invalid
  UTF-8; `read_all` and `args` stop with R0007; an HTTP text body or header that is not UTF-8 is an
  `err` (client) or a 400 answer without calling the handler (`serve`). Before, such a `str` reached the
  program, and `chars` and `print` gave meaningless results.
- **JSON documents that are not UTF-8** can no longer reach `parse_json` (a `str` is always valid), so
  std.json's own "invalid UTF-8" error is reached only by reading bytes; the corpus driver
  (`tests/json/driver.vo`) now reads bytes and converts with `to_text`, and the five such documents
  (`tests/json/invalid/111`–`115`) expect `not UTF-8 at byte N`.
- **`read_line` of a line that is not UTF-8** is an `err`: `replay/bytes`, which counts the bytes of such
  lines and stops at the first `err`, now prints `0` (live and replayed alike); its recording still keeps
  the raw bytes.
- **A served request whose head (request line, query or header) or text body is not UTF-8** is answered
  400 without calling the handler (`security/h7_hostile_requests`: 200 before, with garbled text).
- **The UTF-8 check is exact**: overlong 3- and 4-byte forms, surrogates and code points above U+10FFFF
  are refused, also in source files (the existing lexer error; case `lex/surrogate_source`).
- `std.process.run`'s message for binary output is now `"output is not UTF-8 (run_bytes reads it)"`
  (case `process_steps/limits`).
- `bytes` is a keyword (no program in the repository used it as a name).
- Errors raised inside `std/http.vo` show new line numbers, and the `serve` trace has a `serve_loop`
  frame (`http`, `https_steps`, `serve` cases updated, no other change).

## Size

- **vo's code**: `src/bytes.c` (about 230 lines: conversions, hex, base64, strict-text helpers), and
  extensions in the checker, compiler, VM (one instruction), rendering, `expect_eq`, property tests,
  `concurrent_map` transfer, `src/fs.c` (`__fsb`) and `src/process.c` (raw mode).
- **Library**: `std/bytes.vo` (new), and bytes functions in `std/fs.vo`, `std/process.vo`, `std/http.vo`.
- **Tests**: 32 golden cases in `tests/cases/bytes/`, 16 step cases in `tests/cases/bytes_steps/`, 3 doc
  cases, `tests/bytes_fuzz.py`, `tests/bytes_scale.py`, fixtures in `tests/bytes/` (made by
  `tests/bytes/make_fixtures.py`), helpers `allbytes` and `cat_file`, routes `/allbytes`, `/echo-bytes`
  and `/hex/HEX` on the test server.

## Deviations

- **No new recording fields (research R10 was wrong)**: recordings already store values that are not
  UTF-8 losslessly (`"bytes":true`, each byte escaped; `src/replay.c`), so bytes payloads replay exactly
  with no format change. `contracts/recording.md`'s base64 fields were not added.
- **HTTP primitives keep their `str` types (research R8 simplified)**: the raw exchange stays inside
  `std/http.vo` (split without UTF-8 checks by `__http_part`); text bodies and heads are checked before
  they reach a program, and bytes bodies are reinterpreted with the std-only `__raw_text` /
  `__bytes_of_text`. No `str` holding invalid UTF-8 reaches user code (SC-007).
- **R0003 reuses its existing message** ("Index i is out of bounds for length n.") rather than a
  bytes-specific one.
- **Import paths accept the keyword `bytes`** as a segment (`import std.bytes.from_text;`); the parser
  allows it after the first segment only.
- **File scopes are directories** (as the reference says): the cases use `"./"`.
- **`from_base64("Zg=a")`** reports position 2 (the misplaced `=`), not 3 as tasks.md said.
- **The process text error names no offset** (FR-010 lists process output among the paths naming one):
  its message names `run_bytes` instead; the fuzz test checks only that it is an error exactly when the
  output is not UTF-8.
- **Fuzz size**: 10,000 inputs per in-run path, 1,000 for the paths that need one `vo run` each
  (`read_all`, `args`, `env`), so the full run takes about 35 s.
- **Tooling**: `tests/doc_conformance.py` makes `bytes` sample values with `from_text` and imports
  `std.bytes` functions an example uses; it gives examples an empty stdin. `tests/steps.py` reads
  `\\xNN` in `env` values.
- R0010 from the new `std.fs` functions points into `std/fs.vo`, as for every `std.fs` function since spec 017.
