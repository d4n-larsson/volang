# Results: Security Fixes from Review 1

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | Every finding has a case (table below and the status table in `docs/security-review-1.md`). With the pre-spec binary (`6958225`) the 12 golden cases and 11 of the 12 finding step cases fail (three of them by hanging: L4's FIFO in `c5_replay_program` and `l4_fifo_source`, L3's NUL manifest in `l3_l5_manifest`); `h4_inside_link` passes there by design (it guards that inside links keep working; H4 itself is `race.py`). `server_attacks.py` fails (H8 takes 8 s, the H9 flood kills the server) and `pkg_race.py` runs unpinned code in 22 of 300 runs. |
| SC-002 | met | The review's scripts and programs re-run unchanged against the new binary: `net/utf8.py`, `chunked.py`, `inject.py` answer 400 (or 200 for a valid body) and the server stays alive; `evil_http.py` gets `ok`/`err` for every hostile response, never a runtime error; `dechunk_quadratic.py` 20,000 chunks in 0.00 s, 100,000 a 431 in 0.00 s; the 26 programs under `s1`, `s2`, `s4`, `tools` are refused at check (E0301/E0502/E0503/E0501) or stop with R0010/R0005/R0007; no file outside a grant is read or written. 0 of 32 findings reproduce. |
| SC-003 | met | `make security-check`: `race.py` 1,000 runs, 0 reads outside the scope; `rename.py` 1,000 runs, 0; `pkg_race.py` 1,000 runs, 0 unpinned runs (H6); `longcwd.py` refused. |
| SC-004 | met | `server_attacks.py` (one server, `--max-memory 64`): every attack answered or cut, a normal request answered after each, RSS at most 64 MiB (H10: 63 -> 63 MiB over 200 requests after an R0006). |
| SC-005 | met | Honest programs keep their results (the changed expectations are listed under Behavior changes: line numbers in `std/http.vo`, recording fingerprints, the new lambda frame, the descriptor number, and the four cases moved off literal paths). Gate, cross-platform suites and diff-asm: see Verification. |

## Findings

See the status table in `docs/security-review-1.md` (finding, status, case). All 32 are fixed.

## Verification

| Check | Result |
|---|---|
| `make gate` (x86-64) | passed: lint, golden 508, step 202 (139 + 35 repeated + the server, HTTP and replay sets), catalog, reference, fuzz, examples, diff-engines 1,000 (0 disagreements); ubtrap; diff-asm 2,000 (0 disagreements); gc-check (stress suite 508, stress diff-engines 1,000: 0 disagreements); speed geometric mean 0.881 (limit 1.10). The first run after the render depth limit failed gc-check (`l10_cyclic_expect` differed under the stress build at a 100,000 limit; see Deviations) |
| `make ARCH=aarch64 test`, `make ARCH=riscv64 test` | passed on both: golden 508, step 202 / 139 / 35 (the first run, with `openat2`, failed `effects/scope_ok` under qemu: see Deviations) |
| `make instrumented` | 508 of 508 (the first run failed `l10_cyclic_expect` on an ASan warning; fixed by the render depth limit, see Deviations) |
| `make diff-asm COUNT=10000` | 0 disagreements |
| `make security-check` | passed (SC-003, SC-004) |
| `make prop-eval` | 62 of 62 mutants caught |
| `make refactor-eval`, `make stdlib-eval` | passed |

## Behavior changes

- **Effect parameters take the caller's grant** (R1): a generic frame's `E` part is the caller's running
  grant, never the binding's requirement set. `tests/cases/run/vm_effect_forward.err` changed on purpose:
  the lambda's `PATH` read is now R0010 refused by `main`.
- **Root grant**: every run-time effect check also checks `main`'s (or the test's or property's) clause.
- **Literal targets** outside the declared scopes are E0503 at `vo check` (L1). Four existing cases that
  tested the run-time check through a literal path now pass the path through a `let`
  (`effects/absolute_path`, `effects/scope_escape`, `test/effect_denied_test`, `replay/denied`), and so
  does `tests/scope_fuzz.py`, which now also fuzzes the static check.
- **Scoped file opens** walk the resolved path below the scope directory without following links. Links
  inside a scope keep working (they are resolved by the check, absolute targets included, as long as the
  target lies inside the scope); a link swapped in after the check fails the access (R0010).
