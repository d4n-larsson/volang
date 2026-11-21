---

description: "Task list for vo-lang spec 036: The Toolchain as an Agent Server (vo mcp)"
---

# Tasks: The Toolchain as an Agent Server (`vo mcp`)

**Input**: Design documents from `specs/036-vo-mcp/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R9), data-model.md, contracts/mcp.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's cases land in `tests/mcp.py` first and fail before
its code.

**Organization**:
- Setup: the test client, wiring, the results file.
- Foundational: the command, the line reader, the JSON reader, the reply writer, the protocol's own
  methods.
- US1 (P1) tools: the table, the call runner, results; US2 (P1) isolation and limits; US3 (P2) the
  reference resource; US4 (P2) malformed messages; US5 (P3) documentation.
- Polish: the long session, timing, three platforms, one host by hand, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Everything protocol-visible** (methods, error codes, tool arguments, result shape, limits):
  contracts/mcp.md. Texts of messages: research.md.
- **The test client**: `tests/mcp.py` starts `bin/vo mcp` (so `VO_BIN` and qemu work) in a temporary
  copy of a small project, sends lines, reads one answer per request with a timeout, and prints
  `FAIL <case>: ...` per failure and a one-line summary; exit 1 on any failure.
- **Unchanged programs**: no existing golden case may change except those that print the list of
  commands (T0001 "Missing command"/"Unknown command"), which gain `mcp`.
- **Reference budget**: `python3 tests/ref_examples.py` after the reference change; at most +60 tokens
  (from about 14,755).
- **Between edits**: `make quick AREA=mcp` (T001); `make test` once, through
  `tools/progress.py run test`; the gate only when asked. Never two suites at once.

---

## Phase 1: Setup

- [X] T001 `tools/quick.sh`: a new area `mcp` (the `cli` cases and `python3 tests/mcp.py`); `Makefile`:
  `src/mcp.c` in the sources and `tests/mcp.py` in the `make test` script list
- [X] T002 `tests/mcp.py`: the client described in Conventions, with helpers `session(lines)`,
  `call(tool, arguments)` and `direct(args)` (runs `bin/vo ARGS` in the same directory and returns
  stdout, stderr, status); the project fixture written by the script (a `main.vo` with a function, a
  record, a passing and a failing test, `uses console`; a file with a check error; a file that is not
  canonical); options `--session N` and `--time` reserved
- [X] T003 `specs/036-vo-mcp/results.md`: the skeleton (SC-001 to SC-006, three platforms, the gate
  line "not run")

---

## Phase 2: Foundational (blocks every story)

- [X] T004 `tests/mcp.py`: cases `initialize` (answer has `protocolVersion` echoing each of
  `2025-06-18`, `2025-03-26`, `2024-11-05`; an unknown revision gets `2025-06-18`; `serverInfo.name`
  `vo`, version equal to `vo version`; capabilities `tools` and `resources`), `ping` before and after
  `initialize`, notifications get no answer, end of input gives exit status 0, and stdout is one JSON
  object per line with `"jsonrpc":"2.0"`. `tests/cases/cli/mcp_args.cmd` (`mcp extra`) and
  `mcp_json.cmd` (`mcp --json`): T0001, exit 64. All fail
- [X] T005 `src/main.c`, `src/vo.h`: `mcp` in `cmd_names` with `CMD_MCP`; dispatched right after the
  command name is recognized, before the option loop: `argc != 2` is T0001 "`mcp` takes no arguments:
  it serves the Model Context Protocol on standard input and output."; else `return cmd_mcp(argv,
  env)`. Add `mcp` to the two messages that list the commands
- [X] T006 `src/mcp.c`: the input buffer (4 MiB + 64 KiB, allocated once) and `next_line()` returning
  the next complete line (a line over 4 MiB is skipped to its newline and reported as oversized; end of
  input with a partial last line treats it as a line); the in-place JSON reader: `js_skip(p)` (the end
  of the value at `p`, 0 when malformed, nesting depth limit 64), `js_get(object, key)` (the value's
  start or 0), `js_string(p, &len)` (unescapes in place, `\uXXXX` with surrogate pairs to UTF-8),
  `js_type(p)`; the whole line must be exactly one value
- [X] T007 `src/mcp.c`: the reply writer: a 64 KiB buffer flushed to descriptor 1 (a failed write sets
  `host_gone`); `w_raw`, `w_json_string(bytes, n, &replaced)` (escapes `"`, `\`, control characters;
  bytes that are not valid UTF-8, including overlong forms and surrogates, become U+FFFD and are
  counted); `reply_begin(id)` / `reply_end()` and `reply_error(id, code, message)` where `id` is the
  request's raw JSON text (or `null`)
