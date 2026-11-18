# Research: HTTPS

Decisions for spec 019, each with the rationale and the alternatives rejected. Code references are to
the tree at `814521c` (after spec 018). The library was inspected from the official release tarball
(`mbedtls-4.1.2.tar.bz2`).

## R1. Which library, which version

**Decision**: **Mbed TLS 4.1.2**, from the 4.1 long-term-support branch, which is supported with bug
and security fixes until at least March 2029. The release tarball includes TF-PSA-Crypto 1.1, which
holds the cryptography, as a subtree. It is vendored under `third_party/mbedtls/`:
- **What**: only the files the configuration needs (R4); tests, programs, build systems and the
  network module are left out.
- **Unmodified**: the files are kept byte-for-byte as released. vo's code adapts to them, never the
  other way.
- **Provenance**: `third_party/mbedtls/VENDOR.md` records the version, the tarball's SHA-256, the
  list of kept files and how to update (R13).
- **License**: Apache-2.0 (the library is dual Apache-2.0 / GPL-2.0-or-later), with its `LICENSE`
  file kept next to the code.

**Rationale**:
- It supports TLS 1.3 and TLS 1.2 clients and X.509 chain verification with IP and wildcard names.
- It is written in portable C with documented hooks for memory, entropy and time, and is built for
  bare-metal targets, which is how vo is built.
- It is maintained, with published security advisories.
- 4.1 is the current LTS branch, while 3.6 reaches its end of life in March 2027.
- 4.1.2 is the newest patch of that branch and includes its latest security fixes.

**Alternatives**:
- *BearSSL*: small and constant-time, but TLS 1.2 only and without recent releases.
- *wolfSSL*: GPL or a commercial license.
- *s2n-tls, rustls*: need another library or language.
- *Mbed TLS 3.6*: its support ends in 16 months.
- *The system's OpenSSL*: rejected in clarification; dynamic linking breaks the one-binary rule.

## R2. Building it without a C library

The library already supports running without an operating system. vo provides four things through its
documented hooks:

| Need | Hook | vo provides |
|---|---|---|
| memory (`calloc`/`free`) | `MBEDTLS_PLATFORM_MEMORY` + `MBEDTLS_MEMORY_BUFFER_ALLOC_C` | the library's own allocator over one mapped region of 16 MiB, reserved lazily, separate from vo's collected heap |
| entropy | `MBEDTLS_PSA_DRIVER_GET_ENTROPY` | `mbedtls_platform_get_entropy()` over the `getrandom` system call (`SYS_getrandom` exists on all three platforms) |
| wall-clock time (certificate validity) | `MBEDTLS_PLATFORM_TIME_ALT`, `MBEDTLS_PLATFORM_GMTIME_R_ALT` | `clock_gettime(CLOCK_REALTIME)` (as `now_ms` uses) and a civil-date conversion |
| the few C library functions it calls | a shim header directory `third_party/shim/` (`string.h`, `stdlib.h`, `stdio.h`, `time.h`) | the declarations only. `memcpy`, `memset` and friends already exist in `src/vo_c.c`; the rest (expected: `memcmp`, `memmove`, `strlen`, `strncmp`, `strchr`, a small `snprintf`) go into `src/tls_libc.c`, the exact list taken from the link errors |

Further settings:
- **Off**: printing and the debug, error-string and file-system modules (`MBEDTLS_FS_IO`,
  `MBEDTLS_DEBUG_C`, `MBEDTLS_ERROR_C`, the PSA storage on files), and the network module
  `net_sockets.c` (vo has its own sockets, R3).
- **No assembly**: `MBEDTLS_HAVE_ASM` and the AES-NI, ARM and RISC-V hardware paths are off, so the
  same C code runs everywhere. The constitution allows assembly only in the platform layer, and
  results must be identical on all three platforms.

**Rationale**: these are the library's own extension points, so nothing in the vendored files changes.
TLS memory lives outside vo's collected heap: the collector never sees TLS objects, and a connection's
memory is freed explicitly when it closes.

