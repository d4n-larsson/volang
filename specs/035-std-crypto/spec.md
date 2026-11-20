# Feature Specification: Hashing and Secrets (`std.crypto`)

**Feature Branch**: `035-std-crypto`

**Created**: 2026-11-20

**Status**: Implemented (gate not run)

**Input**: User description: "`std.crypto`: SHA-256, HMAC, constant-time compare, a password hash, and
random bytes from the system under their own effect. `src/sha256.c` and Mbed TLS are in the binary;
nothing is exported. `random_int` is a seeded generator, and `std/web.vo` builds request IDs from it;
nothing in vo can make a session token safely today. Source: docs/review-2026-11/ecosystem.md, Standard
library, 'Hashing and secrets'. Must keep: determinism (tests reproducible, record and replay), effects
checked and enforced, one way to do each thing, no libc, byte-identical results on three architectures,
the reference under 15,000 tokens (about 14,706 now)."

Background: a vo program can serve an API, keep users in SQLite and call other services over HTTPS, but
it cannot do the three small things every such program needs. It cannot make a value nobody can guess:
`random_int` is a seeded generator, right for simulations and wrong for a session token, since anyone
who learns the seed knows every token. It cannot store a password: there is no slow hash, so a program
either stores the password or invents a fast hash, both of which fail the day the database leaks. And it
cannot check a signature: webhooks from payment and source-hosting services are signed with HMAC-SHA256,
and an API token should be compared without revealing how many leading bytes were right. The hash
function is already inside the toolchain; none of it is offered to programs.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Values nobody can guess (Priority: P1)

A developer makes a session token, an API key or a password-reset code from the system's random source:
`to_hex(random_bytes(16))`. The function has its own effect, so a reviewer sees in `vo.effects` which
functions draw secrets, and a function that only declares `random` cannot.

**Why this priority**: it is the gap with no workaround; everything else can be approximated badly, a
secret cannot.

**Independent Test**: a program prints `len(random_bytes(16))` and two tokens; the tokens differ from
each other and from run to run; a function that calls `random_bytes` without declaring the effect is
the usual effect error with its fix.

**Acceptance Scenarios**:

1. **Given** `random_bytes(n)` with `n` from 0 to 65536, **When** called, **Then** it returns `n` bytes
   from the system's random source; two calls give different values; outside that range it is a
   runtime error.
2. **Given** a function that calls `random_bytes`, **When** checked, **Then** it must declare the new
   effect (E0501 with the fix otherwise), the effect appears in `vo.effects` and `vo effects`, and a
   built executable's `--vo-effects` shows it.
3. **Given** `vo run --seed N`, **When** `random_bytes` is called, **Then** the seed does not affect it:
   no command-line option makes a program's secrets predictable.
