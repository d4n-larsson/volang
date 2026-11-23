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
| file.read, file.write | read or write files; list directories, look at paths, append, create directories (`std.fs`) | directories, relative to where the program starts |
| file.delete | delete files, links and empty directories; rename (it removes the old name) | directories, like `file.write` |
| net | HTTP requests | host names |
| clock, random | the time and the machine's time zone setting (`std.time.local_zone` reads the `TZ` variable and `/etc/localtime`, and nothing else of the environment or the file system); random numbers | — |
| env | environment variables | variable names |
| serve | answer HTTP requests | an address (`"8000"`: this machine; `"0.0.0.0:8000"`: the network) |
| process | start a named program and wait for it (`std.process`) | programs: a name found through `PATH`, or a path; required (E0512) |

The promise holds **whatever the rest of the program does**: every function, anonymous function,
imported file, standard-library function and package runs within the grant, however it was written and
by whom. This is what lets a person run code an agent wrote after reading one line: `main`'s `uses`
clause. `vo.effects` (per function) and `vo.packages` (pinned packages) make the rest reviewable, but
the enforcement does not depend on anyone reading them.

The trusted computing base is the `vo` binary (with its embedded standard library and, since spec
019, the vendored TLS library Mbed TLS, since spec 020 the vendored math library CORE-MATH, and since
spec 026 the vendored database engine SQLite, all under `third_party/`, pinned and updated under
constitution 1.5.0: Mbed TLS and SQLite to a release, CORE-MATH, which publishes none, to a commit, each
with the hash of every kept file, checked by `make lint`) and the Linux kernel. Nothing else is trusted: not the program's source, not its packages, not its inputs.

## Attackers

| ID | Attacker | Controls | Wants |
|---|---|---|---|
| A1 | **Hostile program author**: an agent or a person writing vo code that someone else runs; a package author | all vo source except the grant the runner reviewed | to act outside the grant: read secrets, write elsewhere, reach other hosts, run commands, persist; or to hide what the program does |
| A2 | **Remote peer**: an HTTP server the program talks to, a DNS responder, a client of a vo server | bytes on the network | to take over or crash the program or server, read data it should not, or exhaust it |
| A3 | **Hostile data**: files and stdin the program reads within its scopes, JSON, a recording given to `vo replay`, a `vo.packages` manifest or package directory, source given to `vo check`/`fmt`/`doc`/`query`/`edit` | the bytes of one input | to turn input into code execution, effects outside the grant, or writes the command does not document |
| A4 | **Local process** running beside the program as another user or the same user | the file system the program uses (renames, symlinks) | to redirect a scoped file access outside its scope |

Out of scope: an attacker who can change the `vo` binary or the runner's account, the kernel, physical
or side-channel access (timing, cache), the confidentiality and integrity of plain `http://` on the
network (anyone on the path can read and change it; `https://` is verified, see the known limits), and
the build
toolchain that produced `vo`.

## Security properties

Each property is a claim the review must try to break; each break is a finding.

- **S1 Static containment.** A program whose code needs an effect, or a scope, outside its grant does
  not check (E0501, E0503) and is not run. No construct (function values, generics with effect
  parameters, anonymous functions, imports, packages, contracts, properties) hides an effect from the
  checker. A function that declares a scoped effect without scopes (one that is given its target) may
  be called under a scoped declaration: the checker then knows the effect and not the target, which S2
  checks (spec 050).
