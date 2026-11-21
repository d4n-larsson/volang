# Changelog

What changed in each released version of vo, for people and agents who use it. The record of every
decision is in [`specs/`](specs/) and summarized in [`docs/roadmap.md`](docs/roadmap.md); what a version
may and may not change is in [`docs/stability.md`](docs/stability.md).

## Unreleased

- `vo mcp`: the toolchain as a Model Context Protocol server for agent hosts (ten commands as tools, the
  reference as the resource `vo://reference`).
- `vo lsp`: the toolchain as a language server for editors (diagnostics on unsaved text, quick fixes, hover,
  definition, references, rename, formatting, outline).
- `std.tcp`: TCP connections (`connect`, `connect_tls`, `start_tls`, `send`, `read`, `read_exact`,
  `read_until`, `listen`, `accept`) under the `net` and `serve` effects, with typed errors and record and
  replay.
- `net` scopes can name a port: `net("db.internal:5432")`. A bare host still means any port. `std.http`
  obeys ported scopes too. `serve` scopes accept port `0` (a free port, for `std.tcp.listen`).
- Editor grammars in `editors/`: VS Code (TextMate), Neovim, Helix and Zed (tree-sitter), Vim.
- Fixed: `vo impact --diff -` and `vo test --diff -` read a pipe (`git diff | vo impact main.vo --diff -`);
  before, standard input had to be a regular file.

## 0.1.0

The first release: the toolchain as one static binary for Linux on x86-64, ARM64 and RISC-V 64.

**Language**

- Statically typed, with records, variants and exhaustive `match`, generics with `ordered`, `key`, `json`
  and effect parameters, functions as values, constants, and no implicit conversions.
- Checked effects with scopes (`uses file.read("data")`), an effects record (`vo.effects`) that makes
  every change in what a program may touch a reviewed change, and enforcement at run time.
- `result[T]` and typed errors `result[T, E]` with `try` and `map_err`; no exceptions.
- Lists, maps keyed by any value type without floats or collections, ordered maps and sets, priority
  queues, deques, `bytes`.
- Typed holes, property tests, contracts, `@spec` annotations.

**Toolchain**

- `vo check` with stable error codes and mechanical fixes, `vo run`, `vo test` (selection, coverage,
  mutation), `vo fmt`, `vo effects`, `vo doc`, `vo explain`, `vo reference`, `vo package`.
- `vo query`, `vo impact` and `vo edit` for questions and changes by structure, not by text.
- Deterministic runs: `--seed`, `--clock-ms`, `--record` and `vo replay`, `--trace`, time and memory
  limits.
- `vo build`: a program as one executable that prints what it may touch (`--vo-effects`).
- JSON output for every command (`--json`, schema 1).

**Standard library**

- `std.json`, `std.text`, `std.time`, `std.random`, `std.math`, `std.bytes`, `std.collections`,
  `std.option`, `std.regex`, `std.fs`, `std.process`, `std.http` (client with HTTPS, server with worker
  processes), `std.sqlite`, `std.web`, `std.crypto`.
