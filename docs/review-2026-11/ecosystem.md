# Ecosystem: library, tooling, platforms, distribution

A language competes on what surrounds it as much as on what is in it. This file lists what is missing
around vo, most blocking first in each section.

## Project basics

These block adoption before anyone reads a line of vo.

- **No license.** The repository has `LICENSE` files only under `third_party/`. Without one nobody may
  use, copy or contribute to vo. Pick one (MIT or Apache-2.0 are what language toolchains use) and check
  it against Mbed TLS (Apache-2.0 or GPL-2.0-or-later), CORE-MATH (MIT) and SQLite (public domain).
- **The suite does not run in CI.** `.github/workflows/` holds only `pages.yml`. `make gate`,
  `make instrumented` and the cross builds run on one machine. Add a workflow for `make lint test` on
  x86-64, the cross builds under qemu, and the sanitizer build. The suite is the project's best asset and
  it is invisible to a visitor.
- **No releases.** There are no tags and no binaries; installing means cloning and running `make`. Since
  the output is one static binary per architecture, a release is three files and a checksum list. Add an
  install line to the site (`curl ... | sh` plus the manual steps), and `vo version` already reports what
  is needed for bug reports.
- **No stability statement.** Say what "0.1.0, schema 1" promises: which of the language, the JSON
  schema, error codes and `vo.effects` format can change, and how a program is migrated when they do.
  The v1→v2 migration test shows this has been done once; write the policy down.
- **No contribution guide or changelog.** The specs are an unusually good record; a short `CONTRIBUTING.md`
  pointing at the constitution and the spec flow is enough.

## Standard library

Ordered by how many programs each gap blocks.