4. **Given** `vo test`, **When** a test calls `random_bytes`, **Then** the bytes are the same on every
   run of that test (derived from the test's seed), so tests stay reproducible; this is documented as
   the one place the bytes are not secret.
5. **Given** a recorded run, **When** replayed, **Then** `random_bytes` returns the recorded bytes and
   the output is the same; the documentation says a recording holds them, like the files and variables
   it already holds.

---

### User Story 2 - Passwords stored safely (Priority: P1)

A developer stores `hash_password(password)` and later calls `verify_password(password, stored)`. The
stored text carries the method, its cost and a fresh salt, so the developer chooses nothing and cannot
choose wrong.

**Why this priority**: the second gap with no safe workaround, and the one where a home-made answer
does the most harm.

**Independent Test**: `verify_password("pw", hash_password("pw"))` is true and with another password
false; two hashes of the same password differ; a published test vector for the method verifies.

**Acceptance Scenarios**:

1. **Given** a password, **When** hashed twice, **Then** the two texts differ (fresh salt) and both
   verify; a different password does not.
2. **Given** a stored text, **When** read, **Then** it names the method and its cost, so hashes made
   today verify after a later version raises the cost.
3. **Given** a stored text that is not a password hash (empty, cut short, another method), **When**
   verified, **Then** the answer is false, never a runtime error.
4. **Given** a published vector of the method (password, salt, cost, expected key), **When** checked,
   **Then** it verifies on all three architectures.
5. **Given** one hash or verification, **When** timed, **Then** it takes long enough to slow guessing
   (tens of milliseconds at least) and short enough for a login (under a second).

---

### User Story 3 - Hashes, signatures and safe comparison (Priority: P2)

A developer hashes data with SHA-256, signs or checks a message with HMAC-SHA256, and compares a
received token or signature with the expected one in constant time.

**Why this priority**: needed for webhooks, signed cookies, cache keys and checksums; the fast hash
alone is not a security feature but everything else builds on it.

**Independent Test**: the standard vectors for SHA-256 (empty input, "abc", a long input) and
HMAC-SHA256 (RFC 4231) give the published digests; a webhook example checks a signature and rejects a
changed body.

**Acceptance Scenarios**:

1. **Given** `sha256(b)`, **When** called on the standard vectors, **Then** it returns the published 32
   bytes; it is pure (no effect) and works on any length, including empty and multi-megabyte input.
2. **Given** `hmac_sha256(key, message)`, **When** called on the RFC 4231 vectors (short keys, keys
   longer than a block), **Then** it returns the published 32 bytes.
3. **Given** two `bytes` values, **When** compared with the constant-time comparison, **Then** the
   answer equals `==`, and the time taken does not depend on where they first differ.
4. **Given** text, **When** hashed, **Then** the developer converts it explicitly with
   `std.bytes.from_text`, as everywhere else: there are no `str` variants.

---

### User Story 4 - Documented where agents read (Priority: P3)

The reference lists `std.crypto` and the new effect; `vo doc std.crypto` shows each function with a
runnable example; the guide shows a session token, a stored password and a webhook check.

**Why this priority**: an agent that does not find these writes its own, which is the failure this
feature exists to prevent.

**Independent Test**: the reference's module table and effects table include them and stay under
15,000 tokens; every documented example runs.

**Acceptance Scenarios**:

1. **Given** the reference, **When** read, **Then** `std.crypto` is in the module table and the effect in
   the effects table.
2. **Given** `vo doc std.crypto`, **When** read, **Then** every function has an example that runs, and
   `random_int`'s documentation says it is not for secrets and names `random_bytes`.

---

### Edge Cases

- `random_bytes(0)`: empty bytes; negative or over the limit: a runtime error naming the limit.
- The system's random source is unavailable or fails: the program stops with an error; it never falls
  back to a predictable source.
- `hash_password("")`: allowed (a policy on passwords is the program's); very long passwords are hashed
  whole.
- `verify_password` with a stored text whose cost is absurdly large (a hostile database row): refused
  (false) above a documented maximum, so verification cannot be made to run for hours.
- A password or key with any bytes (not UTF-8 problems: passwords are `str`, keys and messages `bytes`).
- Hashing inside a task (`concurrent_map`): allowed; `random_bytes` in a task follows the rules other
  effects follow there.
- Constant-time comparison of different lengths: false; the lengths are not secret.
- `--timeout` during a password hash: the time limit applies as to any computation.
- Recording: the bytes of `random_bytes` are in the recording; password hashes replay identically
  because their salt is recorded.

## Requirements *(mandatory)*

### Functional Requirements

**Secrets**

- **FR-001**: `std.crypto` MUST provide `random_bytes(n)` returning `n` bytes (0 to 65536) from the
  operating system's random source, under a new effect of its own that `random` does not cover.
- **FR-002**: No command-line option of `vo run` or of a built executable MUST change what
  `random_bytes` returns; under `vo test` the bytes MUST be reproducible; under replay they MUST be the
  recorded ones.
- **FR-003**: When the system's random source fails, the program MUST stop with an error and MUST NOT
  continue with bytes from anywhere else.

**Passwords**

- **FR-004**: `hash_password(password)` MUST return a text holding the method, its cost, a fresh random
  salt and the derived key, using a standard, published password-hashing method; it has the effect of
  FR-001 (the salt).
- **FR-005**: `verify_password(password, stored)` MUST be pure, MUST accept every text
  `hash_password` has produced (including with other costs up to a documented maximum), and MUST answer
  false for anything else without a runtime error, comparing in constant time.

**Hashes**

- **FR-006**: `sha256(b)` and `hmac_sha256(key, message)` MUST return the standard 32-byte results for
  any input, with no effect.
- **FR-007**: A constant-time equality for `bytes` (`same_bytes`) MUST be provided, equal in result to
  `==`.
- **FR-008**: Every result MUST be byte-identical on the three architectures.

**Tooling and documentation**

- **FR-009**: The new effect MUST work everywhere effects do: declarations, `vo.effects`, `vo effects`,
  E0501/E0504 with fixes, runtime enforcement, traces, recordings, built executables.
- **FR-010**: Every function MUST be documented in `vo doc` with a runnable example; the reference MUST
  list the module and the effect and stay under 15,000 tokens; `random_int`'s documentation MUST point
  to `random_bytes` for secrets.
- **FR-011**: Existing programs MUST be unchanged: nothing that exists gains the new effect.

### Key Entities

- **Secret bytes**: bytes from the system's random source; never derived from the seed outside tests.
- **Stored password**: one line of text: method, cost, salt, key; self-describing.
- **The new effect**: the permission to draw secret bytes; visible in every effects report.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The published vectors for the hash, the keyed hash and the password method all pass, on
  the three architectures.
- **SC-002**: 1,000 tokens of 16 bytes from one run are all different, and two runs share none.
- **SC-003**: One password hash takes between 50 ms and 1 s on this machine.
- **SC-004**: A program using every function records and replays to identical output, and its tests
  give identical output on every run.
- **SC-005**: Every existing test case and example is unchanged.
- **SC-006**: The reference stays under 15,000 tokens, and every documented example runs.

## Assumptions

- The effect is named `entropy`: it is what the program takes from the machine, and it cannot be
  confused with the seeded `random`.
- The password method is PBKDF2 with HMAC-SHA256 at 600,000 iterations (the current OWASP figure for
  it), a 16-byte salt and a 32-byte key, stored as `pbkdf2-sha256$600000$SALT$KEY` with base64 parts.
  It needs only the hash already in the toolchain. Memory-hard methods (Argon2, scrypt) are left for a
  later version; the stored text names its method so they can be added without breaking old hashes.
- Only SHA-256: SHA-1 and MD5 are left out on purpose (a program that must talk to a system needing
  them is a later, explicit addition); SHA-512 is not needed by anything seen.
- No encryption, signatures with public keys, or key exchange in this spec: each is a larger design.
- `std.web`'s request IDs keep using `random`: they are labels, not secrets. Session handling in
  `std.web` is not part of this spec.
- Hex and base64 of results use `std.bytes`; `std.crypto` adds no encodings.
