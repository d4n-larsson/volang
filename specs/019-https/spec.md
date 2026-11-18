# Feature Specification: HTTPS

**Feature Branch**: `019-https`

**Created**: 2026-11-18

**Status**: Implemented

**Input**: User description: "https" (the first gap of the roadmap's "Planned after 017" table: `std.http`
speaks plain HTTP only, so most public APIs are out of reach; spec 007 follow-up).

Background: `std.http.get` and `post` talk plain HTTP, and `get("https://...")` is
`err("https is not supported")`. Almost every public API, package host and web service today accepts
only HTTPS, so a vo program cannot call them at all, and `docs/security.md` lists "no TLS" as a known
limit: anyone on the network path can read and change what a program sends and receives. The toolchain
links no system libraries, so HTTPS means vo carries its own implementation of the secure connection.

What this spec must keep:
- **The `net` effect as a security boundary**: a host outside the grant is refused before any
  connection.
- **Determinism**: record and replay reproduce a run without the network.
- **Byte-identical behavior** on x86-64, ARM64 and RISC-V.
- **The reference budget.**

It must also not weaken anything: a program must never silently talk to a server it cannot verify.

## Clarifications

### Session 2026-11-18

- Q: How are extra trusted authorities added? → A: An option of `vo run` and `vo test` naming a
  certificate file, chosen by whoever runs the program. Trust is a property of where a program runs,
  not of the program.
- Q: Which protocol versions, and where does the TLS implementation come from? → A: An established,
  maintained TLS library is vendored: its source is compiled into vo, so vo stays one static binary
  with no runtime dependency, and the cryptography is widely reviewed rather than written new. Linking
  the system's library was rejected, because the constitution requires a binary that links nothing, as
  was running an outside program. With a vendored library, TLS 1.3 and TLS 1.2 (forward-secret,
  authenticated ciphers only) are both accepted, so nearly every server can be reached.
- Q: Is serving over HTTPS in scope? → A: No. `std.http.serve` stays plain HTTP in this spec, and
  serving over HTTPS is recorded on the roadmap as its own spec.

## User Scenarios & Testing *(mandatory)*

The users are the same as in specs 001–018: AI coding agents that write vo programs from the reference
and `vo doc`, and the people who review and run those programs.

### User Story 1 - Call an HTTPS API (Priority: P1)

An agent writes a program that fetches JSON from a public HTTPS API (a weather service, a package
index, a status page), parses it and prints a summary. It writes the same `get` call it would for
plain HTTP, with an `https://` URL, and declares `net` with the API's host.

**Why this priority**: this is the gap. Without it, programs that use the network can only talk to
local or legacy plain-HTTP services.

**Independent Test**: step cases against a local HTTPS test server whose certificate is signed by a
test authority the case trusts: `get` and `post` return the same `Response` values as over plain HTTP.

**Acceptance Scenarios**:

1. **Given** a program declared `uses net("api.example.com")`, **When** it calls
   `get("https://api.example.com/v1/items")`, **Then** it gets a `Response` with the status, headers
   and body, exactly as `get` returns for `http://`.
2. **Given** the same program, **When** it calls `post` to an `https://` URL with a JSON body,
   **Then** the server receives the body and the program gets the response.
3. **Given** an `https://` URL with an explicit port (`https://host:8443/`), **When** the program
   calls `get`, **Then** it connects to that port; without a port it uses 443.
4. **Given** a program declared `uses net("api.example.com")`, **When** it calls
   `get("https://other.example.com/")`, **Then** it stops with R0010 before any connection, as for
   `http://` today. A literal URL outside the scope is E0503 at check time.
5. **Given** a response larger than the existing response limit, or a server that stops answering,
   **When** the program calls `get`, **Then** the result is the same `err` or timeout behavior as for
   plain HTTP.

---

### User Story 2 - Never talk to a server that cannot be verified (Priority: P1)

The program connects only when the server proves it is the host in the URL, with a certificate
from an authority the machine trusts. A server with an expired certificate, a certificate for another
name, a self-signed certificate, or a connection someone tampers with gives an `err` with a stable
message that says what was wrong. The program gets no data from that server. There is no switch that
turns verification off.

**Why this priority**: HTTPS without verification only looks secure. An agent that hits a certificate
error will look for a way around it; if there is none in the language, nobody can ship a program
that trusts anyone.

**Independent Test**: step cases with local test servers presenting each kind of bad certificate, and
a tampering proxy. Each gives its own `err` message, and none delivers a response.

**Acceptance Scenarios**:

1. **Given** a server whose certificate is valid for `a.test` and trusted, **When** the program gets
   `https://a.test/`, **Then** it succeeds.
2. **Given** a server whose certificate names another host, **When** the program connects, **Then**
   the result is `err` with a stable message naming the mismatch, and no request is sent.
3. **Given** a certificate that has expired or is not yet valid, a self-signed certificate, or one
   signed by an authority that is not trusted, **When** the program connects, **Then** each is an `err`
   with its own stable message.
4. **Given** a connection where bytes are changed in transit after the handshake, **When** the program
   reads the response, **Then** the result is an `err`, never altered data.