- [X] T008 `src/mcp.c`: `cmd_mcp`: the loop over lines; per line: parse (`-32700`), must be an object
  with a string `method` (`-32600`; an array is `-32600`); `initialize`, `ping`,
  `notifications/*` (ignored), `resources/templates/list`, unknown method (`-32601` only when there is
  an `id`), and `-32600` "the session has not been initialized" for anything but `initialize` and
  `ping` before `initialize`; exit 0 at end of input. T004 passes

**Checkpoint**: `printf` of an `initialize` line into `vo mcp` prints one answer and exits 0.

---

## Phase 3: User Story 1 - An agent finds and uses the toolchain (P1) 🎯 MVP

**Goal**: ten tools listed with schemas; a call returns what the command returns.

**Independent Test**: `python3 tests/mcp.py` equivalence and validation cases.

- [X] T009 [US1] `tests/mcp.py`: `tools/list` has exactly the ten names of contracts/mcp.md, each with
  a `description`, an `inputSchema` (`type: object`, `properties`, `required`,
  `additionalProperties: false`) whose properties and required names equal the contract's table (the
  table is repeated in the test as data), and the hints of the contract. Fails
- [X] T010 [US1] `tests/mcp.py`: equivalence cases, each comparing the result's blocks and exit line
  with `direct(...)`: `check` (clean, check error, a directory, a missing file → 66); `run` (output,
  `args`, `stdin`, no stdin when the program reads, `seed`, `clock_ms`, `timeout`, `max_memory`,
  `mem_stats`, runtime error → 70, `exit(3)`); `test` (pass and fail → 1, `filter`, `tests`, `mode`
  each, `max_mutants`, `diff` against `--diff FILE` of the same text); `fmt` (`check` true → 1, then
  rewriting, compared on two copies of the project); `effects` (plain, `update`); `explain` (known,
  unknown → 64 with `isError` true); `doc` (none, `name` only, `entry` and `name`); `query` (each
  question); `impact` (`diff` text against `--diff FILE`); `edit` (`rename` with `dry_run`, then for
  real on two copies; `add-field`). `isError` is true exactly for status 64 and 125. Fails
- [X] T011 [US1] `tests/mcp.py`: validation cases, each `-32602` with the argument's name in the
  message and no file changed: unknown tool; unknown argument; `paths` a string; `paths` empty;
  `timeout` a string; `mem_stats` a number; missing `file`; `question` outside its choices; an operand
  `--update` in `check.paths`; `arguments` missing for a tool with a required argument; `arguments`
  not an object. A number out of the command's range (`max_memory: 0`) is a result with status 64, not
  a protocol error. Fails
- [X] T012 [US1] `src/mcp.c`: the tool table per data-model.md (name, description, hint, arguments with
  name, type, required, kind, word, choices, description), filled from contracts/mcp.md; confirm
  `edit add-field`'s words against `src/edit.c` and `test --diff -` against `src/main.c`/`src/query.c`
  and correct the contract if they differ; `tools/list` printed from the table. T009 passes
- [X] T013 [US1] `src/mcp.c`: `tools/call`: find the tool; validate `arguments` against the table
  (every rule of T011); build the argument vector (`vo`, the tool, `--json`, options in table order,
  operands, then `--` and the program arguments) in a fixed array of 256 words (more is `-32602`);
  the stdin text and `--diff -`. T011 passes
- [X] T014 [US1] `src/mcp.c`: the call runner per research R3: three pipes; fork; the child sets its own
  process group, `sys_die_with_parent`, puts the pipes on 0, 1, 2, closes every other descriptor and
  calls `vo_exit(vo_main(argc, argv, env))`; the parent writes the stdin text without blocking, reads
  stdout and stderr into the two 1 MiB buffers with `ppoll`, reaps the child, kills its group, and
  writes the result per contracts/mcp.md "Results" (blocks only when not empty; the exit line; a child
  ended by a signal is status 125 with stderr `vo: the command was ended by signal N`). Verify the
  note in R3 (nothing `vo_main` relies on is changed before the fork). T010 passes

**Checkpoint**: every tool equals its command.

---

## Phase 4: User Story 2 - One call cannot hurt the next (P1)

**Goal**: timeouts, memory exhaustion, crashes, floods and cancellations stay inside their call.

**Independent Test**: `python3 tests/mcp.py` isolation cases.

- [X] T015 [US2] `tests/mcp.py`: isolation cases in one session: `run` of a loop with `timeout: 1`
  (R0009, 124); of unbounded allocation with `max_memory: 8` (R0006, 70); `exit(3)`; a runtime error;
  then `check` of the clean file equal to `direct`; the first `run` repeated at the end gives the same
  result as at the start. A program printing 3 MiB to stdout: the block is the first 1 MiB (1,048,576
  bytes) and the exit line has `stdout_omitted` 2,097,152; the same for stderr; a program printing the
  bytes `ff fe`: U+FFFD twice and `replaced: 2`. Fails where not yet true