**Ceiling** (`ponytail:`): 16 MiB of TLS memory per process, enough for the trust store (about 1 MiB
parsed) and a connection (well under 1 MiB). It grows if a deep chain or a larger trust store needs
it.

## R3. Plugging TLS into `std.http`

**Decision**:
- **A new primitive**: `__https_exchange(host, port, request)` (builtin id 65, `uses net`, std only).
  It has the same signature and result as `__http_exchange`.
- **`src/net.c`**: the resolve-and-connect part of `rt_http_exchange` becomes
  `net_open(host, port, &fd)`, shared by both primitives.
- **`src/tls.c`**:
  - performs the handshake over that file descriptor, through send and receive callbacks
    (`mbedtls_ssl_set_bio`) that wait with `net_wait` under the `--timeout` deadline, as plain HTTP
    does;
  - writes the request only after the handshake and verification succeed;
  - reads the response up to the same 16 MiB limit;
  - sends `close_notify` and frees the connection.
- **`std/http.vo`**:
  - `parse_url` accepts `https://` (any case), with a default port of 443, and keeps the scheme in
    `Url`;
  - `exchange` calls `__https_exchange` for HTTPS, so `get` and `post` keep their signatures.
- **`io_https_exchange`** (`src/replay.c`) wraps the call like `io_http_exchange`: the host check
  (R0010), then record or replay (R10).

**Rationale**: one shared connect path, the same framing (`http_frame`) for both schemes, and no
change to the vo API: only the URL changes (SC-001).

**Alternatives**: a fourth argument to `__http_exchange` (builtins take three); a `scheme` encoded in
the port (obscure); a separate `std.https` module (a second API for the same thing).

## R4. Protocol versions, algorithms, verification

**Decision**:
- **Protocol versions**: TLS 1.3 and TLS 1.2 (clarified). A minimum version of 1.2 is set; SSL 3.0,
  TLS 1.0 and 1.1 are compiled out.
- **Cipher suites**: TLS 1.3: AES-128-GCM-SHA256, AES-256-GCM-SHA384, CHACHA20-POLY1305-SHA256.
  TLS 1.2: ECDHE with ECDSA or RSA and AES-GCM or ChaCha20-Poly1305 only, so every suite is
  forward-secret and authenticated. No CBC, no RSA key exchange, no static DH.
- **Key exchange groups**: x25519, secp256r1, secp384r1.
- **Signatures**: ECDSA P-256 and P-384, RSA-PSS and RSA PKCS#1 v1.5 with SHA-256, -384 and -512;
  no SHA-1 or MD5.
- **Verification**: `MBEDTLS_SSL_VERIFY_REQUIRED` always; no code path sets anything else. The host is
  set with `mbedtls_ssl_set_hostname`, which gives SNI and name verification, including IP addresses
  against the certificate's address entries. Validity is checked against the machine's clock (R2).
- **ALPN**: `http/1.1`. No session tickets, resumption or early data (spec assumptions), which also
  avoids the 2026 resumption advisories.

**Rationale**: FR-004 to FR-007. Compiling out what is not accepted makes "weaker connection" impossible
by construction, and shrinks the code.

## R5. The trust store

**Decision**:
- **Default authorities**: the machine's trust store, read on the first HTTPS request of a process
  from the first file that exists of:
  - `/etc/ssl/certs/ca-certificates.crt` (Debian, Ubuntu, Arch, Alpine with ca-certificates);
  - `/etc/pki/tls/certs/ca-bundle.crt` (Fedora, RHEL);
  - `/etc/ssl/cert.pem` (Alpine, others).
- **Parsing**: once, into TLS memory, kept for the process. Certificates in the bundle that fail to
  parse are skipped, as other clients do.
- **No store found**: every HTTPS request is
  `err("no trusted certificate authorities found (looked in /etc/ssl/certs/ca-certificates.crt, ...)")`.
- **Tasks**: a forked task inherits the parsed store.

**Rationale**: FR-008. Lazy loading means programs that never use HTTPS pay nothing; loading once keeps
later requests fast (SC-004).

## R6. `--ca-file FILE` (clarified)

