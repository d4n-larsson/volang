---

description: "Task list for vo-lang spec 021: Binary data"
---

# Tasks: Binary data

**Input**: Design documents from `specs/021-bytes/`

**Prerequisites**: plan.md, spec.md, research.md (R1-R13), data-model.md, contracts/language.md,
contracts/library.md, contracts/recording.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Golden and step cases land first and fail before each piece.

**Organization**:
- Setup: fixtures, the test area, results.
- Foundational: the type, its core operations, rendering, the exact UTF-8 validator, `std.bytes`
  conversions. Every story needs them.
- One phase per user story: US1–US3 are P1, US4–US7 are P2.
- Docs and verification.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Golden cases** go in `tests/cases/bytes/` (`.vo`, `.out`, `.code`, `.cmd` as in
  `tests/cases/math/`). **Step cases** go in `tests/cases/bytes_steps/<name>/steps`, run by
  `tests/steps.py`.
- **Between edits**: `make quick AREA=bytes`, or the areas the change touches. Never run two suites at
  once (they share `build/run-work/`). `make test` once at the end of a phase that changes shared
  files; the three platforms only in T040.
- **Regression rule**: existing cases keep their output, except the behavior changes listed in
  `results.md` (strict text, R5), each with its case.
- **vo's own C**: the strict warning set, explicit lengths, no UB; every `bytes` value is a `Str` object
  (research R1).
- **Shortcuts**: marked `ponytail:`.

---

## Phase 1: Setup

- [X] T001 [P] Create `specs/021-bytes/results.md` (SC-001 to SC-008, Verification, Behavior changes,
  Deviations), as in `specs/020-math/results.md`.
- [X] T002 [P] Fixtures in `tests/bytes/`, each made by a committed script
  `tests/bytes/make_fixtures.py` (deterministic):
  - `all256.bin` (bytes 0..255);
  - `png-3x2.png` (a valid 3×2 PNG; width at bytes 16–19, height at 20–23);
  - `hello.gz` (gzip of `hello\n`, magic `1f 8b`);
  - `latin1.txt` (`caf\xe9\n`, not UTF-8);
  - `utf8-invalid/`: one file per invalid form (lone continuation, overlong 2/3/4-byte, surrogate
    `ED A0 80`, above U+10FFFF `F4 90 80 80`, truncated at the end), named after the form.
- [X] T003 [P] Add the area `bytes` to `tools/quick.sh`:
  - cases: `bytes bytes_steps run doc`;
  - scripts: `doc_conformance.py`;
  - files: `src/bytes.c`, `std/bytes.vo`, `tests/bytes/*`, `tests/bytes_*.py` → `bytes`.
  - Check with `QUICK_FILES=src/bytes.c tools/quick.sh --which`.
- [X] T004 [P] Process helper `tests/process_helpers/allbytes`: a shell script that writes the bytes
  0..255 to stdout with `printf`, and the same reversed to stderr; executable, as the other helpers.

---

## Phase 2: Foundational (blocks every story)

### Tests (first, failing)

- [X] T005 [P] Golden cases in `tests/cases/bytes/`:
  - `ops`: `len`, `b[i]`, `slice`, `+`, `==`, `<` (prefix first), `sort` of a `list[bytes]`, `min`/`max`,
    an `ordered_set[bytes]`, built with `std.bytes.from_ints` and `from_text`;
  - `index_range` and `slice_range`: R0003 with the contract messages, run with `--json`;
  - `from_ints_range`: R0007 "Byte 2 is 256, not 0 to 255.";
  - `type_errors` (check `--json`): `b + "x"`, `print(b)`, `str(b)`, `for x in b`, `map[bytes, int]`,
    `set b[0] = 1;`, each with the contract's message and fix;
  - `render`: a runtime error whose locals include a 3-byte value and a 40-byte value
    (`<3 bytes: 010203>`, `<40 bytes: …...>`);
  - `empty`: `from_text("")` has length 0, equals `from_ints(new list[int])`, and renders `<0 bytes>`;
  - `keyword`: `let bytes = 1;` is the existing keyword error.
- [X] T006 [P] Golden cases for the validator, `tests/cases/bytes/utf8_forms`: `to_text` of each fixture
  in `tests/bytes/utf8-invalid/` gives `err("not UTF-8 at byte N")` with the right N; valid 1–4 byte
  sequences, including U+10FFFF and U+FFFF, give `ok`.

### Implementation

