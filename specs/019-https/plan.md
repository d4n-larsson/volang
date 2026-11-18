# Implementation Plan: HTTPS

**Branch**: `019-https` | **Date**: 2026-11-18 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/019-https/spec.md`

## Summary

`std.http.get` and `post` accept `https://` URLs. vo does TLS with **Mbed TLS 4.1.2**, from the LTS
branch supported until March 2029, vendored unmodified under `third_party/mbedtls/` and built into the
single static binary (research R1). The library runs without libc through its own hooks (R2):
- memory: an allocator over a separate 16 MiB region;
- entropy: `getrandom`;
- time: `clock_gettime`;
- a few C functions behind shim headers.

The design:
- **Connection** (R3): a new std-only primitive `__https_exchange(host, port, request)` shares the
  connect code with plain HTTP. It does the handshake through callbacks that wait under the
  `--timeout` deadline, and sends the request only after verification.
- **Protocol** (R4): TLS 1.3 and TLS 1.2 with forward-secret AEAD suites only. Verification is always
  required; older protocols are compiled out.
- **Trust** (R5, R6): the machine's trust store, read on first use, plus `--ca-file FILE` on `vo run`
  and `vo test`, read at startup (T0002, new T0013).
- **Errors** (R7): nine fixed `err` messages.
- **Recording** (R10): the `http` event gains `"scheme":"https"`, and replay matches it.
- **Tests** (R11): the library's self-tests on three platforms, interoperability with OpenSSL and
  tlslite-ng, a tamper proxy, 1,000 hostile handshakes, and committed test certificates.
- **Governance** (R12): the constitution gains a section for vendored code (1.3.0 to 1.4.0, MINOR),
  applied first.

## Technical Context

**Language/Version**: C (C11, freestanding) for vo's code (`src/tls.c`, `src/tls_libc.c`, changes in
`src/net.c`, `src/replay.c` and the builtin tables); vendored C (Mbed TLS 4.1.2 with TF-PSA-Crypto
1.1); vo for `std/http.vo`; Python 3 for test servers and scripts.

**Primary Dependencies**:
- Mbed TLS 4.1.2, vendored (Apache-2.0).
- Test-only: Python's `ssl` (OpenSSL 3.0) and `tlslite-ng` (pinned, installed into `build/py/` by
  `make tls-deps`), and the `openssl` command for making test certificates.
- No runtime dependency is added. The machine's trust store is read as a file, when present.

**Storage**: the trust store and `--ca-file` files (read only); recordings gain one optional field.

**Testing**:
- golden cases `tests/cases/https/` (no network: E0503, T0001, T0002, T0013, the URL parsing errors);
- step cases `tests/cases/https_steps/` (local servers: success, each failure message, `--ca-file`,
  record and replay, the tamper proxy);
- scripts: `make tls-selftest`, `tests/https_interop.py`, `tests/tls_fuzz.py`, `tests/https_scale.py`;
- `tests/cross_replay.py` extended;
- the full gate on three platforms, instrumented.

**Target Platform**: Linux x86-64 (native), ARM64 and RISC-V 64 (native or qemu-user).

**Project Type**: compiler and runtime of a programming language, as a CLI.

**Performance Goals**:
- SC-004: a handshake and small request under 100 ms locally; 10 MB over HTTPS at least half the speed
  of HTTP;
- programs that do not use HTTPS pay nothing (lazy trust store), and the speed gate stays under 1.10.

**Constraints**:
- Principle II: T0013 catalogued; messages fixed.
- Principle III: the same results on every platform, recordings without secrets.
- Principle V: reference +200 tokens at most, estimated 150.
- Implementation constraints: vendored code under the amended rules; no assembly anywhere.

**Scale/Scope**:
- vo's own C, about 700 lines: `src/tls.c` (handshake, callbacks, trust store, error mapping and the
  memory, entropy and time hooks, about 450), `src/tls_libc.c` (about 120), and changes to `net.c`,
  `replay.c`, the builtin tables, `main.c` and the effect literal checks (about 130).
