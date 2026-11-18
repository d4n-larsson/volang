---

description: "Task list for vo-lang spec 019: HTTPS"
---

# Tasks: HTTPS

**Input**: Design documents from `specs/019-https/`

**Prerequisites**: plan.md, spec.md, research.md (R1-R14), data-model.md, contracts/library.md,
contracts/cli.md, contracts/codes.md, contracts/recording.md, contracts/constitution-amendment.md,
quickstart.md

**Tests**: REQUIRED (constitution IV): golden and step cases land first and fail before each piece.

**Organization**:
- Setup: the constitution amendment, vendoring, test certificates and servers.
- Foundational: the vendored library builds and passes its self-tests on three platforms; the shared
  connect code, the primitive and `--ca-file`.
- One phase per user story: US1 and US2 are P1, US3 and US4 are P2.
- Docs and the gate.

`--ca-file` is foundational because every local HTTPS test needs a test authority. US3 then proves the
story's acceptance scenarios.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Golden cases** (`tests/cases/https/`) need no network: check-time codes, usage errors, URL parsing.
- **Step cases** (`tests/cases/https_steps/`) use local HTTPS servers through a new `tests/steps.py`
  action (T006). Tests never need the internet.
- **Regression rule**: existing programs keep byte-identical results, except the result for an
  `https://` URL, which used to be `err("https is not supported")`. Run `make test` after every task.
- **vo's own C** (`src/tls.c`, `src/tls_libc.c`): the strict warning set, no UB, explicit lengths on
  every buffer, and every library call in `src/tls.c` only.
- **Vendored C** (`third_party/`): unmodified, compiled under the amended rules (T001).
- **Shortcuts**: marked `ponytail:` (plan.md, Complexity Tracking).

---

## Phase 1: Setup

- [X] T001 Apply the constitution amendment in `.specify/memory/constitution.md`:
  - add the three bullets of `contracts/constitution-amendment.md` to "Implementation Constraints";
  - bump the version to 1.4.0, set the amendment date, and update the Sync Impact Report (MINOR: a new
    rule scoping existing ones; templates reviewed);
  - check that `specs/019-https/plan.md`'s Constitution Check now reads PASS for every row.
- [X] T002 [P] Create `specs/019-https/results.md` with the SC-001 to SC-006 table, Verification,
  Behavior changes and Deviations, as in `specs/018-agent-testing/results.md`. Add `019-https` to the
  spec tuple in `tests/catalog.py`.
- [X] T003 Vendoring:
  - write `tools/vendor_mbedtls.sh VERSION SHA256` (research R13). It downloads
    `https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-VERSION/mbedtls-VERSION.tar.bz2`,
    checks the SHA-256 and extracts into `third_party/mbedtls/` only:
    - `include/`, the `library/` files listed in the script, and `LICENSE`;
    - from `tf-psa-crypto/`: `include/`, `core/`, `drivers/builtin/{include,src}`, `platform/`,
      `utilities/` and the `include/` of `drivers/everest` and `drivers/p256-m` if the config needs
      them;
  - then it writes `third_party/mbedtls/VENDOR.md`: version, URL, SHA-256, license (Apache-2.0),
    the kept file list, and how to update;
  - run it for 4.1.2 (record the SHA-256 of the tarball as downloaded);
  - the initial file list is the TLS client, X.509 parsing, PEM, base64, ASN.1, bignum, ECP, ECDSA,
    RSA, SHA-1/224/256/384/512, HMAC, HKDF, AES, GCM, ChaCha20, Poly1305, the PSA core, the memory
    buffer allocator and platform files. Narrow or extend it in T007 from the link result.
- [X] T004 [P] Test certificates (research R11):
  - `tests/tls/make_certs.py` uses the `openssl` command to write `tests/tls/certs/`:
    - `test-ca.pem`/`.key` and an intermediate `test-int.pem`;
    - `a.test` (chain: leaf + intermediate; SAN `a.test`, `127.0.0.1`; 100 years);
    - `expired` (fixed past dates via `openssl ca -startdate/-enddate`);
    - `future` (not valid yet);
    - `other-name` (SAN `b.test` only);
    - `self-signed`;
    - `rogue` (signed by `rogue-ca.pem`, not given to vo);
    - `no-intermediate` (leaf only, so the server omits the intermediate).
  - Commit the generated files. The script is deterministic in what it creates, not in the key bytes.
