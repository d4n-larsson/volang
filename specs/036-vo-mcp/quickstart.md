# Quickstart: validating `vo mcp`

Prerequisite: `make` (and `make ARCH=aarch64`, `make ARCH=riscv64` for the three-platform run).

## By hand

```sh
cd examples/expenses
printf '%s\n' \
  '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"t","version":"0"}}}' \
  '{"jsonrpc":"2.0","method":"notifications/initialized"}' \
  '{"jsonrpc":"2.0","id":2,"method":"tools/list"}' \
  '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"check","arguments":{"paths":["main.vo"]}}}' \
  '{"jsonrpc":"2.0","id":4,"method":"resources/read","params":{"uri":"vo://reference"}}' \
  | ../../build/vo mcp
```

Expected: four lines, ids 1 to 4; id 3's blocks equal `vo check --json main.vo` plus the exit line; id
4's text equals `vo reference`. Exit status 0.

## The checks

```sh
python3 tests/mcp.py              # equivalence, validation, isolation, protocol (in make test)
python3 tests/mcp.py --session 1000   # SC-002: memory after 1,000 mixed calls
python3 tests/mcp.py --time           # SC-003: a call against the direct command
VO_BIN=build-aarch64/vo python3 tests/mcp.py
VO_BIN=build-riscv64/vo python3 tests/mcp.py
```

## A host (SC-005, once, by hand)

In a directory with a vo program, register the server (for Claude Code: `claude mcp add vo -- vo mcp`),
and ask the agent, with no mention of how vo works, to check the program. It should list the tools,
read `vo://reference` and call `check`. See the [contract](contracts/mcp.md) for the tools and results.