5. **Given** a server that offers only protocol versions or algorithms that vo does not accept as
   secure, **When** the program connects, **Then** the result is an `err` naming the reason, not a
   weaker connection.
6. **Given** any vo program, **Then** there is no option, environment variable or API that skips
   verification.

---

### User Story 3 - Trust a private authority on purpose (Priority: P2)

A company runs internal services with certificates from its own authority, and a test suite runs
against a local HTTPS server with a test authority. The person running the program states which extra
authorities to trust, visibly, and nothing else becomes trusted.

**Why this priority**: without it, internal services and local HTTPS tests are impossible, and people
would ask for the "skip verification" switch that User Story 2 refuses.

**Independent Test**: a step case where a program fails against a server signed by a test authority,
then succeeds when the authority is added in the way this story provides, and still fails against a
server signed by a different untrusted authority.

**Acceptance Scenarios**:

1. **Given** a server signed by a private authority, **When** the program runs without adding that
   authority, **Then** the connection fails as untrusted (User Story 2).
2. **Given** the authority added with the `vo run` / `vo test` option that names a certificate file
   (clarified 2026-11-18), **When** the program connects, **Then** it succeeds; a server signed by any
   other private authority still fails.
3. **Given** a recording made with an extra authority, **When** it is replayed, **Then** the replay
   needs neither the authority nor the network.

---

### User Story 4 - Replay and test HTTPS runs offline (Priority: P2)

An agent records a run that calls an HTTPS API and replays it later without the network, exactly as
it does for plain HTTP today. A reviewer reads in the recording what was requested and what came back.

**Why this priority**: determinism is a core promise (Principle III). Recordings are how an agent
reproduces a failure without asking the API again.

**Independent Test**: a step case records a run against a local HTTPS server, stops the server, and
replays with identical output, on all three platforms.

**Acceptance Scenarios**:

1. **Given** a run recorded with `--record` that made HTTPS requests, **When** it is replayed, **Then**
   the output and exit status are identical and no connection is made.
2. **Given** a recording, **When** a reviewer reads it, **Then** each HTTPS request and response is
   there in readable form, marked as HTTPS, like a plain HTTP one; no keys or secrets of the connection
   are stored.
3. **Given** a program that now asks for a different HTTPS URL than the recording, **When** it is
   replayed, **Then** it stops with R0014 as today.
4. **Given** a recording of a plain-HTTP request, **When** the program now asks for the same URL over
   HTTPS, **Then** replay reports a mismatch: the scheme is part of what must match.

---

### Edge Cases

- A URL with uppercase `HTTPS://`, a trailing dot in the host, an IPv4 address instead of a name
  (verified against the certificate's address entries, or refused when it has none), or user and
  password parts (refused as today).
- A server that sends its certificate chain out of order, an incomplete chain, or extra certificates:
  chains are built from what the server sent plus the trusted authorities. A missing intermediate is
  "certificate not trusted", not a crash.
- A certificate with a wildcard name (`*.example.com`): it matches one label only (`a.example.com`,
  not `a.b.example.com` and not `example.com`).
- A response that ends without a proper close (a truncation attack): an `err`, not a shortened body
  that looks complete.
- A server that asks the client for a certificate: the connection proceeds without one or fails with a
  stable message. Client certificates are out of scope.
- Redirects from `http://` to `https://`: returned to the program as today (redirects are not
  followed), so the program sees the `Location` and decides.
- A very slow handshake: covered by the existing time limits (`--timeout` and the per-request limit).
- Clock skew: certificate validity is checked against the machine's clock. That time is not a
  program input and is not recorded; replay does not connect, so it does not check certificates.
- Hostile servers (A2 in the threat model): malformed handshake messages, huge certificates, deep
  chains and oversized records give `err` results within fixed limits, never a crash, a hang or
  unbounded memory.
- The trust store missing or unreadable on the machine: every HTTPS request is an `err` saying no
  trusted authorities were found.

## Requirements *(mandatory)*

### Functional Requirements

**Client**

- **FR-001**: `std.http.get` and `post` MUST accept `https://` URLs and return the same `Response`
  values as for `http://`, with port 443 by default and an explicit port when given.
- **FR-002**: The `net` effect and its host scopes MUST apply to HTTPS exactly as to HTTP. A scope
  names a host, not a scheme, so `net("api.example.com")` covers both. Hosts outside them are R0010,
  checked at run time before any connection (amended in implementation: plain HTTP never had a
  check-time check of literal URLs, and HTTPS keeps the same rule).
- **FR-003**: Existing limits MUST apply unchanged: response size, time limits and the
  one-request-per-connection behavior.

**Verification**

- **FR-004**: A connection MUST be used only after the server's certificate chain is verified:
  - it leads to a trusted authority;
  - every certificate is within its validity period by the machine's clock;
  - it is valid for the URL's host (names and wildcards by the usual rules, IP addresses against
    address entries).