- Vendored: about 40 C files, roughly 60,000 lines.
- `std/http.vo`: about 30 changed lines.
- 1 code; about 10 golden cases, about 15 step cases, 4 scripts, test certificates.
- The binary grows by about 400 KiB.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Assessment |
|---|---|
| I. Agent-first design | **PASS.** No new syntax, function or spelling: an `https://` URL in the same `get`/`post`. One option (`--ca-file`) with one meaning; no way to disable verification, so no second behavior to learn. |
| II. Machine-readable feedback | **PASS.** T0013 is catalogued with a golden case; every TLS failure is an `err` value with one fixed message listed in `vo doc`; T0001, T0002, E0503, R0010 and R0014 gain documented triggers; recording schema 1 gains one optional field (an addition). |
| III. Determinism and defined behavior | **PASS.** Handshake randomness and the certificate clock never reach the program; the same server behavior gives the same result on all platforms (C-only build, self-tests compared across platforms). Recordings carry no secrets and replay needs no network. Hostile servers end in an `err` within fixed limits and the `--timeout` deadline (fuzzed). |
| IV. Test-first through the CLI | **PASS.** Golden and step cases for each scenario and each message land first; self-tests and interop scripts join `make test` and `make gate`. |
| V. Single source of truth, minimal surface | **PASS.** Reference, catalog, contracts, `docs/security.md` and `std/http.vo` docs change together (about 150 tokens). Client only, with serving on the roadmap; no resumption, client certificates or HTTP/2. |
| Implementation constraints | **PASS (constitution 1.4.0).** The amendment applied by T001 allows vendored code under `third_party/` with the same build constraints as vo's own code (freestanding, no system libraries, no assembly, identical results, instrumented build), exempt only from the strict warning set. The choice is justified in Complexity Tracking. |
| Workflow and quality gates | **PASS.** The amendment lands as the first task, with its Sync Impact Report; reference growth is stated; the gate runs on three platforms. |

**Post-design re-check (after Phase 1)**: unchanged. The one violation is resolved by the amendment
that the plan schedules first; every other row passes. No spec line changes.

## Project Structure

### Documentation (this feature)

```text
specs/019-https/
├── plan.md              # This file
├── research.md          # Phase 0: R1-R14
├── data-model.md        # Phase 1: URL, connection, trust anchors, outcomes, the http event
├── quickstart.md        # Phase 1
├── contracts/
│   ├── library.md       # std.http with HTTPS, err messages, reference text
│   ├── cli.md           # --ca-file, the trust store
│   ├── codes.md         # T0013 and new triggers
│   ├── recording.md     # the scheme field
│   └── constitution-amendment.md
├── checklists/requirements.md
└── tasks.md             # Phase 2 (/speckit-tasks)
```

### Source Code (repository root)

```text
third_party/
├── mbedtls/             # NEW: Mbed TLS 4.1.2, kept files only, unmodified; LICENSE; VENDOR.md
└── shim/                # NEW: string.h, stdlib.h, stdio.h, time.h declarations for the vendored code
src/
├── tls.c                # NEW: tls_exchange (handshake, verify, send, read), bio callbacks with
│                        #   deadlines, trust store and --ca-file loading, error mapping, hooks
├── tls_config.h         # NEW: the library configuration (versions, suites, groups, modules off)
├── tls_libc.c           # NEW: the C functions the library needs that vo lacks
├── net.c                # net_open shared by HTTP and HTTPS
├── replay.c             # io_https_exchange; the scheme field; replay matching
├── check_expr.c, check_effects.c, rt_call.c, vo.h  # __https_exchange (id 65, net, std only);
│                        #   https:// literals for E0503
├── main.c               # --ca-file, its only_with rule, startup loading (T0002/T0013)
└── platform/*.h         # SYS_getrandom where missing
std/http.vo              # https:// URLs, port 443, scheme kept, __https_exchange; docs list the errs
.specify/memory/constitution.md   # amendment 1.4.0
docs/reference.md, docs/errors.md, docs/security.md, docs/roadmap.md, README.md
site/docs/guide/standard-library.md (or a new HTTPS section), site tools pages for run/test
tests/tls/               # NEW: certs/ (committed), make_certs.py, selftest.c
tests/https_server.py, tests/https_interop.py, tests/tls_fuzz.py, tests/https_scale.py   # NEW
tests/cases/https/, tests/cases/https_steps/   # NEW
tools/vendor_mbedtls.sh  # NEW
Makefile                 # vendored objects with their own flags; tls-selftest, tls-deps; gate steps
```

**Structure Decision**: the existing single-binary layout plus `third_party/` for vendored code (new
by the amendment). vo's adaptation layer is one file, `src/tls.c`, which keeps every library call in
one place, so an update touches one file.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|---|---|---|
| Third-party C in the binary, exempt from the strict warning set (amendment 1.4.0) | HTTPS needs TLS and X.509 with widely reviewed cryptography; FR-013 and the clarification chose a vendored LTS library | Writing TLS 1.3 ourselves: the largest and riskiest work, with crypto only we review. Linking the system's OpenSSL: breaks the static, dependency-free binary. Running curl: a runtime dependency, and error texts vary by version |

Deliberate shortcuts, each marked `ponytail:` in code:

| Shortcut | Ceiling | Upgrade path |
|---|---|---|
| One 16 MiB TLS memory region per process (R2) | very large trust stores or chains | grow the region, or map per connection |
| No session resumption or connection reuse | one handshake per request (about 5 to 20 ms locally) | keep-alive and tickets when an agent's program shows the cost |
| No revocation checking (OCSP, CRL) | a revoked certificate is still accepted until it expires | OCSP stapling in a later spec; documented in docs/security.md |
| Only three trust store paths | other distributions need `--ca-file` | add paths as they are reported |