- [X] T005 [P] `tests/https_server.py`:
  - **Routes**: those of `tests/http_server.py` (import its handler).
  - **Implementations**: wraps them in TLS with Python's `ssl` (OpenSSL), or with `--impl tlslite`
    using `tlslite-ng` from `build/py/`.
  - **Options**:
    - `--cert NAME` (from `tests/tls/certs/`);
    - `--tls12-only`, `--tls13-only` and `--tls10-only` (TLS 1.0 only, which vo must refuse);
    - `--cipher SUITE`;
    - `--tamper`: a proxy mode that flips one byte of the first application-data record from the
      server.
  - Prints `port N` when listening, like `http_server.py`.
- [X] T006 [P] `tests/steps.py`:
  - add the action `server https NAME [OPTIONS...]`. It starts `tests/https_server.py` on a free port
    and makes `{port}` available, like `server http`; `stop-server` stops it.
  - add the action `no-trust-store vo ARGS...`, which runs vo where none of the three trust store paths
    exist, with `unshare -rm` and tmpfs mounts over `/etc/ssl` and `/etc/pki`. When `unshare` is not
    permitted, the step prints a fixed "skipped: no user namespaces" line, and its case expects either.
  - document both in the module docstring.
  - add the Makefile target `tls-deps`: `pip install --target build/py tlslite-ng==VERSION` with
    `--require-hashes` from `tests/tls/requirements.txt` (pinned version and hash).

---

## Phase 2: Foundational (blocks every story)

- [X] T007 Building the library:
  - `third_party/shim/` holds `string.h`, `stdlib.h`, `stdio.h`, `time.h` (and `errno.h`, `assert.h`
    if needed), with only the declarations and types the vendored files use.
  - `src/tls_config.h` is the library configuration (research R2, R4):
    - on: TLS 1.2 and 1.3 client only, X.509 verification, PEM, `MBEDTLS_PLATFORM_MEMORY` with the
      buffer allocator, `MBEDTLS_PSA_DRIVER_GET_ENTROPY`, `MBEDTLS_HAVE_TIME_DATE` with
      `MBEDTLS_PLATFORM_TIME_ALT` and `MBEDTLS_PLATFORM_GMTIME_R_ALT`;
    - groups x25519, P-256, P-384; the suites of R4; ALPN;
    - off: `MBEDTLS_HAVE_ASM`, hardware AES, SHA and other acceleration, `MBEDTLS_FS_IO`, the debug
      and error modules, PSA file storage, `net_sockets`, session tickets, servers, SSL 3 to TLS 1.1,
      CBC suites and RSA key exchange.
  - The `Makefile` compiles `third_party/mbedtls` sources into `$(BUILD)/third_party/*.o`:
    - with vo's freestanding flags (`-ffreestanding -fno-builtin -nostdlib -ffp-contract=off -O2`);
    - with `-I third_party/shim -I third_party/mbedtls/include ...` and the config macros;
    - with the upstream's warnings and no `-Werror` (amendment);
    - for all three `ARCH`es, linked into `vo`.

  The build succeeds on x86-64, `ARCH=aarch64` and `ARCH=riscv64` with no undefined symbols beyond
  those T008 provides.
- [X] T008 The hooks:
  - `src/tls_libc.c`: the C functions the link reports missing (expected: `memcmp`, `memmove`,
    `strlen`, `strncmp`, `strchr`, a minimal `snprintf`/`vsnprintf` for the few formatted strings),
    each with an explicit length, under vo's strict warnings.
  - In `src/tls.c`:
    - `mbedtls_platform_get_entropy` over `SYS_getrandom`, looping on short reads and EINTR; add the
      syscall number to `src/platform/aarch64.h` and `riscv64.h` if missing;
    - the time hooks over `clock_gettime(CLOCK_REALTIME)` with a civil-date conversion;
    - `tls_init()`: maps the 16 MiB region lazily (`ponytail:`), calls the buffer allocator init and
      `psa_crypto_init` once.

  `make` links with zero undefined symbols on all three platforms; `make test` is unchanged.
