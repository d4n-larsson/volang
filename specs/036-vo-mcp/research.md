# Research: `vo mcp`

## R1. Transport and framing

**Decision**: the protocol's stdio transport: one JSON-RPC 2.0 message per line on stdin, one per line
on stdout, UTF-8, no embedded newlines. A line longer than 4 MiB is discarded up to its newline and
answered with `-32600` (id `null`). A JSON array (a batch) is `-32600`: batches exist only in one old
revision and no host needs them.

**Rationale**: it is what every host speaks; the server needs no network code.

**Alternatives**: the HTTP transport (out of scope); `Content-Length` framing (that is the language
server protocol's, not MCP's).

## R2. Protocol revisions and methods

**Decision**: the server knows revisions `2025-06-18`, `2025-03-26` and `2024-11-05`. `initialize`
answers with the client's revision when it is one of these, else `2025-06-18` (the protocol's rule: the
server proposes one it supports). The messages used here are the same in all three. Methods:

| Method | Answer |
|---|---|
| `initialize` | `protocolVersion`, `capabilities: {"tools":{},"resources":{}}`, `serverInfo: {"name":"vo","version":"0.1.0"}`, `instructions` (two sentences: read the `vo://reference` resource first; tool results are vo's JSON lines) |
| `notifications/initialized`, other notifications | nothing |
| `ping` | `{}` |
| `tools/list` | the ten tools (no paging) |
| `tools/call` | R3, R6 |
| `resources/list` | the reference |
| `resources/read` | the reference's text; another address is `-32002` |
| `resources/templates/list` | an empty list (some hosts ask regardless of capabilities) |
| `notifications/cancelled` | stops the call with that id if it is running (R3) |
| anything else with an id | `-32601` |

Before `initialize`, only `initialize` and `ping` are answered; anything else is `-32600` ("the
session has not been initialized"). The server does not wait for `notifications/initialized`: hosts
that send requests right after the `initialize` answer are common and harmless.

**Alternatives**: claiming the newest revision without having checked its text against the code was
rejected; adding a revision later is one string.

## R3. A call is the command, in a forked copy

**Decision**: for `tools/call` the server forks. The child makes its group its own, asks to die with
its parent (`sys_die_with_parent`), puts three pipes on descriptors 0, 1 and 2, closes the rest, and
calls `vo_main(argc, argv, env)` with `vo TOOL --json ARG...`; its return value is the exit status. The
parent feeds the call's `stdin` text (if any) and closes the pipe, collects stdout and stderr (R5),
watches its own stdin for a cancellation, and reaps the child. Afterwards it kills the child's group
(whatever the program started), as `std.process` does.

- Exit status: the child's. A child ended by a signal (a crash in the toolchain) gives status 125 and
  the stderr text `vo: the command was ended by signal N`, marked as an error (FR-006, FR-009).
- Cancellation: `notifications/cancelled` whose `requestId` equals the running call's id kills the
  group; the call gets no answer (the protocol's rule). Other lines read while a call runs stay in the
  input buffer and are handled afterwards, in order.
- End of input while a call runs: the call and every complete line already read are finished and
  answered, then the server exits 0. (So `printf '...' | vo mcp` works, which the tests and any script
  rely on.) A host that is gone is noticed when an answer cannot be written: the server kills the
  running group and exits. If the server itself is killed, the child dies with it.
- Time and memory: nothing added. `run` and `test` have the command line's defaults (10 s, 512 MiB);
  the other commands have none on the command line and none here; cancellation covers a slow one.

**Rationale**: the child starts from the server's state right after `vo mcp` was recognized, which is
the state a fresh `vo` has before it reads its arguments, so the result is the command's by
construction. `fork` without `exec` avoids finding the executable's own path and costs under a
millisecond. Every kind of failure (exit, R-codes, out of memory, a crash) is confined to the child.

**Alternatives**: running commands in the server's own process with a recovery jump (the way tests
recover) cannot survive a crash or reset every global, and leaks. `exec` of `/proc/self/exe` works but
is slower and adds a failure mode.

**To verify in step 1**: that nothing `vo_main` relies on is changed by the parent before the fork
(`envp`, `opt_json`, `pos_args`, `opt_cmd` are assigned again by the child's `vo_main`; `app_start`
finds no embedded program). `mcp` is dispatched right after the command name is recognized and refuses
any further argument, so no option state exists.

## R4. One table for the tools

**Decision**: a static table: per tool its name, description, hints, and its arguments; per argument
its name, JSON type (`string`, `integer`, `boolean`, list of strings), whether it is required, a
description, and how it becomes command-line words:

| Kind | Becomes |
|---|---|
| operand | the value (each element, for a list), in table order |
| flag | `--name` when true |
| option | `--name VALUE` (repeated for a list) |
| program arguments (`run` only) | `--` and the elements, last |
| stdin text | fed to the child's input; `impact` and `test` also add `--diff -` |
| choice | an operand restricted to listed words (`query`'s question, `edit`'s edit, `test`'s mode as a flag) |

`tools/list` prints the table as JSON Schema (`type: object`, `properties`, `required`,
`additionalProperties: false`); `tools/call` checks the arguments against it (unknown name, wrong type,
missing required, a word outside a choice: `-32602` naming the argument) and builds the vector. Number
ranges are not in the schema: the command checks them and its T0001 comes back as an error result, so
ranges live in one place.

**Operands that look like options**: an operand that begins with `--` is `-32602` ("write `./--x`"). An
option's value is consumed as a value by the command's parser and program arguments come after `--`, so
no argument can turn into an option the caller did not name.

**Rationale**: one source for the list, the validation and the vector; adding a tool or an option is a
table row.

**Alternatives**: one `args: [str]` list per tool (no schema, the agent must know the command line: the
problem this feature removes); a schema per subcommand of `query` and `edit` as separate tools (more
tools than commands; the spec fixes ten).

## R5. Fixed memory in the server

**Decision**: three buffers allocated once: input (4 MiB + one read), stdout capture (1 MiB) and stderr
capture (1 MiB). Incoming JSON is read in place: a scanner that skips a value and finds a key in an
object returns spans into the input line; strings are unescaped in place when used (a JSON string never
grows when unescaped). Replies are written straight to descriptor 1 through a small buffered writer
that escapes as it goes. Output beyond a capture buffer is read and counted, not kept.

Bytes in captured output that are not valid UTF-8 are written as U+FFFD and counted; control
characters are escaped as JSON requires.

**Rationale**: SC-002 (memory after 1,000 calls) holds trivially; nothing in the parent uses the
collector or the arena after start.

**Alternatives**: the runtime's `json_parse` builds vo values and needs a running collector and a call
node for errors; it would make the server a vo runtime for the sake of a dozen keys.

## R6. The shape of a result

**Decision**: everything is in `content`, as text blocks, so every host shows it to the model:

1. the command's stdout, if not empty;
2. the command's stderr, if not empty;
3. one line: `{"v":1,"kind":"exit","status":N}`, with `"stdout_omitted":B` and/or `"stderr_omitted":B`
   (bytes cut) and `"replaced":C` (invalid bytes replaced) only when not zero.

`isError` is true when the status is 64 (usage) or 125 (internal), else false. No `structuredContent`
and no `outputSchema`: hosts differ in which of the two they pass on, and the output is JSON lines
already.

**Rationale**: stdout arrives unchanged in its own block (FR-005); the exit line uses the schema-1
record style the agent already reads. `kind: "exit"` exists only in tool results, never in a command's
output, so schema 1 is not changed.

## R7. Error codes

**Decision**: no new vo code. `vo mcp X` and `vo mcp --json` are T0001 ("`mcp` takes no arguments: it
serves the Model Context Protocol on standard input and output."). Protocol errors use JSON-RPC's:
`-32700` parse error, `-32600` invalid request, `-32601` method not found, `-32602` invalid parameters,
`-32002` resource not found.

## R8. Documentation

**Decision**: the reference gets one line in the command list (`vo mcp`) and one bullet (≤ 60 tokens).
A site page `tools/mcp.md` (the site's tools section, as `build.md`): the configuration for a host, the
tools table, the result shape, the limits, what is not offered. `tools/site.py` lists it. Roadmap,
README (one line), changelog (Unreleased) and the review table are updated. The usage messages that
list commands gain `mcp`.

## R9. Tests

**Decision**: `tests/mcp.py`, part of `make test`, runs through `bin/vo` (so it works under qemu):

- equivalence: for each tool and each argument, the result's blocks and status equal the direct
  command's stdout, stderr and exit status in the same directory (a temporary copy of a small project);
- validation: unknown tool, unknown argument, wrong type, missing required, `--` operand;
- isolation: timeout, memory limit, `exit(3)`, a runtime error, then a clean `check`; output over 1 MiB
  on each stream; invalid UTF-8; a cancelled call; end of input during a call; a killed child;
- protocol: every case of US4; stdout line discipline;
- `--session N` (not in `make test`): SC-002, N calls with the server's resident size read from
  `/proc`; `--time`: SC-003, paired with direct commands.

A golden case covers the T0001 for arguments. The existing cases that print the command list change by
the word `mcp` (expected changes, re-accepted).