- **std.http**: request and response heads must be ASCII text in CRLF lines (400 for a request, `err` for
  a response); chunk-size lines, extensions and trailers count against the 64 KiB head limit (a flood of
  tiny chunks is 431); a response header whose name is not a token, or whose name or value holds CR, LF
  or NUL, becomes a 500. Request parsing runs inside the guarded call, so server traces gain an
  `anonymous function` frame. Client framing errors are `err("malformed response: ...")`.
- **Recordings**: server requests are recorded as framed (head plus decoded body); the client's raw
  response is still recorded as received and framed after replay, so old client recordings replay
  unchanged. Programs that import `std.http` have a new fingerprint (a "program differs" note on old
  recordings).
- **Replay** checks every file event against the grant, lexically, whatever its recorded result; the
  recorded program must be a relative `.vo` path without `..` naming a regular file; the reader rejects
  bad literals, overflowing integers and recordings larger than `--max-memory` (T0008).
- **Packages**: directory links inside a package are now followed and hashed (they were skipped), so the
  pin of a package with such a link changes once (E0701 until `vo package` is run again); links leading
  outside and unreadable directories refuse the package (E0702, new message). E0701's suggested command
  names the directory relative to the working directory.
- **New code E0211** `link_outside_tree`: an import resolving outside the program's tree (and outside
  every listed package) through a link.
- **Diagnostics** show control bytes as `\xNN` in human output.
- **Temporary files** of `fmt`, `edit`, `effects --update` and `package` are `<path>.vo-<tag>-<random>.tmp`,
  created exclusively.
- The start directory's descriptor is opened when the program loads, so the first descriptor a program
  sees is 4 (visible as `listener: int = 4` in `serve/handler_fails`).

## Deviations

- **No `openat2` (research R3).** The plan opened scoped files with `openat2(RESOLVE_BENEATH)`. qemu 8.2's
  user mode does not implement it (ENOSYS), so the ARM64 and RISC-V suites refused every scoped write.
  The research's fallback, a component-by-component `O_NOFOLLOW` walk of the already resolved path from
  the scope directory, is equally race-free (a component that became a link fails the open) and works on
  every kernel, so it replaced `openat2` instead of being kept as a second path: no kernel 5.6
  requirement, 25 system calls (one fewer than planned).
- **L1 is resolved, not lexical (research R2).** A purely lexical check refused paths the run allows
  (`sub/../in/f`, an absolute path inside the scope). The static check resolves the literal and the
  scopes as the run-time check does (links followed, from the working directory), so it never refuses
  what the run would allow; `scope_fuzz.py` checks that both agree on 300 random paths.
- **Framing lives in `serve.c`, used by `replay.c` for responses** (research R4 named `net.c`): the client
  frames after recording or replaying, which keeps the recording format raw.
- **Server memory**: framing a 16 MiB body first held three copies and raised R0006 under
  `--max-memory 64` before the handler ran (outside the guard). The framer now keeps one copy of a
  Content-Length message and sizes the read buffer exactly once the length is known.
- **Render depth limit**: `render_value` stops at 50,000 levels (as well as at low native-stack
  headroom or a full root stack). Under ASan, unwinding the full native stack after L10's R0005 printed
  a sanitizer warning; the limit makes the stop point the same on every build (50,000 stays below the
  stress build's smaller root stack, where a first try at 100,000 still differed), and the report now
  shows the start of each value (cut at 200 characters) instead of `...`.
- **Test harness**: `tests/steps.py` gained `mkfifo`, `\xNN` escapes in `request-raw` and `write`,
  `{bytes:N}` in `write`, and `rm` of a link to a directory.

## Follow-ups

- **L8 has no automated case**: testing the DNS question check needs a resolver the test controls
  (`/etc/resolv.conf` is fixed). Verified by tracing three runs (three different IDs) and live lookups.
- **C3 cases cover file.read, file.write and env**; net and serve go through the same root-grant check
  without a case of their own.
- **Old server recordings** of chunked requests replay with the chunk framing in the body (the request
  was recorded raw before this spec).
- `vo check` on an unreadable file prints a JSON summary line in human mode (pre-existing, unrelated).