**Decision**:
- **Commands**: `vo run` and `vo test` take `--ca-file FILE`, repeatable. Each file is PEM with one or
  more certificates, and they are added to the trust store.
- **When read**: the files are read and parsed at startup, before the program runs, not lazily, so a
  typo fails at once.
  - An unreadable file is **T0002** `cannot_read_input` (exit 66).
  - A file with no valid certificate is a new **T0013** `invalid_certificate_file` (exit 64), naming
    the file.
- **Elsewhere**: the option is T0001 with every other command. `vo replay` needs no trust store
  because it never connects.
- **Not recorded**: the option is not part of a recording (it is not a program input).

**Rationale**: trust is a property of the run, decided by whoever runs it, like curl's `--cacert`.
Failing at startup turns a silent "not trusted" during the run into an immediate, precise error.

**Alternatives**: an environment variable (invisible in a command line someone reviews; the spec
forbids any environment variable that changes verification); a project file (rejected in
clarification).

## R7. Error messages

**Decision**: each failure is an `err` value from `get`/`post` with a fixed message (HOST:PORT as in
existing network errors):

| Kind | Message |
|---|---|
| not trusted (unknown authority, missing intermediate) | `the certificate of HOST:PORT is not trusted` |
| self-signed | `the certificate of HOST:PORT is self-signed and not trusted` |
| wrong host | `the certificate of HOST:PORT is for another host` |
| expired | `the certificate of HOST:PORT has expired` |
| not yet valid | `the certificate of HOST:PORT is not valid yet` |
| no acceptable version or algorithm | `HOST:PORT offers no TLS version or algorithm that vo accepts` |
| tampered (bad record authentication) | `the connection to HOST:PORT was tampered with` |
| any other handshake failure | `the TLS handshake with HOST:PORT failed` |
| no trust store | `no trusted certificate authorities found (looked in ...)` |

The library's verification flags and error codes map to these kinds, in that order of precedence: when
several flags are set, the first row that applies wins, so the report is deterministic.
- **Self-signed**: a one-certificate chain whose issuer equals its subject.
- **Unexpected close**: a server that closes without `close_notify` ends the response, as many HTTP
  servers do. Truncation is then caught by the existing HTTP framing (a body shorter than its
  `Content-Length`, or a chunked body without its last chunk), which is already an `err`.
- **Resolving, connecting, timeouts and the 16 MiB limit**: unchanged messages.

**Rationale**: FR-005. Messages say what to fix (`--ca-file` for a private authority, the server's
certificate otherwise) and never echo library internals.

## R8. Hostile servers

**Decision**: the library's own limits bound record and handshake sizes: `MBEDTLS_SSL_IN_CONTENT_LEN`
at 16 KiB, the TLS maximum, and the maximum chain length at 8. Every call returns within the deadline
because all waits go through `net_wait` with the `--timeout` deadline. The response limit stays 16 MiB.

A fuzz script (`tests/tls_fuzz.py`) runs 1,000 local servers that send mutated or truncated handshake
messages, deterministically seeded. Each run must end with an `err` or a valid response, within the
time limit, without a crash or R0006 (SC-003).

## R9. Determinism

The handshake uses randomness and the real clock, but neither reaches the program. It gets the same
`Response` or the same `err` for the same server behavior. Byte-identical results across platforms
follow from:
- the C-only build (no assembly, `-ffp-contract=off`; the library has no floating point in TLS);
- the same configuration on all three platforms;
- the self-tests (R11).

`--clock-ms` fixes the program's clock, not certificate validation, and the reference says so.

## R10. Recording

**Decision**:
- **Event**: an HTTPS request is recorded as the existing `http` event with a new field
  `"scheme":"https"`, within recording schema 1. Older recordings have no field and mean `http`.
- **Replay**: it compares the scheme with host, port and request line; a different scheme is R0014
  (US4 scenario 4). Replay never connects and never loads a trust store.
- **No secrets**: nothing about the connection (keys, certificates, versions) is recorded; only the
  request line and the response, as for HTTP.

## R11. Testing the cryptography (FR-013, SC-002)