- **S2 Run-time containment.** Every effectful operation checks its target at run time (R0010) against
  every declaration on the call path that has scopes for the effect, from the running function to the
  root grant (`main`'s, or the test's or property's clause): file paths after
  resolving `.`, `..`, absolute paths and symbolic links; host names; variable names; serve addresses.
  This holds inside library functions, anonymous functions and packages, which run with their caller's
  grant, and during replay (lexically: replay never looks at the file system). A scoped file is opened
  from its scope directory (itself opened from the directory the program started in) one path
  component at a time without following links, so a link swapped in after the check fails the open
  instead of escaping; links inside a scope that stay
  inside it work, absolute ones or ones leaving it do not (spec 013). Paths with a NUL byte are refused.
  Spec 017's directory operations (listing, information, append, create, delete, rename) reach their
  target the same way and then act through the `*at` system call on the parent directory, so a swapped
  link cannot redirect them either; deleting and renaming act on a link itself, never on its target,
  and never remove a scope directory itself. A listing reports a link's target kind only when the
  target is inside the grant. A program is allowed only when its text matches a `process` scope of
  both grants exactly. SQLite (spec 026) reaches files only through vo's own OS layer
  (`src/sqlite_vfs.c`), which checks every file SQLite opens (the database, its journal, files of
  `attach` and `vacuum into`) against both grants and opens it the same way; a database's effects are
  part of its type (`Database[uses file.read, file.write]`), and every statement on a file database
  checks them again at run time, so a handle built by hand (`new Database { id: 0 }`) is R0010.
  Temporary files stay in memory; WAL mode and the pragmas that name directories are refused.
- **S3 No other capabilities.** The only things a program can do are the effects above. The binary
  reaches 43 system calls (`src/platform/*.h`; spec 026 added `pread64`, `pwrite64`, `fsync` and
  `ftruncate` for SQLite's files). Processes are created only for tasks (spec 016: copies
  of the program, which share nothing), for server workers (spec 028: copies of the program that hold no
  connection and no listening socket, only pipes to their server) and for `process` (spec 017); signals
  are sent only to those children and their process groups; no native code
  loading (SQLite is built without extension loading), no raw sockets, no file descriptors a program
  can name. No environment variable, file or
  argument outside the grant changes what a program may do.
- **S4 Memory safety.** No program and no input (A1-A3) makes the toolchain read or write memory it
  does not own, or run code other than the program's: overflows, out-of-bounds access, division by zero,
  stack exhaustion and allocation failure are runtime errors with a code, not undefined behavior.
  SQLite's memory is its own fixed heap (64 MiB, outside the collected heap); a damaged database file is
  an `err("corrupt: ...")`, never a crash (tests/sqlite_fuzz.py).
- **S5 Bounded resources.** Time (`--timeout`, default 10 s, per request for servers), live memory
  (`--max-memory`; for `serve_workers`, per worker, as `--timeout` is per handler call and per setup), stack depth, HTTP responses (16 MiB), server requests (head 64 KiB, 100 headers, body
  16 MiB, 10 s to arrive) are limited, and reaching a limit is a reported error. `--timeout 0` turns the
  time limit off on purpose. Regular expressions (spec 025) are safe on hostile patterns and texts:
  matching never backtracks, so its time is linear in the text for every pattern (and stops at
  `--timeout`), and a pattern from data (`parse_regex`) is bounded (10,000 bytes, repetition counts up to
  1,000, 20,000 instructions after repetitions, groups nested 200 deep) or refused with an `err`; the
  compiled patterns are kept in a cache of 16. SQL statements stop at `--timeout` (a progress handler
  every 1,000 SQLite steps), waiting for another process's lock is bounded (5 s, never past the
  deadline), and SQLite's heap is fixed (64 MiB: a statement needing more is `err("nomem: ...")`).
- **S6 Commands that do not run programs are safe on hostile source.** `vo check`, `fmt`, `doc`, `query`,
  `explain`, `effects`, `package` and `edit` never execute program code, and write only what they
  document (`fmt` and `edit`: the program's files; `effects --update`: `vo.effects`; `package`:
  `vo.packages`; `--trace`/`--record FILE`: that file).
- **S7 Replay containment.** `vo replay` reads and writes nothing but the recording: no files, network,
  environment, clock or stdin. A hostile recording can only feed the program wrong inputs or be refused
  (T0008); it cannot widen the program's grant, which replay checks as in the recorded run.
- **S8 Package integrity.** A package runs only when its files hash to the pin in `vo.packages` (E0701),
  and the bytes that ran are the bytes that were hashed; a directory that cannot be read, or a link
  leading outside the package, refuses it (E0702). A package has no grant of its own and cannot be
  imported past its exports. An import never reaches a file outside the program's tree or a listed
  package through a link (E0211).
