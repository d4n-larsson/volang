# vo mcp

`vo mcp` makes the toolchain a [Model Context Protocol](https://modelcontextprotocol.io) server, so an agent host finds the vo commands and the language reference by itself, with no prompt about vo.

## Synopsis

```text
vo mcp
```

It takes no options. It reads one JSON-RPC message per line on standard input, writes one per line on standard output, and exits with status 0 when its input ends. Start it in the project's directory: paths in tool calls are relative to it, as on the command line.

## Adding it to a host

A host needs the command and its working directory. For Claude Code, in the project's directory:

```text
claude mcp add vo -- vo mcp
```

For hosts configured with a JSON file:

```text
{"mcpServers": {"vo": {"command": "vo", "args": ["mcp"]}}}
```

## Tools

Each tool is the command of the same name with `--json`, and its arguments are the command's operands and options by name.

| Tool | Arguments | Does |
|---|---|---|
| `check` | `paths` | [vo check](check.md): diagnostics with fixes |
| `run` | `file`, `args`, `stdin`, `timeout`, `seed`, `clock_ms`, `max_memory`, `mem_stats`, `workers`, `ca_files` | [vo run](run.md): checks, then runs `main` with the effects it declares |
| `test` | `paths`, `filter`, `tests`, `specs`, `diff`, `mode` (`list`, `coverage`, `mutate`), `max_mutants`, and `run`'s limits | [vo test](test.md) |
| `fmt` | `paths`, `check` | [vo fmt](fmt.md): rewrites files unless `check` is true |
| `effects` | `file`, `update` | [vo effects](effects.md) |
| `explain` | `code` | [vo explain](explain.md) |
| `doc` | `entry`, `name` | [vo doc](doc.md) |
| `query` | `question` (`callers`, `refs`, `type-at`, `specs`), `entry`, `target` | [vo query](query.md) |
| `impact` | `entry`, `diff` | [vo impact](impact.md) |
| `edit` | `edit` (`rename`, `add-field`), `entry`, `target`, `change`, `dry_run` | [vo edit](edit.md) |

`diff` is the text of a unified diff, not a file name. `stdin` is the program's standard input; without it the program reads an empty input. Every tool is marked for the host as reading only (`check`, `explain`, `doc`, `query`, `impact`), changing files (`fmt`, `effects`, `edit`) or running the user's program (`run`, `test`).

## Results

A result is up to three text blocks: what the command wrote to standard output, what it wrote to standard error, and one line with its exit status. Empty outputs have no block.

```text
{"v":1,"kind":"diagnostic","severity":"error","code":"E0301", ...}
{"v":1,"kind":"summary","command":"check","errors":1, ...}
```

```text
{"v":1,"kind":"exit","status":65}
```

The first block is exactly what `vo check --json` prints ([JSON output](json-output.md)); the statuses are the command line's ([exit statuses](exit-statuses.md)). A failing test, a check error or a runtime error is a normal result. A result is marked as an error only for a usage error (status 64) or an internal error (125).

Arguments are checked before anything runs: an unknown tool or argument, a missing one, a wrong type, or a path that begins with `--` is the protocol's error `-32602`, naming the argument.

## The reference

The server has one resource, `vo://reference`: the language reference that [vo reference](reference.md) prints. An agent should read it before writing vo.

## Limits and isolation

Every call runs as its own process, so a program that loops, runs out of memory, exits or fails cannot affect the server or the next call.

| What | Limit |
|---|---|
| Time and memory | the command's: 10 seconds and 512 MiB for `run` and each test, unless the call gives `timeout` or `max_memory` |
| Output | the first 1 MiB of standard output and of standard error; the exit line then has `stdout_omitted` or `stderr_omitted` with the bytes left out |
| An incoming message | 4 MiB |
| Calls at a time | one; others wait and are answered in order |

Output that is not UTF-8 has each bad byte replaced by U+FFFD, and the exit line has `replaced` with their number. A host can cancel a running call (`notifications/cancelled`). When the host goes away, whatever is running is stopped.

The server gives a program nothing the command line would not: `run` and `test` execute the user's program with the effects `main` declares, in the server's directory. It is not a sandbox.

## Not offered

`build`, `package`, `replay` and `version` are not tools, and `run` and `test` have no `--trace` or `--record`: use the command line for those. There is no HTTP transport, and no prompts.