- **FR-005**: Each failure MUST be an `err` (not a runtime error) with a stable, documented message per
  kind: not trusted, wrong host, expired or not yet valid, protocol or algorithm not acceptable,
  handshake failed, connection tampered with or truncated. No request bytes are sent before
  verification succeeds.
- **FR-006**: There MUST be no way to disable or weaken verification: no option, no environment
  variable, no function.
- **FR-007**: Only TLS 1.3, and TLS 1.2 with forward-secret authenticated ciphers, MUST be accepted
  (clarified 2026-11-18). A server offering only older versions or weaker algorithms gets an `err`.
- **FR-008**: The trusted authorities MUST by default be the machine's own trust store, at its usual
  location on the supported Linux systems.
- **FR-009**: Extra authorities MUST be trusted only through an option of `vo run` and `vo test` that
  names a certificate file (one or more authorities), given by whoever runs the program (clarified
  2026-11-18). A program cannot add authorities itself, and `vo replay` needs none.

**Record and replay**

- **FR-010**: `--record` MUST save HTTPS requests and responses like plain HTTP ones, with the scheme,
  and never connection keys or other secrets of the connection. `vo replay` MUST replay them without the
  network or the trust store. The scheme is part of what a replayed request must match (R0014).

**Robustness and determinism**

- **FR-011**: Malformed or hostile input from a server MUST give an `err` within fixed limits on sizes,
  lengths and counts. It MUST never crash, hang beyond the time limits, or use unbounded memory.
- **FR-012**: The same server responses MUST give byte-identical program results on x86-64, ARM64 and
  RISC-V. Every cryptographic operation MUST give identical results on all three platforms.
- **FR-013**: The cryptographic operations MUST pass published test vectors for every algorithm used,
  and handshakes MUST interoperate with at least two independent widely used server implementations
  (checked locally, with no internet access needed).

**Documentation**

- **FR-014**: The reference MUST describe HTTPS in at most 200 more tokens. `docs/security.md` MUST
  update its boundary and known limits: what HTTPS now protects, and what it does not (DNS, server
  compromise, side channels). Each new `err` message MUST be listed in the library documentation.

### Key Entities

- **Secure connection**: a connection to one host and port whose server has been verified; carries one
  request and its response, then closes.
- **Trust anchors**: the certificate authorities whose signatures make a server trusted: the machine's
  trust store, plus any extra authorities named by the run's certificate-file option.
- **Verification failure**: one of a fixed set of kinds, each with a stable message, returned as an
  `err`.
- **HTTPS event in a recording**: a request and response like a plain HTTP one, marked with its
  scheme.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A program that calls an HTTPS API needs no more lines than the same program over plain
  HTTP: only the URL's scheme differs. Checked by running the existing HTTP examples and step cases
  with `https://` URLs against local test servers.
- **SC-002**: Every algorithm passes its published test vectors (100%). Handshakes succeed against at
  least two independent widely used server implementations, configured with each accepted version and
  algorithm.
- **SC-003**: Every verification failure kind is triggered by a step case and gives its documented
  message: untrusted, wrong host, expired, not yet valid, self-signed, missing intermediate, weak
  protocol, tampered and truncated. In 1,000 fuzzed malformed server handshakes, none crashes, hangs
  or exceeds the memory limit.
- **SC-004**: A handshake and a small request to a local server complete in under 100 ms; a 10 MiB
  response over HTTPS completes in under 1 s on loopback, whatever suite the server picks (amended in
  implementation: vo's TLS uses no AES instructions, so half of loopback HTTP speed is out of reach).
- **SC-005**: A run recorded against a local HTTPS server replays with identical output and exit
  status, with the server stopped, 20 times in a row, and identically on all three platforms.
- **SC-006**: The reference grows by at most 200 tokens. Every existing golden and step case passes
  unchanged, except the one documented `https is not supported` result. The full gate passes on
  x86-64, and `make test` on ARM64 and RISC-V.

## Assumptions

- Client certificates, HTTP/2, session resumption, early data, OCSP, revocation lists and certificate
  pinning are out of scope. Revocation is a known limit for `docs/security.md`.
- DNS stays as it is (IPv4, `/etc/hosts`, one resolver query). HTTPS protects the connection, not the
  name lookup; a spoofed address fails verification instead of silently connecting.
- Timing and other side channels stay out of the threat model, as they are today. Cryptographic code
  is still written to avoid secret-dependent branches where practical.
- Downloading packages (the roadmap follow-up that needs HTTPS) is a separate spec.
- Serving over HTTPS is out of scope (clarified 2026-11-18). `std.http.serve` stays plain HTTP, and a
  public vo server runs behind a proxy that handles HTTPS. It is recorded on the roadmap as its own
  spec.
- Tests never need the internet: every case uses local servers and test authorities.
- The TLS implementation is an established, maintained library whose source is vendored into the
  repository and built into the binary (clarified 2026-11-18). Planning picks the library and its
  version, and records where it came from and how it is updated.
- Third-party source cannot meet the constitution's rules for vo's own C code (the strict warning set,
  and nothing written by others). A constitution amendment that scopes those rules and states the rules
  for vendored code is a dependency of this spec, made in planning.