- [X] T007 Type `bytes` (research R1, R2):
  - `TY_BYTES (20)` in `src/vo.h`;
  - the keyword in `src/lex.c` and `src/parse.c`;
  - the type name in `src/types.c` (printing `bytes`);
  - the ordered constraint `C`/`T: ordered` accepts it (`src/check_generic.c`, `src/check_expr.c`);
  - map keys refuse it with the existing key-type error and a note naming `std.bytes.to_hex`.
- [X] T008 Operations in the checker (`src/check_expr.c`), per `contracts/language.md`:
  - `len`, `slice`, `+` (`OP_CONCAT`), the comparison operators, `sort`/`min`/`max`, `expect_eq`;
  - indexing `b[i]` → `int`, read only;
  - E0301/E0302 messages and fixes for mixing with `str`, `print`/`str()`, and `for` (each fix only when
    exactly one conversion fits).

  Review every `TY_STR` site (`grep -n TY_STR src/*.c`, 35 sites). For each, add `TY_BYTES` where the
  runtime behavior is the same (ordering, equality, `concurrent_map` transfer in `src/task.c`, ordered
  collections in `src/rt_coll.c`), or make sure it refuses `bytes` (text-only builtins, map keys).
- [X] T009 Runtime:
  - byte indexing: one compiled instruction in `src/compile.c` and `src/vm.c`, returning
    `(unsigned char)data[i]`, R0003 with the contract message;
  - `slice` of `bytes` uses the `str` slice without the UTF-8 boundary check (`src/rt_str.c`);
  - `+` and comparisons share the `str` code.
- [X] T010 Rendering (research R9): `<N bytes: HEX>` (first 32 bytes, then `...`; `<0 bytes>`) in
  `src/rt_render.c` for traces, locals, test reports and JSON value fields.
