# Implementation Plan: The Toolchain as an Agent Server (`vo mcp`)

**Branch**: `036-vo-mcp` (work on `main`, as for specs 001–035) | **Date**: 2026-11-21 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/036-vo-mcp/spec.md`

## Summary

A new command, `vo mcp`, in one new file `src/mcp.c`. The server reads one JSON-RPC message per line
(R1), answers the protocol's own methods itself (R2), and runs every tool call as the ordinary command
in a forked copy of itself whose input, output and error are pipes (R3). Isolation, limits, the grant
and "the same output as the command" all follow from that one decision: the call *is* the command. The
tools and their arguments are one static table that produces the tool list, validates a call and builds
the command's argument vector (R4). The server itself allocates nothing per message (R5).

| Part | File | What |
|---|---|---|
| Command | `src/main.c`, `src/vo.h` | `mcp` in the command table; dispatched before option parsing; takes no arguments (T0001) |
| Server | `src/mcp.c` (new) | line reader, in-place JSON reader, replies, the tool table, the call runner |
| Docs | `docs/reference.md`, `site/docs/tools/mcp.md` (new), `tools/site.py`, `docs/roadmap.md`, `README.md`, `CHANGELOG.md`, `docs/review-2026-11/README.md` | R8 |
| Tests | `tests/mcp.py` (new, in `make test`), `tests/cases/cli/` (usage errors) | R9 |

## Technical Context

**Language/Version**: C (C11, freestanding, no libc).

**Primary Dependencies**: none new. `fork`, `pipe`, `ppoll`, `wait4`, `kill` and the death-with-parent
setting are already used by `std.process` (`src/process.c`) and the server workers.

**Storage**: none.

**Testing**: a scripted client (`tests/mcp.py`) compares every tool with the direct command; `make
quick` between edits, `make test` once per change; the gate only when asked.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler toolchain (CLI).

**Performance Goals**: a call costs the command plus one `fork` (SC-003: ≤ 5 ms over the direct
command; a direct command pays an `exec` instead).

**Constraints**: stdout holds only protocol messages (FR-003); the server's memory is fixed (FR-011,
SC-002); a call can never pass an option the caller did not name (R4); reference ≤ 15,000 tokens
(+≤ 60 here; 14,755 now).

**Scale/Scope**: about 550 lines of C, 300 of Python tests, no new error code (R7).

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | The toolchain presents itself to an agent: ten tools named as the commands, arguments named as the options. Nothing new to learn; nothing is offered twice. | Pass |
| II. Machine-readable feedback | A result is the command's schema-1 output, byte for byte, plus one `exit` line. No second format. Protocol mistakes use JSON-RPC's codes with a message naming the argument. | Pass |
| III. Determinism | A call is the command in a fresh process: same seed, clock and limits as the command line; no state is shared between calls, so a result does not depend on the session. | Pass |
| IV. Test-first | `tests/mcp.py` is written first: for each tool, the result must equal the direct command; then the isolation, limits and malformed-message cases. | Pass |
| V. Single source of truth, minimal surface | One table defines the tools for listing, validating and running. The reference is served from the embedded copy `vo reference` prints. No HTTP, prompts or sampling. | Pass |
| Vendored code | No new third-party code. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/036-vo-mcp/
├── plan.md
├── research.md          # R1–R9
├── data-model.md        # the tool table, a call's result
├── quickstart.md
├── contracts/
│   └── mcp.md           # methods, tools and their arguments, results, errors, limits
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/mcp.c (new), src/main.c, src/vo.h, Makefile
docs/reference.md, site/docs/tools/mcp.md (new), tools/site.py, docs/roadmap.md, README.md, CHANGELOG.md
tests/mcp.py (new), tests/cases/cli/
```

**Structure Decision**: one new C file. The commands are not touched: the server calls `vo_main` in the
child with the argument vector it built.

## Order of work

1. `tests/mcp.py` with the opening exchange, the tool list and one tool; the command, the line reader,
   the JSON reader and the protocol's own methods.
2. The tool table and the call runner; every tool compared with its command.
3. Isolation and limits: output cut, cancellation, end of input, a child ended by a signal; the 1,000
   call session.
4. The resource; malformed messages.
5. Docs; `make test`; three platforms; timing; one host by hand (SC-005); results.

## Complexity Tracking

No constitution violations.
