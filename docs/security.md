# vo Security Model

What vo protects, against whom, and what it promises. Every security review and every spec that adds
an effect, an input format or a command is checked against this document.

## The boundary

**A vo program can do nothing outside its grant.** The grant of `vo run` is the `uses` clause of `main`;
the grant of `vo test` is each test's `uses` clause. A grant names effects and, for most, scopes:

| Effect | What it allows | Scope |
|---|---|---|
| console | write stdout and stderr | — |
| input | read stdin | — |
| file.read, file.write | read or write files | directories, relative to where the program starts |
| net | HTTP requests | host names |
| clock, random | the time, random numbers | — |
| env | environment variables | variable names |
| serve | answer HTTP requests | an address (`"8000"`: this machine; `"0.0.0.0:8000"`: the network) |

The promise holds **whatever the rest of the program does**: every function, anonymous function,
imported file, standard-library function and package runs within the grant, however it was written and
by whom. This is what lets a person run code an agent wrote after reading one line: `main`'s `uses`
clause. `vo.effects` (per function) and `vo.packages` (pinned packages) make the rest reviewable, but
the enforcement does not depend on anyone reading them.

The trusted computing base is the `vo` binary (with its embedded standard library) and the Linux
kernel. Nothing else is trusted: not the program's source, not its packages, not its inputs.

## Attackers

| ID | Attacker | Controls | Wants |
|---|---|---|---|
| A1 | **Hostile program author**: an agent or a person writing vo code that someone else runs; a package author | all vo source except the grant the runner reviewed | to act outside the grant: read secrets, write elsewhere, reach other hosts, run commands, persist; or to hide what the program does |
| A2 | **Remote peer**: an HTTP server the program talks to, a DNS responder, a client of a vo server | bytes on the network | to take over or crash the program or server, read data it should not, or exhaust it |
| A3 | **Hostile data**: files and stdin the program reads within its scopes, JSON, a recording given to `vo replay`, a `vo.packages` manifest or package directory, source given to `vo check`/`fmt`/`doc`/`query`/`edit` | the bytes of one input | to turn input into code execution, effects outside the grant, or writes the command does not document |
| A4 | **Local process** running beside the program as another user or the same user | the file system the program uses (renames, symlinks) | to redirect a scoped file access outside its scope |

Out of scope: an attacker who can change the `vo` binary or the runner's account, the kernel, physical
or side-channel access (timing, cache), the confidentiality and integrity of plain HTTP on the network
(vo has no HTTPS: anyone on the path can read and change requests and responses), and the build
toolchain that produced `vo`.

## Security properties

Each property is a claim the review must try to break; each break is a finding.

- **S1 Static containment.** A program whose code needs an effect, or a scope, outside its grant does
  not check (E0501, E0503) and is not run. No construct (function values, generics with effect
  parameters, anonymous functions, imports, packages, contracts, properties) hides an effect from the
  checker.
- **S2 Run-time containment.** Every effectful operation checks its target against the running grant
  at run time (R0010): file paths after resolving `.`, `..`, absolute paths and symbolic links; host
  names; variable names; serve addresses. This holds inside library functions, anonymous functions and
  packages, which run with their caller's grant, and during replay.
- **S3 No other capabilities.** The only things a program can do are the effects above. The binary
  reaches 24 system calls (`src/platform/*.h`): no process creation, no signals sent, no native code
  loading, no raw sockets, no file descriptors a program can name. No environment variable, file or
  argument outside the grant changes what a program may do.
- **S4 Memory safety.** No program and no input (A1-A3) makes the toolchain read or write memory it
  does not own, or run code other than the program's: overflows, out-of-bounds access, division by zero,
  stack exhaustion and allocation failure are runtime errors with a code, not undefined behavior.
- **S5 Bounded resources.** Time (`--timeout`, default 10 s, per request for servers), live memory
  (`--max-memory`), stack depth, HTTP responses (16 MiB), server requests (head 64 KiB, 100 headers, body
  16 MiB, 10 s to arrive) are limited, and reaching a limit is a reported error. `--timeout 0` turns the
  time limit off on purpose.
- **S6 Commands that do not run programs are safe on hostile source.** `vo check`, `fmt`, `doc`, `query`,
  `explain`, `effects`, `package` and `edit` never execute program code, and write only what they
  document (`fmt` and `edit`: the program's files; `effects --update`: `vo.effects`; `package`:
  `vo.packages`; `--trace`/`--record FILE`: that file).
- **S7 Replay containment.** `vo replay` reads and writes nothing but the recording: no files, network,
  environment, clock or stdin. A hostile recording can only feed the program wrong inputs or be refused
  (T0008); it cannot widen the program's grant, which replay checks as in the recorded run.
- **S8 Package integrity.** A package runs only when its files hash to the pin in `vo.packages` (E0701);
  a package has no grant of its own and cannot be imported past its exports.
- **S9 Server robustness.** A vo server listens only on its declared address, answers one request at a
  time, and keeps serving after malformed, oversized or failing requests; no request makes it crash,
  grow without bound, or run outside its grant.

## Known limits (accepted, documented)

- **Plain HTTP and DNS**: no TLS, no DNSSEC; an A2 attacker on the path can change what the program
  receives. Programs must treat network input as untrusted data, which S4 and S5 protect the toolchain
  from.
- **One request at a time**: a slow client can hold a vo server for up to 10 s per connection (a
  denial of service that the request limits bound but do not prevent).
- **Console output is not filtered**: a program (or data it prints) can send terminal escape sequences
  to the runner's terminal.
- **Recordings contain the inputs**: file contents, variables and HTTP traffic a program read are in its
  recording; recordings are as sensitive as those inputs.
- **The grant is only as good as its review**: a broad grant (`file.read("./")`, an unscoped `net`) is
  honored as written.

## Open questions for the first review

Recorded here so the review starts from what is already known, not as accepted limits:

- **Check-then-open (A4)**: `rt_check_path` resolves the path (following links) and the file is then
  opened by its original path, which follows links again. A local process that swaps a symbolic link
  between the two steps may redirect the access outside the scope. A vo program cannot create links
  itself (no such system call is reachable), so this needs A4.
- **New C parsers** reachable by A2/A3: the server's request framing (`src/serve.c`), the recording
  reader (`src/replay.c`), the manifest reader (`src/packages.c`), DNS answers (`src/net.c`).

## Review method

Each area is reviewed against the properties above, by reading the code and by trying to break it with
a proof of concept (a vo program, an input file, a request). A finding records the property it breaks,
the attacker, a reproduction, the impact and a severity:

| Severity | Meaning |
|---|---|
| Critical | breaks S1-S4 or S7 for A1-A3: effects outside the grant, or code execution |
| High | breaks S4/S6/S8/S9 with a narrower precondition (A4, unusual flags), or a crash of a server |
| Medium | a resource limit that can be exceeded (S5), a documented write that goes elsewhere, a misleading grant |
| Low | hardening: a defense-in-depth gap with no known exploit |

Fixes are specified (Spec Kit) with each finding as a requirement and its reproduction as the first
failing golden case.
