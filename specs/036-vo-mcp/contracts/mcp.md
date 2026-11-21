# Contract: `vo mcp`

```sh
vo mcp        # no options, no arguments; anything else is T0001 (exit 64)
```

Serves the Model Context Protocol on stdin/stdout, one JSON-RPC 2.0 message per line, until end of
input; exit status 0. Paths are relative to the directory it was started in.

## Methods

| Method | Result |
|---|---|
| `initialize` | `{"protocolVersion": P, "capabilities": {"tools": {}, "resources": {}}, "serverInfo": {"name": "vo", "version": "0.1.0"}, "instructions": "..."}`; `P` is the client's revision if it is `2025-06-18`, `2025-03-26` or `2024-11-05`, else `2025-06-18` |
| `ping` | `{}` |
| `tools/list` | `{"tools": [...]}` |
| `tools/call` | see Results |
| `resources/list` | `{"resources": [{"uri": "vo://reference", "name": "reference", "title": "vo language reference", "description": "The whole language and toolchain in one document; read it before writing vo.", "mimeType": "text/markdown"}]}` |
| `resources/read` `{"uri": "vo://reference"}` | `{"contents": [{"uri": "vo://reference", "mimeType": "text/markdown", "text": T}]}`, `T` = the output of `vo reference` |
| `resources/templates/list` | `{"resourceTemplates": []}` |
| `notifications/cancelled` `{"requestId": ID}` | stops the running call with that id; that call gets no answer |
| other notifications | ignored |

## Errors

| Code | When |
|---|---|
| `-32700` | the line is not JSON (id `null`) |
| `-32600` | not a request object, a batch, a line over 4 MiB, or a request before `initialize` |
| `-32601` | unknown method (requests only) |
| `-32602` | unknown tool; an argument that is unknown, missing, of the wrong type, outside its choices, or an operand beginning with `--`; the message names it |
| `-32002` | `resources/read` of an unknown address |

## Tools

Every tool runs `vo TOOL --json ...`. `R` = required. Hints: *reads* (`readOnlyHint: true,
openWorldHint: false`), *writes* (`readOnlyHint: false, destructiveHint: true, openWorldHint: false`),
*runs* (`readOnlyHint: false, destructiveHint: true, openWorldHint: true`: the user's program runs with
the effects it declares).

| Tool | Arguments | Command words | Hint |
|---|---|---|---|
| `check` | `paths` [str] R | `PATH...` | reads |
| `run` | `file` str R; `args` [str]; `stdin` str; `timeout` int; `seed` int; `clock_ms` int; `max_memory` int; `mem_stats` bool; `workers` int; `ca_files` [str] | `[options] FILE [-- ARG...]` | runs |
| `test` | `paths` [str] R; `filter` str; `tests` [str]; `specs` [str]; `diff` str (the diff's text); `mode` `list`\|`coverage`\|`mutate`; `max_mutants` int; `timeout`, `seed`, `clock_ms`, `max_memory`, `mem_stats`, `ca_files` as `run` | `[options] PATH...` | runs |
| `fmt` | `paths` [str] R; `check` bool | `[--check] PATH...` | writes |
| `effects` | `file` str R; `update` bool | `[--update] FILE` | writes |
| `explain` | `code` str R | `CODE` | reads |
| `doc` | `entry` str; `name` str | `[ENTRY] [NAME]` | reads |
| `query` | `question` `callers`\|`refs`\|`type-at`\|`specs` R; `entry` str R; `target` str | `QUESTION ENTRY [TARGET]` | reads |
| `impact` | `entry` str R; `diff` str R (the diff's text) | `ENTRY --diff -` | reads |
| `edit` | `edit` `rename`\|`add-field` R; `entry` str R; `target` str R; `change` str R (the new name, or the field); `dry_run` bool | `[--dry-run] EDIT ENTRY TARGET CHANGE` | writes |

Not offered: `--trace`, `--trace-limit`, `--record`; the commands `build`, `package`, `replay`,
`version`.

## Results

```json
{"content": [{"type": "text", "text": "<stdout>"},
             {"type": "text", "text": "<stderr>"},
             {"type": "text", "text": "{\"v\":1,\"kind\":\"exit\",\"status\":65}"}],
 "isError": false}
```

- The stdout and stderr blocks are present only when not empty, and are the command's bytes.
- The last block is always the exit line. It also has `"stdout_omitted": B` / `"stderr_omitted": B`
  when a stream exceeded 1 MiB (B bytes were dropped from its end) and `"replaced": C` when C bytes
  that were not valid UTF-8 were written as U+FFFD.
- `isError` is true for status 64 (usage, T0001) and 125 (internal error, or the command was ended by a
  signal), false otherwise.

## Limits

| What | Limit |
|---|---|
| an incoming message | 4 MiB |
| stdout, stderr of a call | 1 MiB each |
| calls at a time | 1; others wait in order |
| time, memory | the command's (`run`/`test`: 10 s and 512 MiB by default) |
