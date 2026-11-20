# Research: Hashing and Secrets (`std.crypto`)

## R1. Surface

**Decision**: `std/crypto.vo` exports six functions (contracts/library.md): `sha256`, `hmac_sha256`,
`same_bytes`, `random_bytes`, `hash_password`, `verify_password`, and nothing else. They sit on five
internal builtins (`__sha256`, `__hmac_sha256`, `__pbkdf2_sha256`, `__bytes_same`, `__entropy`), the
way `std.bytes` sits on its own.

**Alternatives considered**: plain builtins (`sha256(b)` without an import): every other library
function is imported, and these are rarely used. A `random_token()` returning text:
`to_hex(random_bytes(16))` says the same and shows the length.

## R2. SHA-256 and HMAC

**Decision**: `src/sha256.c` gets a context struct (`Sha256`: state, buffer, counts) with
`sha256_start/add/end`; today's `sha256_init/update/final` and `sha256` keep their signatures on one
static context, so package hashing and `vo build` are untouched. HMAC-SHA256 (RFC 2104) in
`src/crypto.c`: a key longer than 64 bytes is hashed first; inner and outer pad contexts are computed
once per key.

**Rationale**: HMAC and PBKDF2 need two hashes in flight and saved pad states; a context is the
standard shape and costs nothing for the old callers.

**Alternatives considered**: Mbed TLS's SHA-256 and `md` layer: already vendored, but configured for TLS
with its own allocator region and PSA plumbing; calling into it from the runtime would tie hashing to
the TLS setup.

## R3. Passwords

**Decision**: PBKDF2-HMAC-SHA256 (RFC 8018), 600,000 iterations, 16-byte salt, 32-byte key.

- `__pbkdf2_sha256(password: bytes, salt: bytes, iterations: int) -> bytes` computes one 32-byte block
  with precomputed pads (two compressions per iteration), polling the timeout every 1,024 iterations.
- `hash_password(p)` in vo: `"pbkdf2-sha256$600000$" + base64(salt) + "$" + base64(key)` with
  `salt = random_bytes(16)`; effect `entropy`.
- `verify_password(p, stored)` in vo: splits on `$`; requires four parts, the method name, an
  iteration count from 1 to 10,000,000 (the documented maximum; a hostile row cannot ask for more), a
  salt and a 32-byte key that decode; recomputes and compares with `same_bytes`. Anything else: false.
  Pure.

**Rationale**: a standard method buildable on the hash already here; OWASP's current figure for it. The
text names method and cost, so a later Argon2id can be added and old hashes still verify.

**Alternatives considered**: Argon2id or scrypt now: better against hardware guessing, but each is a
new primitive (BLAKE2b or Salsa20/8) and tens of megabytes per hash against a 512 MiB default limit;
a later spec. bcrypt: needs Blowfish and truncates at 72 bytes.

## R4. Constant-time comparison

**Decision**: `same_bytes(a, b)`: false at once when the lengths differ (lengths are not secret), else
the OR of the XOR of every byte pair, with no early exit, read through a volatile accumulator so the
compiler keeps the loop.

## R5. The effect

**Decision**: a twelfth effect, `entropy`, appended to `es_names` (bit 11; `builtin_effects` is 16 bits
wide). No scopes. `__entropy` carries it; `random_bytes` and `hash_password` declare it. Every table or
loop sized by the number of effects is found by searching for the existing last one (`process`).

**Rationale**: appended, so every existing set, record and recording keeps its bits (FR-011).

## R6. Where the bytes come from

**Decision**:

- `vo run`, built executables: `getrandom(buf, n, 0)`, looped over short reads and `EINTR`; any other
  failure is a runtime error (R7). `--seed` is not consulted.
- `vo test` (and property cases): a generator of its own, seeded from the test's seed (so `--seed`
  changes it there, as it changes `random_int`), separate from `random_int`'s state so drawing secret
  bytes does not shift a test's other random numbers.
- Recording: each call is an event holding the bytes (hex); replay returns them and stops with R0014
  when the program asks for another count. Recordings already hold file contents and variables; the
  documentation of `--record` says they hold these too.
- Tasks (`concurrent_map`): allowed, as other effects are in tasks; each child draws from the system
  (in tests: from a generator seeded by the test's seed and the task's index, as `random_int` is there).

## R7. Errors

**Decision**: no new code. `random_bytes(n)` outside 0 to 65536 is R0007 `invalid_argument` (as
`random_int` with `lo > hi`). A failing `getrandom` is also R0007, with its own message ("The system's
random source failed: `random_bytes` cannot continue."): a structured runtime report that stops the
program (constitution III), never continuing with other bytes. It cannot be produced in a test, so it
gets no code of its own (the catalog requires a golden case per code); R0007's catalog entry gains a
sentence. The effect uses E0501, E0504 and R0010.

## R8. Existing behavior

**Decision**: nothing existing gains `entropy`: `std.web.random_id` stays on `random`, TLS keeps its
own entropy inside `net`. `std.random`'s module comment and `random_int`'s reference row say "not for
secrets: `std.crypto.random_bytes`".

## R9. Documentation

**Decision**: reference: `entropy` row in the effects table, `std.crypto` row in the module table, five
words on `random_int`'s row (+≤ 100 tokens). `vo doc std.crypto` examples run under the doc tests (a
test context, so `random_bytes` examples are reproducible; they show lengths, not values). Guide:
`standard-library.md` gains a section (a session token, a stored password, a webhook signature check);
`apis.md` links to it. Roadmap, README, review index.

## R10. Tests

**Decision**: `tests/cases/crypto/`:

- `sha256` (FIPS vectors: empty, "abc", the 56-byte message, one million `a`), `hmac` (RFC 4231 cases 1,
  2, 3, 4, 6, 7), `pbkdf2` (through `verify_password` on stored texts built from published
  PBKDF2-HMAC-SHA256 vectors: "passwd"/"salt"/1 and "Password"/"NaCl"/80000 from RFC 7914 §11, first 32
  bytes), `same_bytes`, `password` (hash twice: differ, both verify, wrong password false; malformed
  stored texts false; a cost over the maximum false);
- `random_bytes` (`test {}`: lengths, two draws differ, the same output on every run), `random_run`
  (a step case: two runs differ: compared inside one program is not possible, so the case checks only
  length and that `--seed` given twice still differs, with the output hidden and compared by `same`),
  `effect_missing` (E0501 with the fix), `effect_record` (`vo effects`), `bad_count` (R0007),
  `record_replay` (a step case), `tasks`;
- `doc` (`vo doc std.crypto`).

`tests/crypto_vectors.py` (in `make test`): SHA-256 and HMAC of 200 random inputs against Python's
`hashlib`/`hmac`, and 5 PBKDF2 cases at small costs against `hashlib.pbkdf2_hmac`, through one vo
program per batch; SC-002 (1,000 tokens distinct, two runs disjoint); `--time` for SC-003.