- **Known-answer self-tests**:
  - **What**: the library's self-tests (`MBEDTLS_SELF_TEST`), with NIST and RFC vectors for AES,
    GCM, ChaCha20, Poly1305, SHA-256/384/512, HMAC and HKDF, ECP, ECDSA and RSA.
  - **How**: `tests/tls/selftest.c` is linked against the vendored objects and vo's shim.
    `make tls-selftest` builds it and runs it natively and under qemu for ARM64 and RISC-V, and its
    output must be identical on all three.
  - **Where**: in `make test` (it takes a few seconds).
- **Interoperability**: `tests/https_server.py` serves fixed responses over HTTPS with test
  certificates, through two independent implementations:
  - Python's `ssl` (OpenSSL 3), for TLS 1.3 and 1.2 with each accepted suite;
  - `tlslite-ng` (a pure-Python TLS 1.3/1.2 implementation), installed into `build/py/` by
    `make tls-deps` from a pinned version and SHA-256.

  Without `tlslite-ng` its cases report "skipped" in `make test`, but `make gate` requires them.
- **Test certificates**: in `tests/tls/certs/`, made by `tests/tls/make_certs.py` with the openssl
  command and committed:
  - a test authority, an intermediate and server certificates for `a.test` and `127.0.0.1`
    (valid for 100 years);
  - one expired and one not yet valid, with fixed dates;
  - one for another name and one self-signed;
  - one signed by a second, untrusted authority.
- **Tamper proxy**: `tests/https_server.py --tamper` flips one byte of the first application record.

## R12. Constitution amendment (1.3.0 to 1.4.0)

The constitution's rules for vo's C code cannot hold for code we did not write. This is MINOR: a new
section that scopes existing rules; no principle changes. Its text is in
`contracts/constitution-amendment.md`, applied by the first task:
- **Location and pinning**: vendored code lives only under `third_party/`, pinned to a released version
  whose checksum is recorded, unmodified. Any patch is a file in `third_party/<name>/patches/` with its
  reason.
- **Build rules that still apply**: freestanding, no system libraries, `-ffp-contract=off`, no
  assembly, and the instrumented build must pass.
- **Rules that do not**: vo's strict warning set, without `-Werror`. Vendored code compiles with the
  upstream's own warning level, and any new warning on update is reviewed.
- **Updates**: a security release of a vendored library is applied within the spec cycle in which it
  appears.
- **License**: compatible (Apache-2.0, MIT, BSD), recorded in `VENDOR.md`.

## R13. Size and updating

**Size**:
- About 40 C files of the library are compiled, mainly TLS client, X.509, ASN.1, PEM, bignum, ECP,
  RSA, SHA, AES, GCM, ChaCha20-Poly1305, HKDF and the PSA core. That is roughly 60,000 lines, against
  124,590 in the full tarball.
- Expected binary growth: about 400 KiB (measured during implementation, recorded in the results).

**Updating**: `tools/vendor_mbedtls.sh VERSION SHA256` downloads the tarball, checks the checksum,
extracts the listed files into `third_party/mbedtls/` and rewrites `VENDOR.md`. An update is a normal
change: the diff of the vendored files, then the full gate.

## R14. Reference and documentation

**Reference**, about 150 tokens (limit 200):
- the `std.http` row becomes "HTTP and HTTPS";
- one bullet covering: certificates always verified against the machine's store plus `--ca-file`; TLS
  1.3 and 1.2; the `err` messages are in `vo doc std.http`; `--clock-ms` does not change certificate
  validation;
- `[--ca-file FILE]` on the `run` and `test` command lines.

**`docs/security.md`**:
- the out-of-scope line and the "Plain HTTP and DNS" known limit are replaced: HTTPS protects the
  connection's confidentiality and integrity and the server's identity;
- what it still does not cover: DNS, plain `http://` URLs, server compromise, revocation (no OCSP or
  CRL), side channels;
- vendored code joins the trust boundary: the binary now includes third-party code.

**Library docs**: `std/http.vo` lists the error messages in its documentation comments (`vo doc`).
