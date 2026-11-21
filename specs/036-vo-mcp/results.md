# Results: `vo mcp` (spec 036)

Measured 2026-11-21 on x86-64 (ARM64 and RISC-V under qemu). **The gate was not run**; `make test` was
run once (see the end).

## Success criteria

| | Criterion | Result |
|---|---|---|
| SC-001 | every tool equals its command | **Pass**: 42 calls over the ten tools in `tests/mcp.py` (each argument at least once), compared with the direct command in an identical copy of the project: stdout, stderr and exit status equal; the files left by `fmt`, `effects` and `edit` equal. Only a test record's measured `ms` is masked |
| SC-002 | 1,000 mixed calls, memory does not grow | **Pass**: `tests/mcp.py --session 1000`: all answered (check errors, runtime errors, memory limit, 3 MiB floods, a timeout every 100th); the server's resident size is 1,632 KiB after 10 calls and 1,632 KiB after 1,000 |
| SC-003 | ≤ 5 ms over the direct command | **Pass**: `tests/mcp.py --time`, paired medians: `check` 0.82 ms against 0.91 ms direct, `run` 0.69 against 0.81, `explain` 0.60 against 0.73. A call is slightly faster than starting the command (a fork instead of an exec) |
| SC-004 | malformed messages | **Pass**: 19 cases, each followed by an answered ping; plus 300 damaged lines (`--fuzz`, in `make test`), every answer valid JSON, exit 0 |
| SC-005 | a real host, by hand | **Not done**: no host was driven from this session. Steps: quickstart.md, "A host" |
| SC-006 | reference under 15,000 tokens; existing cases unchanged | **Pass**: about 14,803 tokens (+48). Changed cases: the three that print the list of commands (`cli/unknown_command`, `cli/unknown_command_json`, `build/damaged`), by the word `mcp` |

## Three platforms (FR-017)

`tests/mcp.py` passes with `build/vo`, `build-aarch64/vo` and `build-riscv64/vo` (7 groups, 0 failures
each).

## What changed against the plan

- **`--diff -` did not read a pipe.** `vo impact --diff -` and `vo test --diff -` opened `/dev/stdin` and
  required a regular file, so `git diff | vo impact main.vo --diff -` failed with T0002. The `diff`
  argument of the tools needs a pipe, so `src/query.c` now reads standard input to its end. This is a fix
  to the command line as well.
- **Hints**: every tool states all of `readOnlyHint`, `destructiveHint` (where it applies) and
  `openWorldHint`, since the protocol's defaults are the pessimistic ones. `run` and `test` are marked
  destructive: the user's program may write files.
- **A host that is gone** is noticed while a call runs (the output's other end is closed), not only at
  the next answer, so a program without a time limit is stopped at once.
- **Words per call**: at most 256 command words; more is `-32602`.
- **Tests and code were written together**, not test-first task by task: the client (`tests/mcp.py`) was
  first run against a server that already existed.

## Known limits

- When only one of stdout and stderr is not empty, a result has one output block and nothing says which
  stream it was (`run` reports on stderr, the other commands on stdout).
- A 1 MiB cut can fall inside a UTF-8 character; that character becomes U+FFFD and is counted in
  `replaced`.
- `edit` names `vo.effects` by its full path in its records (as the command does).
- A method or key written with JSON escapes (`"ping"` as `"ping"`) is not recognized.

## Size

`src/mcp.c`: about 1,000 lines as formatted (the plan said 550).
`tests/mcp.py`: about 690 lines.

## `make test`

Passed once after the last code change: 40 steps in 7m02s, including `tests/mcp.py`. The docs written after it started (results, tasks, contract) are not code.