- **S9 Server robustness.** A vo server listens only on its declared address, reads up to 64 connections
  at once and answers them one handler call at a
  time (with `serve_workers`, one per worker, up to 64 workers, every worker with `main`'s grant), and
  keeps serving after malformed, oversized or failing requests; a worker that dies is replaced and only
  its request fails (500); no request makes it crash, grow without bound, or run outside its grant.

## Known limits (accepted, documented)

- **HTTPS, plain HTTP and DNS** (spec 019): `https://` requests are sent only after the server's
  certificate chain is verified against the machine's trust store (plus `--ca-file` authorities given
  by whoever runs the program), for validity now and for the host; TLS 1.3 and 1.2 with forward-secret
  authenticated ciphers only, and nothing turns verification off. This protects the confidentiality
  and integrity of the exchange and the server's identity. It does not cover: plain `http://` URLs
  (an A2 attacker on the path can change them), DNS (no DNSSEC; a spoofed address fails verification
  instead of connecting), certificate revocation (no OCSP or CRL: a revoked certificate is accepted until
  it expires), a compromised server or authority, and side channels. Programs must still treat network
  input as untrusted data, which S4 and S5 protect the toolchain from.
- **64 connections**: a vo server reads up to 64 connections at once (spec 016), each with 10 s to send
  its request; 64 slow clients can still hold it (a denial of service that the limits bound but do not
  prevent), and a handler that waits delays the next one.
- **A granted program is not confined** (spec 017): `process("git")` allows starting `git`, which then
  runs with the rights of the user running `vo`, outside the program's file, network and environment
  scopes. Granting a program grants everything it can do; whoever can replace that program's file (A4)
  decides what runs. Confining children to the grant is planned (roadmap).
- **Escaped grandchildren** (spec 017): a run kills the child's process group when the child ends or
  the time is up, and the child dies with `vo`; a grandchild that leaves the group (`setsid`), or any
  grandchild when `vo` itself is killed with SIGKILL, survives.
- **Console output is not filtered**: a program (or data it prints) can send terminal escape sequences
  to the runner's terminal.
- **Recordings contain the inputs**: file contents, variables and HTTP traffic a program read are in its
  recording, and so are the bytes `std.crypto.random_bytes` returned (tokens, keys, salts); recordings
  are as sensitive as those inputs. A server's recording keeps each request as it arrived: the
  passwords of sign-in forms and `Authorization` headers, and the session cookies of signed-in
  browsers (spec 052, decided to keep replay exact). Keep it like the application's database. Test recordings (`vo test --record`, spec 054) are meant to be committed: they write the values of `Authorization`, `Proxy-Authorization`, `Cookie`, `Set-Cookie` and of headers, query parameters and variables named with `--redact` as `[redacted]`; a secret in a body or a URL path is kept, so recording uses test credentials.
- **A target that is a value is checked when it is used** (spec 050): a function with scopes may call
  a helper or a package function that is given its target; a target outside the scopes stops the
  program at that operation (R0010), not at `vo check`. Literal targets passed to the file, `env` and
  `process` functions are checked before the program runs; literal hosts are not.
- **Browser sessions** (spec 052): `std.security`'s browser way signs a browser in with a session
  cookie. What it answers for each threat, and what it leaves to the application:
  - *a stolen cookie*: `HttpOnly` keeps it from scripts, `Secure` and the `__Host-` name keep it off
    plain HTTP and sibling subdomains, a session lasts a fixed lifetime, and sign-out ends it in the
    application's storage, so a copy stops working at once; a session is not bound to an address or a
    device;
  - *session fixation*: a session value is made only by a successful sign-in, from 16 random bytes,
    and an older session of the same browser is ended;
  - *cross-site request forgery*: a request signed in by the cookie with a method other than `GET`,
    `HEAD`, `OPTIONS`, and every post of the sign-in form, must say it comes from the application's own
    origin (`Sec-Fetch-Site`, else `Origin`), or it is refused with 403 before the operation runs;
    `SameSite=Lax` is a second layer. A `GET` route that changes something is not protected: changes
    are posts;
  - *scripts reading the cookie*: `HttpOnly` (keeping scripts off the frontend's pages is the
    frontend's part);
  - *open redirect*: the page to return to after signing in is followed only when it is a path of the
    application (`local_path`);
  - *the timing of the name lookup*: an unknown name costs the same password check as a wrong
    password;
  - *a copy of the session storage*: it holds the SHA-256 of each session value, never the value;
  - *guessing*: 128 random bits per session; *brute force* is not limited: every refusal is a security
    event with the name, which an application can count to slow or lock an account.
- **The grant is only as good as its review**: a broad grant (`file.read("./")`, an unscoped `net`) is
  honored as written.

## Open questions for the first review

Answered by [review 1](security-review-1.md) (both confirmed: H4, and H7-H9/C5/L6 among the parsers) and
fixed by spec 013. Recorded here so the review started from what was already known, not as accepted
limits:

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