| Gap | Today | Notes |
|---|---|---|
| HTTP client options | `get(url)` and `post(url, content_type, body)` only | no request headers (so no `Authorization`), no `PUT`/`PATCH`/`DELETE`, no timeout per request, redirects returned and not followed, one request per connection. One `request(Request) -> result[Response]` with a record covers all of it. **Most urgent item in this file.** |
| Hashing and secrets | `src/sha256.c` and Mbed TLS are in the binary; nothing is exported | `std.crypto`: SHA-256, HMAC, constant-time compare, a password hash, and random bytes from the system under their own effect. `random_int` is a seeded generator, and `std/web.vo:749` builds request IDs from it; nothing in vo can make a session token safely today. |
| TCP and UDP | HTTP only | on the roadmap. Unlocks Postgres, Redis, SMTP and message queues, each as a package. |
| Serving HTTPS, keep-alive, IPv6 | plain HTTP, `Connection: close`, IPv4 | on the roadmap in part. A proxy in front works, so this is after the client. |
| Streaming | whole files, bodies and outputs | see [language.md](language.md#9-streaming) |
| Time zones | `std.time` is UTC only | embed the tz database or read the system's under `file.read`; local time is needed by anything that prints a date for a person |
| Data formats | JSON only | CSV first (agents are asked for it constantly and quoting rules are easy to get wrong), then TOML or YAML for configuration, then gzip since HTTP uses it |
| Command-line arguments | `args()` gives a list | a small `std.cli` that turns a record with annotations into a parser and a help text, the way `std.web` does for requests |
| Logging | `print`/`eprint` | structured lines with levels and fields, to stderr, with the clock effect only when timestamps are on |
| Paths | string concatenation | `std.path`: join, parent, name, extension, clean. Cheap and prevents `a//b` and `..` mistakes before the run-time check does |
| Text | ASCII case mapping, byte lengths | see [language.md](language.md#4-strings) |
| Templates | none | `std.web` returns JSON; an HTML template with escaping by default would make it a full web stack |
| Postgres | the `postgres` package (spec 049) | done as a package in vo on `std.tcp`, with `std.sqlite`'s names so a program moves between them; `sql("...")` is checked when it runs, not by `vo check` |
| Process control | run to completion, no input stream | interactive children and signals, after streaming |

`std.math` gaps (`log2`, `log10`, `hypot`, hyperbolic functions) are noted in the roadmap as "when a
program needs it"; that is fine.

## Distribution

- **`vo build main.vo -o app`.** Today a vo program is run as `vo run main.vo` from its source tree,
  so shipping one means shipping the toolchain, the sources, `vo.effects` and `vo.packages`. The binary
  already embeds the standard library; let it embed a checked program too and write a copy of itself
  with the program attached. The result is one static executable for each of the three architectures.
  - The grant is baked in and can be printed: `./app --vo-effects` shows what `main` declares. That is a
    feature no other language's binaries have: an executable that states what it can touch, enforced.
  - Flags like `--seed`, `--record` and `--max-memory` stay available behind a `--vo-` prefix.
- **Downloading packages** (on the roadmap). The content-hash design is right. What it needs is a fetch
  step: `vo package add NAME URL` downloads, hashes and pins, and nothing is fetched at check or run
  time. A registry can wait; a git URL and a hash are enough to start an ecosystem. `vo.effects` showing
  a dependency's capabilities is the differentiator, so show it in the `add` output.
- **Containers.** A static binary with no libc fits a `FROM scratch` image. One documented Dockerfile on
  the site covers it.

## Platforms

- **macOS** (on the roadmap). Most developers who would try vo are on it. The raw-syscall design makes
  it a real port; `src/platform/hosted.c` for the sanitizer build is the starting point, since macOS
  requires going through libSystem anyway.
- **WebAssembly.** A `wasm32` build of the checker and VM (no network, files in memory) gives a
  playground on the documentation site: type vo, see diagnostics with fixes, run it. Every language that
  grew in the last decade had one. It is also the cheapest answer to "Windows": a browser.
- **Windows.** WSL works today since the binary is a static Linux executable. Say so on the site and
  defer a native port.

## Tooling

- **Language server** (on the roadmap). `vo check --json`, `vo query` and `vo edit rename` already
  produce diagnostics, fixes as code actions, hover, references and rename. The server is mostly a
  protocol layer. It needs the checker to be incremental or fast: see
  [optimizations](optimizations.md#3-checking-is-quadratic-in-the-number-of-top-level-items).
- **Editor grammar.** The site has a syntax highlighter; there is no TextMate or tree-sitter grammar in
  the repository. GitHub, VS Code, Zed, Neovim and Helix each need one of those two.
- **An MCP server for agents.** vo's audience is agents, and its interface is a CLI with `--json`.
  Wrapping `check`, `test`, `doc`, `query`, `impact`, `edit` and `explain` as tools, with `vo reference`
  as a resource, makes vo usable from any agent with no prompt engineering. Ship it in the binary
  (`vo mcp`). Together with a published agent skill this is the on-brand equivalent of an IDE plugin.
- **Profiler.** The VM counts steps already. `vo run --profile` with steps, calls and allocated bytes per
  function is nearly free, deterministic (so a profile is diffable, which no sampling profiler offers),
  and would have pointed at every finding in `optimizations.md`.
- **Debugger** (on the roadmap). Replay makes a stepper that also goes backwards possible. For agents, a
  non-interactive form is more useful: `vo replay REC --break main.vo:42 --print x,y` prints the values
  at each hit as JSON lines.
- **More edits.** Spec 006 lists them: extract function, move an item, change a signature. Each one is
  a refactoring an agent otherwise does by text and gets wrong; the 12-of-41 broken text renames in the
  spec 006 evaluation is the argument.
- **`vo fix`.** Applying the first fix of every diagnostic is something every agent loop
  reimplements (`tests/` has a fix applier). Make it a command, with `--dry-run`.
- **Documentation generation.** `vo doc` prints to the terminal; `vo doc --html DIR` for a program or
  package gives packages a home page for free.
- **REPL or `vo eval`.** Low priority for agents, expected by people. `vo eval 'len(split("a,b", ","))'`
  wrapped in a `main` is enough.

## Evidence

- **Finish benchmark v2.** The roadmap's own first row. vo's claim is that agents do better in it, and
  that is unmeasured: v1 tied at the ceiling and the first v2 run was invalid. Every priority in this
  review should be re-read against that result. If v2 also ties, the roadmap's fallback (long,
  unsupervised, multi-step tasks) is where effects, `vo.effects` review and `vo edit` should show, and
  the `voledger` experiment is the first instance of it.
- **Compare with more than Python.** TypeScript and Go are what agents are most often asked to write.
  A capability task graded on declared effects against Go graded by observation is the comparison a
  reader will ask for.
- **Publish speed and memory tables** against CPython, Node and Go, generated by a script in the
  repository, on the site. The numbers in `optimizations.md` are good enough to show once the cliffs are
  fixed.
- **A second security review** after TCP, packages from the network and `vo build` land, since each
  widens the boundary the first review covered.

## A possible order

1. License, CI, releases. HTTP client options. The four cliffs in `optimizations.md`.
2. Benchmark v2. Typed errors, constants, keys. `std.crypto`. `vo build`.
3. `vo mcp`, editor grammar, language server. TCP. Streaming. Package download.
4. macOS, the WebAssembly playground. Workers. Decide on interfaces from what the experiments show.
