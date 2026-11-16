# Research: Security Fixes from Review 1

Finding IDs refer to [docs/security-review-1.md](../../docs/security-review-1.md).

## R1: The root grant (C2, C3, H1)

- **Decision:** the runtime keeps a pointer to the **root grant**: `main`'s declared set in `vo run` and
  `vo replay`, the test's or property's declared set in `vo test` (set where each starts). Every run-time
  effect check (`rt_check_bit`, `rt_check_name`, `rt_check_path`) checks the running frame's grant **and**
  the root grant; either refusing is R0010, naming the function whose grant refused (spec 009's rule, the
  root's function when the root refused).
- **Effect parameters at run time (C3, FR-002):** a generic frame's grant no longer substitutes the
  binding of `E` (a requirement set with "any scope" bits) into its declaration. The `E` part takes the
  caller's running grant, as library and anonymous frames already do; the concrete part stays the
  declaration. A requirement set is never a grant.
- **Rationale:** one check against the set a person reviewed closes every way a callee's own declaration
  or a binding widens the grant (C2, C3, H1 and anything not yet found), without relying on the static
  check being perfect. Cost: one more set test per effectful builtin call (effects are I/O-bound).

## R2: Static checking of nested function types (C2, L1)

- **Decision:** after a generic call's type arguments are inferred, every argument is checked against its
  instantiated parameter type (`expect_type` on all arguments, not only top-level function values), and
  the same for generic variant payloads (`vcase_generic`). `ty_unify` compares the effects of function
  types, including an already-bound effect parameter. Containers are invariant, so a function value whose
  effects differ inside a `list`/`map`/`result`/record/variant is a type error (E0301/E0502 with the
  effects named).
- **L1 (SHOULD):** a string literal passed directly as the path of `read_file`/`write_file`, the name of
  `env`, or the address of `serve` is checked lexically against the caller's scopes: outside them is
  E0503 (the existing "too narrow a scope"). Non-literal targets stay run-time only.

## R3: Opening files without redirection (C1, H2, H3, H4)

- **Decision:**
  - **NUL:** any path containing a NUL byte is refused (R0010) before checking.
  - **Start directory:** at startup the runtime opens the start directory once (`O_PATH|O_DIRECTORY`) and
    keeps the descriptor; if it, or reading its path, fails, every scoped file access is refused (R0010).
    Relative paths are opened from that descriptor, never from the current working directory, so renaming
    the directory while the program runs changes nothing (H3).
  - **Scoped opens:** the check works as today (lexical resolution plus links) to find the scope the path
    is under and the path relative to it. The file is then opened with `openat2(scope_fd, rel, flags,
    RESOLVE_BENEATH | RESOLVE_NO_MAGICLINKS)`, where `scope_fd` is the scope directory opened beneath the
    start descriptor the same way. The kernel then guarantees that the open stays inside the scope however
    links change between the check and the open (H4): a swapped link fails with an error (R0010) instead
    of escaping.
  - **Unscoped grants** (`file.read` with no scope) open from the start descriptor with plain `openat`.
- **Behavior change (documented):** a symbolic link inside a scope whose target is absolute, or leaves
  the scope, can no longer be followed for a scoped access; relative links that stay inside work.
- **Requirement:** Linux 5.6 or later (openat2). On `ENOSYS` scoped file access is refused with a message
  naming the kernel requirement. Two system calls are added to the platform layer: `openat2` (437 on all
  three) and `getrandom` (R6).
- **Alternatives:** a component-by-component `O_NOFOLLOW` walk in user space (more code, the same result,
  kept as the fallback if a supported platform lacks openat2); checking twice (still racy, rejected).

## R4: HTTP framing and parsing (H7, H8, H9, M2, M5, L9)

- **Decision:** one framing implementation in C, used by the server (requests) and the client
  (responses): it reads the head (bounded), validates that the head is ASCII (bytes 0x80-0xFF, NUL and bare
  CR are refused: 400 for requests, `err` for responses), and returns the head text plus the **decoded**
  body (`Content-Length` or chunked, chunks joined in C). `dechunk` and the body handling in
  `std/http.vo` go away; vo parses only head lines (ASCII), which cannot raise R0007.
  - Total size cap per request: head 64 KiB (request line, headers, chunk-size lines, extensions and
    trailers all count) plus body 16 MiB; more is 413/431, an error for the client.
  - Reading checks the 10 s deadline and the stop signal on every iteration and resumes scanning where the
    previous read stopped (L9).
  - In `serve`, request parsing and the handler run inside the guarded call (`__serve_call` gets a vo
    function that parses and handles), so parsing has the per-request deadline and any remaining failure
    is a 500, never the end of the server.
  - `response_text` refuses (500) a response whose header names are not tokens or whose names or values
    contain CR, LF or NUL (M5).
- **Rationale:** the framing and the body decoding disagreeing was a crash (H7); making C the only place
  that decides where a body ends removes the class.

## R5: Symbolic links in the toolchain (H5, H6, M3, M4, L3, L4)

- **Temporary files (M3):** `write_file_safely` and `ed_write_tmp` create the temporary file with
  `O_CREAT|O_EXCL|O_NOFOLLOW` and an unpredictable name (`getrandom`); the rename is already safe.
- **Imports (M4):** each import's file is resolved (links followed) and must lie under the program's tree
  (the entry file's directory) or, for package imports, under the package's directory; otherwise
  **E0211 `link_outside_tree`** naming the link. The missing-import scan applies the same rule.
- **Packages (H5, H6):** `pkg_hash` follows directory links that stay inside the package directory,
  refuses the package (E0702) on any directory it cannot read or link leading outside, and keeps the bytes
  it hashed: the loader registers package files from those bytes instead of reading them again.
- **Regular files only (L4):** every source, package, recording and manifest read opens with
  `O_NONBLOCK`, checks with `statx` that the file is regular, and fails otherwise (T0002, E0702, T0008).
- **Manifests (L3):** control bytes in a token are E0703.

## R6: Smaller fixes

- **C4:** `hof_sort_by` copies the elements into a rooted snapshot before the first key call and works on
  it; if the list's length or buffer changed when the keys are done, R0007 ("the list changed while
  `sort_by` ran").
- **C5:** `vo replay` loads the recorded `program` only if it is a relative path ending in `.vo`, without
  `..` or an empty component, naming a regular file; otherwise T0008.
- **M1, L2:** replay checks every file event against the grant lexically (the scope logic without reading
  links), whatever the recorded result.
- **L6:** literals must match exactly, integers must not overflow, and a recording larger than
  `--max-memory` is T0008.
- **H10, L11:** `rt_poll` does not raise while the collector runs (a `gc_busy` flag; the timeout is raised
  at the next poll after it); `rt_serve_call`'s failure path restores `gc_on`, the out-of-memory hook and
  the error extras. Properties (`pr_try`) get the same restore.
- **H11, L10:** `values_equal` and `render_value` call `check_stack` (R0005) and treat identical objects as
  equal without descending; a full root stack during a run is R0005 instead of T0003.
- **L5:** E0701's suggested command prints the package directory relative to the working directory.
- **L7:** human diagnostics show bytes below 0x20 (except tab) and 0x7F as `\xNN`.
- **L8:** DNS queries use a random ID (`getrandom`) and accept only answers whose question matches.

## R7: Proving each finding

- Each finding gets a case built from the review's reproduction (golden, step, or a script under
  `tests/security/` for races and attacks that need timing or a helper process), written before its fix
  and failing on `6958225`. `docs/security-review-1.md` gains a status column: fixed, with the case.
- The race (H4) and rename (H3) scripts run 1,000 times in `make security-check` (outside `make test`,
  which must stay fast); the server attack scripts run there too.