- [X] T016 [US2] `tests/mcp.py`: cancellation: a `run` with `timeout: 0` of a loop, then
  `notifications/cancelled` with its id, then `ping`: no answer for the call, the ping is answered
  within 2 s, and no process of the group remains (`/proc` scan for the child's command line); a
  cancellation with another id is ignored. Two calls sent together are answered in order. End of
  input sent right after two calls: both are answered, exit 0. Closing the server's stdout during a
  long `run` and then its stdin: the server exits within 2 s and the program's process is gone;
  SIGKILL of the server during a long `run`: the program's process is gone. Fails
- [X] T017 [US2] `src/mcp.c`: output cut and count (reading on past 1 MiB without keeping); while a call
  runs, `ppoll` also on descriptor 0: new bytes go to the input buffer (not read when it is full);
  complete lines are scanned once each for `notifications/cancelled` with `params.requestId` equal to
  the running id (compared as raw JSON text) → kill the group, reap, no answer; the line is marked
  handled; other lines wait. End of input: finish the call and the lines already read. `host_gone`:
  kill the running group and exit 0. T015 and T016 pass

**Checkpoint**: the session survives everything in T015–T016.

---

## Phase 5: User Story 3 - The reference is one request away (P2)

**Independent Test**: `python3 tests/mcp.py` resource cases.

- [X] T018 [US3] `tests/mcp.py`: `resources/list` equals the contract's entry; `resources/read` of
  `vo://reference` has text equal to `direct(["reference"])` stdout and `mimeType` `text/markdown`;
  another address and a missing `uri` are `-32002` and `-32602`. Fails
- [X] T019 [US3] `src/mcp.c`: `resources/list` and `resources/read` from the embedded reference text
  `cmd_reference` prints (expose it from `src/main.c` if it is not reachable). T018 passes

---

## Phase 6: User Story 4 - A protocol that does not break (P2)

**Independent Test**: `python3 tests/mcp.py` protocol cases.

- [X] T020 [US4] `tests/mcp.py`: each followed by a `ping` that must be answered: a line that is not
  JSON (`-32700`, id `null`); `[]` and a batch of two requests (`-32600`); `42`; an object without
  `method`; a request with an unknown method (`-32601`) and the same as a notification (no answer);
  `tools/call` before `initialize` (`-32600`); a line of 5 MiB (`-32600`, id `null`); an empty line and
  a line of spaces (ignored); a string id and a number id echoed exactly (`"id":"a\"b"`, `"id":7`);
  trailing text after the object (`-32700`); nesting 100 deep (`-32700`); a `\u0000` and a lone
  surrogate in a string argument (a valid answer either way). Fix whatever fails in `src/mcp.c`
- [X] T021 [US4] `tests/bytes_fuzz.py`-style run inside `tests/mcp.py --fuzz N` (not in `make test`):
  N random and mutated lines; the server must answer a final `ping` and exit 0, with every output line
  valid JSON

---

## Phase 7: User Story 5 - Documented where hosts and agents read (P3)

- [X] T022 [US5] `docs/reference.md`: `vo mcp` in the command list and one bullet (what it serves: the
  commands as tools with the same output, `vo://reference`); `python3 tests/ref_examples.py` stays
  under 15,000 tokens (+≤ 60)
- [X] T023 [P] [US5] `site/docs/tools/mcp.md` (new) per research R8: host configuration, the tools
  table, the result shape, the limits, what is not offered; `tools/site.py` TOOLS gains `mcp`;
  `make site-check`
- [X] T024 [P] [US5] `docs/roadmap.md`, `README.md` (one line), `CHANGELOG.md` (Unreleased),
  `docs/review-2026-11/README.md` (the row for the MCP server)

---

## Phase 8: Polish

- [X] T025 Re-accept the golden cases that print the command list after reading each diff; `make test`
  once through `tools/progress.py run test`
- [X] T026 `tests/mcp.py --session 1000` (SC-002: the server's resident size after 1,000 mixed calls
  is no higher than after 10) and `--time` (SC-003: paired with direct commands, median difference
  ≤ 5 ms); record in results.md
- [X] T027 `VO_BIN=build-aarch64/vo` and `VO_BIN=build-riscv64/vo python3 tests/mcp.py` (FR-017);
  record
- [ ] T028 SC-005 by hand per quickstart.md "A host", if a host is available in this environment;
  otherwise record it as not done and leave the steps for the user
- [X] T029 `specs/036-vo-mcp/results.md` completed; spec status "Implemented (gate not run)"

---

## Dependencies

- Setup → Foundational → US1 → US2. US3 and US4 need only Foundational; US5 needs US1–US3 for its
  facts. Polish last.
- Within a story: the test task before the code task.

## Parallel opportunities

- T023 and T024 (different files). The C work is one file and is sequential.

## Implementation strategy

MVP is Foundational plus US1: an agent can list and call the tools. US2 makes it safe to leave running;
US3 and US4 are small; US5 and Polish close it.