- [X] T009 [P] The self-test:
  - `tests/tls/selftest.c` calls the library's self-test functions for every enabled algorithm and
    prints one line each plus a summary.
  - The Makefile target `tls-selftest` links it with the vendored objects, `src/tls_libc.c` and the
    hooks into a freestanding binary per `ARCH`, runs it natively and under qemu, and requires
    "all passed" and identical output on the three platforms. Add the target to `make test`.
- [X] T010 `src/net.c`:
  - move resolving and connecting out of `rt_http_exchange` into
    `long net_open(Str *host, long port, Variant **err)`, which returns the connected non-blocking fd
    or sets the existing error results;
  - make `net_wait` and the send loop callable from `src/tls.c` (prototypes in `src/vo.h`);
  - plain HTTP behavior is unchanged (`make test`).
- [X] T011 The primitive `__https_exchange(host, port, request)`:
  - builtin id 65 (`B_HTTPS`, `B_LAST` 65), the name in `src/check_expr.c` (std only) with the
    signature of `__http_exchange`, `[65] = EF_NET` in `src/check_effects.c`, and the dispatch to
    `io_https_exchange` in `src/rt_call.c` (static_assert updated);
  - `io_https_exchange` in `src/replay.c` does the host check (R0010), then calls
    `tls_exchange(host, port, request)` (T016) and frames the result like `io_http_exchange`. Factor
    the shared part into one function with a `tls` flag; recording comes in US4.
- [X] T012 `--ca-file FILE` (research R6):
  - parse it, repeatable, in `src/main.c`, with an `only_with` rule: only `run` and `test` (T0001;
    `replay` included);
  - before the program runs, `tls_add_ca_file(path)` in `src/tls.c` reads and parses each file into
    the trust anchors:
    - an unreadable file is T0002 (exit 66);
    - no certificate parsed is the new T0013 `invalid_certificate_file` (exit 64) with the message of
      `contracts/codes.md`;
  - add the catalog entry for T0013 in `docs/errors.md`, after T0012;
  - golden cases in `tests/cases/https/`: `bad_ca_file` (a non-PEM file in the case directory, `.cmd`
    `run --ca-file {dir}/not_a_cert.txt {}`), `missing_ca_file`, `ca_file_with_replay`,
    `ca_file_with_check`, `explain_t0013`.

**Checkpoint**: `make test` passes; `make tls-selftest` passes on three platforms; the binary has the
library, and `--ca-file` parses.

---

## Phase 3: User Story 1 — Call an HTTPS API (P1) 🎯 MVP

**Goal**: `get` and `post` work with `https://` URLs exactly as with `http://`.

**Independent Test**: step cases against a local HTTPS server signed by the test authority (given with
`--ca-file`) return the same `Response` values as the plain HTTP cases.

### Tests (first, failing)

- [X] T013 [P] [US1] Golden cases in `tests/cases/https/`:
  - `literal_outside` (`get("https://other.test/")` under `uses net("a.test")`: E0503);
  - `url_forms` (a program printing `parse`-level errors for `"HTTPS://a.test"` accepted,
    `"https://user@a.test/"` refused as today, `"ftp://x/"` with the new message
    `not an http:// or https:// URL`);
  - `outside_runtime` (a host built at run time outside the scope: R0010 before any connection).
