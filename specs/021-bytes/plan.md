# Implementation Plan: Binary data

**Branch**: `021-bytes` | **Date**: 2026-11-18 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/021-bytes/spec.md`

## Summary

A built-in type `bytes`, with the same runtime object as `str`: a length and the data. Only the checker
tells the two apart (research R1), so most runtime code is shared. Text operations simply refuse
`bytes`, and the byte operations skip the UTF-8 checks.

**Core** (R3): `len`, `slice`, indexing to `int`, `+`, `==`, ordering, `sort`/`min`/`max`/ordered
collections, `expect_eq` and property tests. There is no literal, no `for`, and `bytes` cannot be a map
key.

**Library** (R4): one new module, `std.bytes`, with conversions (`from_text`, `to_text`, `from_ints`,
`to_ints`), `to_hex`/`from_hex`, `to_base64`/`from_base64`, and standard input and output as bytes. No
new builtin names, so no name is taken from existing programs; only the keyword `bytes` is new.

**Input and output** (R6–R8):
- `std.fs`: `read_bytes`, `write_bytes`, `append_bytes`.
- `std.process`: `run_bytes`, `run_with_bytes`.
- `std.http`: `get_bytes`, `post_bytes`, `serve_bytes`, with records whose `body` is `bytes`.

  The text functions stay, unchanged for UTF-8 data.

**Strict text** (R5): every path from outside data to a `str` checks UTF-8:
- `read_file`, `read_line`, `env`, HTTP text bodies and headers, and process text output return `err`;
- `read_all` and `args` stop with R0007.

Each message names the byte offset and the bytes function to use instead.

**Recordings** (R10): byte payloads are stored as base64 in new fields. Text fields keep their form, so
existing recordings still replay.

## Technical Context

**Language/Version**: C (C11, freestanding) for vo's code; vo for `std/bytes.vo` and the changes to
`std/fs.vo`, `std/process.vo` and `std/http.vo`; Python 3 for test scripts.

**Primary Dependencies**: none new.

**Storage**: recordings gain optional base64 fields (schema 1, fields added only).

**Testing**:
- golden cases `tests/cases/bytes/`, one per operation, edge and error path, plus the strict-text cases;
- step cases `tests/cases/bytes_steps/`: files, process output, HTTP and HTTPS against the local test
  servers, record and replay;
- `tests/bytes_fuzz.py` (SC-007): 10,000 random inputs through every text path;
- the RFC 4648 vectors;
- `make quick AREA=bytes` between edits (a new area); `make test` once at the end; the three platforms at
  the end of the spec.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler and runtime of a programming language, as a CLI.

**Performance Goals**:
- SC-004: reading, slicing and writing 100 MB of bytes takes at most 2× the text path (the same code
  without the UTF-8 check, so it should be at most 1×);
- hex and base64 are in C, at 100 MB in under 1 s;
- SC-008: a 10 MB download as bytes takes no longer than as text.

**Constraints**:
- Principle I: one form per construct. Text functions and bytes functions are separate names, never one
  function returning either.
- Principle II: no new codes. R0003 and R0007 get new messages, and E0301's fix names the conversion.
- Principle III: UTF-8 validity is defined by the Unicode standard (overlong forms and surrogates are
  invalid), and the same on every platform.
- Principle V: reference +200 tokens at most, estimated +170.

**Scale/Scope**:
- C: a new type kind in the checker (about 35 `TY_STR` sites to review, about 15 to extend), byte
  indexing in the compiler and VM, about 8 primitives in `src/bytes.c` (about 400 lines), UTF-8 checks
  at about 8 input points, rendering, property generation, recordings (about 150 lines);
- vo: `std/bytes.vo` (about 120 lines), plus about 150 changed lines in `std/fs.vo`,
  `std/process.vo` and `std/http.vo`;
- about 45 golden cases, about 20 step cases, 1 fuzz script.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Gate | Status |
|---|---|
| I. Agent-first: one form per construct, no implicit conversion | PASS. `bytes` and `str` never mix implicitly (E0301 with a fix naming the conversion). Bytes I/O are separate functions, so there is no mode argument and no union result. |
| II. Machine-readable feedback | PASS. Existing codes R0003, R0007 and E0301 get new messages that name the offset and the function to use. Recordings change only by added fields, within schema 1. |
| III. Determinism and defined behavior | PASS. Every byte operation is defined: out of range is R0003, an element outside 0–255 is R0007, invalid input is an `err` with its offset. Recordings become lossless for binary data, which they are not today. |
| IV. Test-first through the CLI | PASS. Golden and step cases come first. |
| V. Single source, minimal surface | PASS. One keyword, no builtin names; the library is documented by `vo doc`; reference estimate +170 tokens. YAGNI: no literal, no `for`, no map keys, no base64url, no streaming. |
| Implementation constraints | PASS. C only, no new vendored code, freestanding. |
| Behavior change | STATED. Non-UTF-8 text input that used to become a corrupted `str` is now an `err` or R0007 (R5), listed in the change and in `results.md`. |

Post-design re-check: unchanged, all PASS.

## Project Structure

### Documentation (this feature)

```text
specs/021-bytes/
├── plan.md, research.md, data-model.md, quickstart.md
├── contracts/language.md        # the type, operations, rendering, errors
├── contracts/library.md         # std.bytes, std.fs, std.process, std.http additions
├── contracts/recording.md       # the base64 fields
└── tasks.md                     (/speckit-tasks)
```

### Source Code (repository root)

```text
src/lex.c, src/parse.c, src/types.c        # keyword and type `bytes` (TY_BYTES)
src/check_expr.c, src/check*.c             # operators, indexing, builtin overloads, E0301 fixes
src/compile.c, src/vm.c                    # byte indexing (one instruction), the rest shared with str
src/bytes.c                                # new: primitives (conversions, hex, base64, I/O as bytes), utf8 checks
src/rt_io.c, src/replay.c, src/main.c      # strict UTF-8 at read_file/read_line/read_all/env/args; base64 recording fields
src/process.c, src/net.c, src/serve.c      # raw I/O as bytes
src/rt_render.c, src/rt_test.c, src/prop.c, src/task.c   # rendering, expect_eq offset, generation, concurrent_map transfer
std/bytes.vo (new), std/fs.vo, std/process.vo, std/http.vo
tests/cases/bytes/, tests/cases/bytes_steps/, tests/bytes_fuzz.py, tests/bytes/ (fixtures: PNG, gzip, 0..255)
tools/quick.sh                             # area `bytes`
docs/reference.md, docs/errors.md          # type, operations, strict text; messages
```

**Structure Decision**: one new C file for the byte primitives and the UTF-8 boundary helper; everything
else extends the existing files, beside the `str` cases.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|---|---|---|
| Parallel bytes functions in std.fs, std.process and std.http (`read_bytes` beside the text functions, `BytesResponse` beside `Response`) | Text must stay `str` for existing programs, and Principle I forbids one function that returns either | Changing `Response.body` to `bytes`: it breaks every HTTP program, and every caller would need `to_text`. A mode argument: a union result, against Principle I. |
| Behavior change (strict UTF-8) | Chosen in the spec's clarification: every `str` valid, so text operations are defined | Keeping lenient text: `chars` and `print` stay meaningless on such strings. |