- [X] T011 Exact UTF-8 (research R5):
  - `utf8_seq_len` in `src/text.c` follows Unicode Table 3-7 (E0 needs A0–BF, ED needs 80–9F, F0 needs
    90–BF, F4 needs 80–8F);
  - remove its `ponytail:` note;
  - check that source-file lexing (`src/lex.c`) now reports overlong forms and surrogates with the
    existing code, and add a case `tests/cases/lex/surrogate_source` with such a source file (written by
    the case's `.cmd`, or committed as bytes).
- [X] T012 Primitives and `std.bytes` conversions (research R4):
  - `src/bytes.c`: `from_text` (copy), `to_text` (validate, `err("not UTF-8 at byte N")`), `from_ints`
    (R0007 "Byte K is V, not 0 to 255."), `to_ints`;
  - the signature codes `y` (`bytes`), `Y` (`result[bytes]`) and `I` (`list[int]`) in
    `src/check_expr.c`'s `builtin_sigs`, with the new primitive ids in `src/vo.h`, `src/rt_call.c` and
    `src/check_effects.c` (pure);
  - `std/bytes.vo`: the four conversions with `vo doc` comments and examples;
  - register the module in `STD_FILES` (`Makefile`) and `doc_modules` (`src/doc.c`).

**Checkpoint**: T005, T006 pass; `make test` passes (shared files changed), with only the lexer case new.

---

## Phase 3: User Story 1 — Read and inspect a binary file (P1) 🎯 MVP

**Goal**: read a file as bytes and compute from its bytes.

**Independent Test**: `tests/run.sh tests/cases/bytes` (the read cases).

### Tests (first, failing)

- [X] T013 [P] [US1] Golden cases (`.cmd`: `run {}` from a directory with the fixtures, as `tests/cases/fs`
  cases do), using `std.fs.read_bytes`:
  - `read_png` prints `137 80 78 71` and the width and height `3 2`;
  - `read_gzip` prints `gzip` from the magic `1f 8b`;
  - `read_all256` checks `len` 256 and `b[i] == i` for every i;
  - `read_missing` gives the same `err` as `read_file`;
  - `read_scope` is R0010 outside the `file.read` scope.

### Implementation

- [X] T014 [US1] `std.fs.read_bytes(path) -> result[bytes]` (research R6):
  - a primitive `__fsb(op, path, data: bytes) -> result[bytes]` in `src/fs.c` with op 1 = read, sharing
    the open, scope and link code of `read_file`;
  - its effect is checked by op, like `__fs`;
  - `std/fs.vo` gains the function with a `vo doc` comment.

**Checkpoint**: T013 passes.

---

## Phase 4: User Story 2 — Write binary data (P1)

**Goal**: build bytes and write or append them to a file.

**Independent Test**: `python3 tests/steps.py tests/cases/bytes_steps`, the write cases.

### Tests (first, failing)

- [X] T015 [P] [US2] Step cases in `tests/cases/bytes_steps/`:
  - `write_ints`: `from_ints` of 0..255 written with `write_bytes`; `cmp` against `all256.bin` through
    an `expected/` file;
  - `write_text`: `from_text("héllo")` written; the file holds its 6 UTF-8 bytes;
  - `append`: two `append_bytes` calls;
  - `join_slice`: `+` and `slice` leave the originals unchanged (printed with `to_hex`);
  - `write_scope`: outside `file.write` is R0010.

### Implementation

- [X] T016 [US2] `write_bytes` and `append_bytes` in `std/fs.vo` (ops 2 and 3 of `__fsb`), sharing
  `write_file`/`append_file`'s code in `src/fs.c` (the same atomic-replace and link rules).

**Checkpoint**: T015 passes.

---

## Phase 5: User Story 3 — Run programs with binary output (P1)

**Goal**: process output as bytes; spec 017's limit lifted.

**Independent Test**: the process step cases.

### Tests (first, failing)

- [X] T017 [P] [US3] Step cases in `tests/cases/bytes_steps/`:
  - `run_bytes`: `allbytes` gives 256 bytes on stdout and the reverse on stderr, and the exit code;
  - `run_with_bytes`: `cat` gets a binary input and echoes it unchanged;
  - `run_text_binary`: `run` of `allbytes` is still an `err`, with the new message
    `"output is not UTF-8 (run_bytes reads it)"`;
  - `run_bytes_record`: record, then replay offline; replay gives the same bytes (the recording has
    `stdout_base64`).

### Implementation

- [X] T018 [US3] `__run_bytes` in `src/process.c`, sharing `__run`'s argument, grant and timeout code and
  returning the raw output.
  - `record BytesOutput`, `run_bytes` and `run_with_bytes` in `std/process.vo` with `vo doc` comments;
  - the text `run`'s message gains `(run_bytes reads it)`; update the spec 017 case that shows it.
- [X] T019 [US3] Recordings (`contracts/recording.md`): `run` events written by the bytes functions carry
  `stdout_base64`/`stderr_base64` (`src/replay.c`), with base64 encode and decode shared with
  `src/bytes.c`.
  - Replay reads them back exactly.
  - A text `run` replayed against a bytes event, or the reverse, is R0014.

**Checkpoint**: T017 passes.

---

## Phase 6: User Story 4 — Convert between bytes and text forms (P2)

**Goal**: hex, base64 and UTF-8 text conversions.

**Independent Test**: the conversion golden cases.

### Tests (first, failing)

- [X] T020 [P] [US4] Golden cases:
  - `base64_rfc`: the 7 RFC 4648 vectors (`""`, `f`, `fo`, `foo`, `foob`, `fooba`, `foobar`), both ways;
  - `hex`: `to_hex` of `all256.bin` and back; uppercase accepted;
  - `decode_errors`: `from_hex("0g")` → position 1, `from_hex("abc")` → odd count, `from_base64("Zm9v!")`
    → position 4, `from_base64("Zm9")` → length, `from_base64("Zg=a")` → position 3;
  - `to_text`: valid → `ok`, `latin1.txt`'s bytes → `err("not UTF-8 at byte 3")`;
  - `stdin_stdout`: `read_stdin` of a binary `.in` file, `write_stdout` of it unchanged (`.out` compared
    as bytes).

### Implementation

- [X] T021 [US4] Hex and base64 in `src/bytes.c` (single pass, one allocation; decoding names the first
  invalid position), and `read_stdin` (shares `read_all`'s reading, without the check) and
  `write_stdout` (shares `write`'s output), with their effects.
  - `std/bytes.vo` gains them with `vo doc` comments.
  - Recordings: `read_all` as bytes records `value_base64` (T019's helpers).

**Checkpoint**: T020 passes.

---

## Phase 7: User Story 5 — Compare and test binary data (P2)

**Goal**: useful test failures and generated bytes.

**Independent Test**: the test-runner golden cases.

### Tests (first, failing)

- [X] T022 [P] [US5] Golden cases (`test --json {}`):
  - `expect_eq_bytes`: two 2,000-byte values that differ at offset 1,000; the report names byte 1000,
    both bytes, and 8 bytes of context in hex;
  - `prop_bytes`: a property over `b: bytes` (`len(from_hex(to_hex(b))) == len(b)`) passes, and a
    failing one (`len(b) < 3`) shrinks to a 3-byte `<3 bytes: 000000>`.

### Implementation

- [X] T023 [US5] `expect_eq` on `bytes`: the first differing offset (or length difference), in
  `src/rt_test.c`, beside the `str` case.
- [X] T024 [US5] Property generation and shrinking for `bytes` in `src/prop.c`: lengths from the size
  schedule, elements biased to 0, 255 and printable ASCII; shrink shorter first, then smaller elements.

**Checkpoint**: T022 passes.

---

## Phase 8: User Story 6 — Download and serve binary content over HTTP (P2)

**Goal**: bytes bodies over HTTP and HTTPS, client and server, recorded.

**Independent Test**: the HTTP step cases.

### Tests (first, failing)

- [X] T025 [P] [US6] Test servers: `tests/http_server.py` (and so `tests/https_server.py`) gain
  `/allbytes` (bytes 0..255, `application/octet-stream`) and `/echo-bytes` (echoes a POST body).
- [X] T026 [P] [US6] Step cases in `tests/cases/bytes_steps/`:
  - `get_bytes` and `get_bytes_https` (`/allbytes` written to a file, compared);
  - `post_bytes` (binary body echoed back);
  - `get_text_binary`: `get` of `/allbytes` is `err("the body is not UTF-8 at byte 128 (get_bytes reads it)")`;
  - `serve_bytes`: a handler answers 256 bytes, the case's client checks them and the length header;
  - `serve_text_binary`: a binary POST to a text `serve` handler is answered 400, and the handler is
    not called;
  - `http_record_replay`: record a `get_bytes` and a text `get`, then replay offline (both
    `response_base64`);
  - the existing HTTP and HTTPS step cases stay unchanged (run them).

### Implementation

- [X] T027 [US6] Raw exchange as bytes (research R8):
  - `__http_exchange`, `__https_exchange` and `__serve_next` return `result[bytes]`; `__serve_reply`
    takes `bytes`; `__http_part` splits `bytes` (`src/net.c`, `src/serve.c`, `src/tls.c`, the signature
    table);
  - `std/http.vo` parses the head through `to_text` (a non-UTF-8 header: `err("a header is not
    UTF-8")`) and converts text bodies with `to_text` (the R5 message);
  - `serve` answers 400 for a non-UTF-8 text request body.
- [X] T028 [US6] Bytes API in `std/http.vo`: `BytesResponse`, `BytesRequest`, `get_bytes`, `post_bytes`
  and `serve_bytes` (same effects and scopes), with `vo doc` comments; examples that run offline (the
  serve address error, as `serve`'s example does).
- [X] T029 [US6] Recordings: `http`, `request` and `response` events write `request_base64` and
  `response_base64` (`src/replay.c`). Replay reads both these and the old text fields, so existing HTTP
  recordings and `tests/cross_replay.py` still pass.

**Checkpoint**: T026 passes, and every existing `http`, `https`, `serve` and `replay` case is unchanged.

---

## Phase 9: User Story 7 — Text stays text (P2)

**Goal**: every `str` is valid UTF-8; each input path checks.

**Independent Test**: the strict-text golden cases and `tests/bytes_fuzz.py`.

### Tests (first, failing)

- [X] T030 [P] [US7] Golden cases in `tests/cases/bytes/` (contract messages, `--json` for spans):
  - `strict_read_file` (`latin1.txt`: `err` naming byte 3 and `read_bytes`);
  - `strict_read_line` and `strict_read_all` (binary `.in`; `read_all` is R0007);
  - `strict_args` (a `.cmd` passing a non-UTF-8 argument through `printf`, or a step case if the case
    runner cannot);
  - `strict_env` (a step case with `env NAME=\xff`);
  - `utf8_unchanged`: a UTF-8 file with every 1–4 byte length reads exactly as before.
- [X] T031 [P] [US7] `tests/bytes_fuzz.py` (SC-007): 10,000 seeded random byte strings (half valid UTF-8
  with corruptions, half random) through each path:
  - `read_file` (files), `read_line`/`read_all` (stdin), `args`, `env`, process text output, HTTP text
    body (local server);
  - each time, either an error naming an offset that Python's strict decoder agrees is the first invalid
    byte, or a `str` equal to Python's decode.

  Added to `make test`'s script list.

### Implementation

- [X] T032 [US7] Checks at each input point (research R5):
  - `read_file` and `read_line` (`src/rt_io.c` and their replay paths in `src/replay.c`), `read_all`
    and `args` (R0007 at the call), `env`;
  - the messages exactly as in `contracts/library.md`;
  - a replayed run checks the recorded data the same way.
  - The behavior changes go in `results.md`.

**Checkpoint**: T030 and T031 pass; `make test` passes, with only the listed behavior changes.

---

## Phase 10: Polish and verification

- [X] T033 [P] Reference (`docs/reference.md`):
  - the `bytes` row in the types table;
  - a short operations paragraph (contract text);
  - the strict-text sentence;
  - `std.bytes` in the modules table;
  - the bytes functions named in the `std.fs`, `std.process` and `std.http` rows.

  Record the token growth (estimate +170, limit +200) in `results.md`.
- [X] T034 [P] `docs/errors.md`: the new R0003 and R0007 messages, and the E0301 fixes, in their
  sections; `tests/catalog.py` passes.
- [X] T035 [P] Site and roadmap:
  - `site/docs/guide/`: a "Binary data" section (reading, writing, hex/base64, processes, HTTP, strict
    text);
  - `docs/roadmap.md`: spec 021 **Done**, with "Not done" (literal, `for`, map keys, base64url,
    streaming, non-UTF-8 directory names), and the "Binary data" gap row removed;
  - `README.md`: the spec list;
  - `docs/security.md`: recordings are now lossless for binary data.
- [X] T036 [P] `tests/bytes_scale.py` (SC-004, SC-008):
  - 100 MB read, slice, write as bytes against the same as text (at most 2×), within the default
    memory limit;
  - hex and base64 of 100 MB under 1 s each;
  - a 10 MB `get_bytes` against `get` of a text body (at most 1×, plus 10% noise).

  Added to `make gate`.
- [X] T037 Gate in `Makefile`: `make test` includes `bytes_fuzz.py`; `make gate` adds "bytes determinism"
  (`tests/run.sh --repeat 20 tests/cases/bytes` and `steps.py --repeat 20 tests/cases/bytes_steps`) and
  `bytes_scale.py`.
- [X] T038 [P] `vo doc` cases in `tests/cases/doc/` (`bytes_module`, `bytes_item`, `doc/all` updated);
  `tests/doc_conformance.py` passes, with every new item's example run.
- [X] T039 `make test` on x86-64; `make instrumented`.
- [X] T040 Cross-platform, once: `make ARCH=aarch64 test` and `make ARCH=riscv64 test`, run one after the
  other (never at the same time); `tests/cross_replay.py` with the new recordings.
- [X] T041 Record everything in `results.md` (SC-001 to SC-008, the behavior changes, sizes,
  deviations); set the spec status to Implemented; mark every task [X]. `make gate` only if the user
  asks for it.

---

## Dependencies & Execution Order

- **Phase 1** (T001–T004): all in parallel.
- **Phase 2** (T005–T012): T005 and T006 first (failing). Then T007 → T008 → T009, with T010 and T011
  alongside T009, and T012 after T008 (signature codes). Blocks every story.
- **US1** (T013–T014): after Phase 2.
- **US2** (T015–T016): after US1's T014 (shares `__fsb`).
- **US3** (T017–T019): after Phase 2; T019 needs base64 (T021's encoder, or one written first in
  `src/bytes.c` and shared).
- **US4** (T020–T021): after Phase 2; independent of US1–US3.
- **US5** (T022–T024): after Phase 2.
- **US6** (T025–T029): after US4 (`to_text` messages, base64 for recordings).
- **US7** (T030–T032): after Phase 2. T032 is best done after US6, so that HTTP text bodies are checked
  by T027.
- **Polish** (T033–T041): after the stories; T039–T041 last, in order.

## Parallel Examples

- Phase 1: T001, T002, T003 and T004 together.
- After Phase 2: US1 → US2 in one stream; US3, US4 and US5 in others (different files: `src/process.c`,
  `src/bytes.c`, `src/rt_test.c` + `src/prop.c`).
- Polish: T033, T034, T035, T036 and T038 together.
- Never two test suites at once: write in parallel, run one at a time.

## Implementation Strategy

- **MVP**: Phases 1–3. The type, its operations and reading binary files.
- **Next**: US2 and US3 (writing, process output); then US4 (hex, base64, stdin/stdout).
- **Then**: US5 (tests), US6 (HTTP), US7 (strict text, the behavior change, last so it lands with every
  bytes alternative in place).
- **Last**: docs, scale, `make test`, instrumented, then the three platforms once.