- [X] T014 [P] [US1] Step cases in `tests/cases/https_steps/`, each starting `server https a.test` and
  running with `--ca-file test-ca.pem` (copied into the case):
  - `get` (`/plain`, `/json`, `/chunked`, `/close`, `/status/404`, `/headers` at
    `https://127.0.0.1:{port}/...`, with outputs identical to the HTTP step cases' outputs; reuse their
    programs with `https`);
  - `post` (a JSON body echoed);
  - `redirect` (the 302 is returned, not followed);
  - `timeout` (`/slow` with `--timeout 1`: R0009);
  - `limit` (a body over 16 MiB: the existing err);
  - `name_host` (`https://a.test:{port}/` with `a.test` resolved through a hosts entry is not possible
    in a step case, so use `127.0.0.1`, which is in the certificate's addresses. Note in the case
    that name verification is covered by US2's cases).

### Implementation

- [X] T015 [US1] `std/http.vo`:
  - `parse_url` accepts `https://` in any case, keeps `scheme` in `Url`, uses default port 443 for it,
    and changes the error for other schemes to `not an http:// or https:// URL: ...`;
  - `exchange` calls `__https_exchange` when the scheme is `https`;
  - update the module comment (no longer "no https") and the doc examples
    (`get("https://example.com/")` no longer an err example: use an `ftp://` one);
  - in `src/check_effects.c`, the literal-target check of `get` and `post` (spec 013 L1) takes the host
    from `https://` URLs too (E0503).
- [X] T016 [US1] `tls_exchange(Str *host, long port, Str *request)` in `src/tls.c`, returning a
  `result[str]` like `rt_http_exchange`:
  - `net_open`, then the SSL config (client, TLS 1.2 to 1.3, `VERIFY_REQUIRED`, the anchors, ALPN
    `http/1.1`) and `mbedtls_ssl_set_hostname(host)`;
  - BIO callbacks over the fd: non-blocking; on WANT_READ or WANT_WRITE they call
    `net_wait(..., deadline)` and give R0009 at the `--timeout` deadline as plain HTTP does;
  - the handshake, then `mbedtls_ssl_get_verify_result` (0 required);
  - write the request, read until close (`close_notify` or EOF) up to 16 MiB, `mbedtls_ssl_close_notify`
    and free everything.

  Errors here are generic ("the TLS handshake with HOST:PORT failed") until US2 maps them; the
  machine's trust store loads lazily on first use (research R5).

**Checkpoint**: T013-T014 pass.

---

## Phase 4: User Story 2 — Never talk to a server that cannot be verified (P1)

**Goal**: every verification failure is an `err` with its documented message; no request bytes are sent
before verification; no way to disable it.

**Independent Test**: one step case per message of `contracts/library.md`, plus a tamper proxy and a TLS
1.0-only server; none delivers a response.

### Tests (first, failing)

- [X] T017 [P] [US2] Step cases in `tests/cases/https_steps/`, each running with
  `--ca-file test-ca.pem`:
  - `untrusted` (server `rogue`);
  - `missing_intermediate` (`no-intermediate`);
  - `self_signed`;
  - `wrong_host` (`other-name`);
  - `expired`;
  - `not_yet_valid` (`future`);
  - `old_protocol` (`--tls10-only`);
  - `tampered` (`--tamper`);
  - `no_trust_store` (the `no-trust-store` action, without `--ca-file`).

  Each expects the exact `err` text. The server logs whether it received request bytes, and the case
  checks it received none before verification.
- [X] T018 [P] [US2] `tests/tls_fuzz.py`: 1,000 seeded local servers. Each completes TCP, then sends a
  mutated or truncated ServerHello, certificate or handshake record (bit flips, length fields at 0,
  max and random, records split at random points, random bytes after the hello). vo runs
  `get("https://127.0.0.1:PORT/")` with `--timeout 5` and `--ca-file`. Every run must end with exit 0
  and an `err` or a response, never exit 70 (R0006) or 125, never past the timeout. Add it to
  `make gate`.

### Implementation

- [X] T019 [US2] Error mapping in `src/tls.c` (research R7):
  - map the library's handshake errors and verify flags to the nine messages, with the precedence of
    R7 (not trusted < self-signed detection < wrong host < expired < not yet valid; protocol and
    algorithm alerts; bad record MAC means tampered; anything else is "handshake failed");
  - self-signed: a one-certificate peer chain whose issuer equals its subject;
  - the "no trusted certificate authorities found" err when no store path exists and no `--ca-file`
    was given.
- [X] T020 [US2] Verification cannot be weakened:
  - a golden case `tests/cases/https/no_insecure_option` runs `vo run --insecure x.vo` and
    `vo run --no-verify x.vo` (T0001 unknown option);
  - `tests/catalog.py` or a small check greps `src/tls.c` for `MBEDTLS_SSL_VERIFY_NONE` and
    `MBEDTLS_SSL_VERIFY_OPTIONAL` and fails if either appears.

**Checkpoint**: T017-T018 pass.

---

## Phase 5: User Story 3 — Trust a private authority on purpose (P2)

**Goal**: `--ca-file` adds exactly the authorities named, for that run only.

**Independent Test**: fails without the file, succeeds with it, still fails for another authority;
replay needs neither.

### Tests (first, failing)

- [X] T021 [P] [US3] Step cases in `tests/cases/https_steps/`:
  - `ca_file` (server `a.test`: without `--ca-file`, the err "not trusted"; with it, the response; with
    `--ca-file rogue-ca.pem` only, still not trusted against `a.test`);
  - `ca_file_two` (two `--ca-file` options, both servers trusted);
  - `ca_file_bundle` (one file holding two authorities).

### Implementation

- [X] T022 [US3] Make sure the anchors are the machine store plus the `--ca-file` certificates:
  - not the store when `--ca-file` is given, but in addition to it;
  - parsed once and inherited by tasks: add a case `ca_file_tasks`, with `concurrent_map` over three
    HTTPS gets with `--ca-file`;
  - `--ca-file` documented in `site/docs/tools/run.md` and `test.md` Options tables, so the options
    check passes.

**Checkpoint**: T021 passes.

---

## Phase 6: User Story 4 — Replay and test HTTPS runs offline (P2)

**Goal**: recordings keep HTTPS requests with their scheme and no secrets; replay needs no network.

**Independent Test**: record against a local server, stop it, replay identically; a scheme change is
R0014.

### Tests (first, failing)

- [X] T023 [P] [US4] Step cases in `tests/cases/https_steps/`:
  - `record_replay` (record with `--record r.jsonl --ca-file ...`, `stop-server`, then `vo replay`
    gives the same output; `keep r.jsonl` shows `"scheme":"https"` and no key material);
  - `replay_scheme_mismatch` (a recording of `http://127.0.0.1:P/plain` replayed by a program asking
    `https://127.0.0.1:P/plain`: R0014 naming both);
  - `replay_old_recording` (an existing http recording without `scheme` still replays).
- [X] T024 [P] [US4] Extend `tests/cross_replay.py` with `https_steps/record_replay`'s recording, which
  must replay identically on all three platforms.

### Implementation

- [X] T025 [US4] `src/replay.c`:
  - the recorder writes `"scheme":"https"` for HTTPS http events (a new field, written after `port`);
  - the reader accepts it (absent means http);
  - replay matching compares scheme, host, port and request line;
  - `http_desc` shows `https://host:port` or `http://host:port` in R0014 messages for recordings with
    a scheme. Keep the old text for old ones, so existing replay cases stay unchanged.

**Checkpoint**: T023-T024 pass.

---

## Phase 7: Polish and the gate

- [X] T026 [P] Reference, with the text of `contracts/library.md`: the modules row, the HTTPS bullet,
  and `[--ca-file FILE]` on the `run` and `test` lines. Record the token growth (limit 200) in
  `results.md`.
- [X] T027 [P] `docs/security.md`:
  - replace the out-of-scope line and the "Plain HTTP and DNS" known limit: HTTPS protects the
    connection's confidentiality and integrity and the server's identity; DNS, plain `http://`,
    revocation (no OCSP/CRL), server compromise and side channels are not covered;
  - add the vendored library to the trust boundary, with how updates are applied (amendment).
- [X] T028 [P] `std/http.vo` documentation comments list every `err` message of `contracts/library.md`.
  `tests/doc_conformance.py` and `vo doc std.http.get` show them, with doc examples as `EXPR is VALUE`
  that run offline. Update the `doc/all` cases if their output changes.
- [X] T029 [P] Site:
  - `site/docs/guide/standard-library.md` (the HTTP section) gains HTTPS: verification, `--ca-file`,
    the messages;
  - the run and test tool pages list `--ca-file`;
  - `docs/roadmap.md`: 019 to **Done**, with a "Not done" list (serving over HTTPS, already a roadmap
    row; resumption; revocation);
  - `README.md` mentions HTTPS.
- [X] T030 [P] `tests/https_interop.py` (SC-002):
  - against `tests/https_server.py` with OpenSSL: TLS 1.3 with each of the three suites, and TLS 1.2
    with each accepted ECDHE-ECDSA and ECDHE-RSA suite (an RSA and an ECDSA leaf; add `a.test-ecdsa`
    to the certificates);
  - against `--impl tlslite` for TLS 1.3 and 1.2;
  - every handshake succeeds and returns `/plain`.

  `make test` runs the OpenSSL part; `make gate` requires the tlslite part (after `make tls-deps`).
- [X] T031 [P] `tests/https_scale.py` (SC-004):
  - the median handshake and small request over 20 runs is under 100 ms;
  - a 10 MB `/big` route (add it to `tests/http_server.py`) over HTTPS takes at most twice the time over
    HTTP.

  Print the figures; add the script to `make gate`.
- [X] T032 Gate in `Makefile`: an "https determinism" step,
  `python3 tests/steps.py --repeat 20 tests/cases/https_steps` (except the fuzz) and
  `tests/run.sh --repeat 20 tests/cases/https`, plus `tests/tls_fuzz.py`, `tests/https_interop.py`
  (both parts) and `tests/https_scale.py`. `make instrumented` builds the vendored code with the
  sanitizers too.
- [X] T033 Full verification, recorded in `results.md`:
  - on x86-64: `make test`, `make gate`, `make instrumented` (with
    `VO_BIN=build-instrumented/vo python3 tests/steps.py tests/cases/https_steps`) and
    `make security-check`;
  - on the other two: `make ARCH=aarch64 test` and `make ARCH=riscv64 test`, with `make tls-selftest`
    on all three;
  - measure the binary growth;
  - fill SC-001 to SC-006 and the Deviations;
  - set the spec status to Implemented and mark every task [X].

---

## Dependencies & Execution Order

- **Phase 1** (T001-T006): T001 first, since the amendment allows T003. T002, T004, T005 and T006 in
  parallel after T001; T003 after T001.
- **Phase 2** (T007-T012): T007, then T008, then T009 (parallel with T010). T011 needs T010. T012
  needs T008 (it parses certificates). Blocks every story.
- **US1** (T013-T016): after Phase 2. T013 and T014 in parallel; then T015 and T016 (T016 is the core).
- **US2** (T017-T020): after US1's T016. T017 and T018 in parallel; then T019; T020 is independent.
- **US3** (T021-T022): after US1 (needs working HTTPS); independent of US2.
- **US4** (T023-T025): after US1; independent of US2 and US3.
- **Polish** (T026-T033): T026-T031 after the stories they document; T032 and T033 last.

## Parallel Examples

- Phase 1, after T001: T002, T004, T005 and T006 together; T003 alongside.
- Phase 2: T009 (self-test) with T010 (`net_open`).
- After US1: US2 (error mapping), US3 (trust option cases) and US4 (recording) in three streams; they
  touch different code (`tls.c` mapping; cases plus `tls.c` anchors; `replay.c`).
- Polish: T026, T027, T028, T029, T030 and T031 together.

## Implementation Strategy

- **MVP**: Phases 1-3. Programs can call HTTPS APIs, verified against the machine's trust store, and
  local tests run with `--ca-file`.
- **Next**: US2. Every failure says what is wrong; no data from unverified servers.
- **Then**: US3 and US4 (trust option scenarios; recording with the scheme).
- **Last**: docs, interop, fuzz, speed, and the full gate on three platforms.
