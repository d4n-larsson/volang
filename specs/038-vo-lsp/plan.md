# Implementation Plan: The Toolchain in the Editor (`vo lsp`)

**Branch**: `038-vo-lsp` (work on `main`, as for specs 001–037) | **Date**: 2026-11-21 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/038-vo-lsp/spec.md`

## Summary

A new command, `vo lsp`, in a new file `src/lsp.c`, built like `vo mcp`: the server process only reads
messages and keeps the open documents; every answer is computed in a forked copy (R2). The copy registers
the open documents' texts as already-loaded files, so the loader never reads them from disk (R3), runs
the ordinary command (`check`, `query type-at`, `query refs`, `edit rename --dry-run`, `fmt`) with its
output captured in memory instead of printed (R4), and translates that schema-1 output into the
protocol's answer, converting positions with the file texts it has at hand (R5, R6). So an answer is the
command's answer by construction. What `vo mcp` and `vo lsp` share (reading JSON in place, writing JSON,
running a copy with a deadline) moves out of `src/mcp.c` into `src/jsonrpc.c` (R9).

| Part | File | What |
|---|---|---|
| Command | `src/main.c`, `src/vo.h` | `lsp` in the command table, dispatched like `mcp` |
| Shared | `src/jsonrpc.c` (new, from `src/mcp.c`) | in-place JSON reader, JSON writer, UTF-8 check, "run this in a copy" with output collection, deadline and cancellation |
| Server | `src/lsp.c` (new) | framing, lifecycle, open documents, scheduling of checks, one function per request run in the copy |
| Capture | `src/sys.c`, `src/main.c` | output of a command into memory; `write_file_safely` into memory (so nothing is ever written) |
| Editors | `editors/vscode/` (client code), `editors/zed/`, `site/docs/tools/editors.md` | start the server (R10) |
| Docs | `docs/reference.md`, `site/docs/tools/lsp.md` (new), `tools/site.py`, roadmap, README, changelog, review table | R11 |
| Tests | `tests/lsp.py` (new, in `make test`), `tests/cases/cli/` | R12 |

## Technical Context

**Language/Version**: C (C11, freestanding, no libc); a few lines of JavaScript for the VS Code client.

**Primary Dependencies**: none in the toolchain. The VS Code folder gains `vscode-languageclient`
(installed by `npm install` in that folder; not part of building or testing vo).

**Storage**: none.

**Testing**: a scripted client (`tests/lsp.py`) compares every answer with the command's; `make quick
AREA=lsp` between edits, `make test` once per change; the gate only when asked. This spec touches
`src/mcp.c` and `src/sys.c`: `tests/mcp.py` and, because of spec 036's lesson, the collector stress run
of two cases (`tests/run.sh --stress` on `run/oom_small` and `map_keys/many`) are run before finishing.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler toolchain (CLI).

**Performance Goals**: diagnostics within 200 ms of a change in a 2,000-line file of a 20-file program,
hover within 100 ms (SC-003): a fork plus one check (milliseconds) plus the pause before checking.

**Constraints**: no static buffer over 64 KiB (the collector scans static data in every vo process);
document texts and message buffers are mapped and unmapped; the server never runs a program and never
writes a file (FR-003, enforced by the write capture); reference ≤ 15,000 tokens (+≤ 50; 14,803 now).

**Scale/Scope**: about 900 lines in `src/lsp.c`, 350 moved to `src/jsonrpc.c` (with `src/mcp.c`
shrinking by as much), 600 of Python tests.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | The agent's interface is unchanged; the people who read its code get the same answers in their editor. No language change. | Pass |
| II. Machine-readable feedback | Diagnostics keep code, message and fixes; a quick fix is the toolchain's fix, edit for edit. | Pass |
| III. Determinism | An answer depends only on the open texts and the files on disk: each is computed in a fresh copy. | Pass |
| IV. Test-first | `tests/lsp.py` first: per feature, the answer must equal the command's; then unsaved text, robustness, memory. | Pass |
| V. Single source of truth, minimal surface | One checker: the copy runs the commands themselves. No completion or other feature without toolchain data behind it. The shared protocol code exists once. | Pass |
| Vendored code | None in the toolchain. The VS Code client library is the editor's own, fetched by npm into the extension folder, never built into vo. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/038-vo-lsp/
├── plan.md
├── research.md          # R1–R12
├── data-model.md        # open document, request, the copy's result
├── quickstart.md
├── contracts/
│   └── lsp.md           # methods, capabilities, mappings, limits
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/lsp.c (new), src/jsonrpc.c (new), src/mcp.c, src/sys.c, src/main.c, src/vo.h, Makefile
editors/vscode/ (extension.js, package.json), editors/zed/extension.toml
docs/reference.md, site/docs/tools/lsp.md (new), site/docs/tools/editors.md, tools/site.py
tests/lsp.py (new), tests/cases/cli/
```

**Structure Decision**: one file for the server, one for what two servers share. The commands are not
changed beyond the two capture points.

## Order of work

1. Move the shared code to `src/jsonrpc.c`; `tests/mcp.py` still passes.
2. `tests/lsp.py` with lifecycle and diagnostics; framing, documents, the copy with overlays and
   capture; diagnostics and quick fixes.
3. Hover, definition, references; rename and format; document symbols.
4. Scheduling (pause, newest wins, dependents), limits, cancellation, robustness, memory.
5. Editors, docs; stress cases; `make test`; three platforms; timing; results.

## Complexity Tracking

No constitution violations.
