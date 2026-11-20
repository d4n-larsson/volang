# Implementation Plan: Hashing and Secrets (`std.crypto`)

**Branch**: `035-std-crypto` (work on `main`, as for specs 001–034) | **Date**: 2026-11-20 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/035-std-crypto/spec.md`

## Summary

A new library module `std/crypto.vo` over five internal builtins (R1): SHA-256 and HMAC-SHA256 on the
toolchain's own `src/sha256.c` (R2), PBKDF2 for passwords (R3), a constant-time comparison (R4) and
`random_bytes` from `getrandom` under a twelfth effect, `entropy` (R5, R6). `hash_password` and
`verify_password` are written in vo on top of them (R3). Tests draw reproducible bytes; recordings hold
the bytes drawn (R6).

| Part | File | What |
|---|---|---|
| Hashing | `src/sha256.c`, `src/crypto.c` (new) | a context-based SHA-256 core kept behind today's functions; HMAC, PBKDF2, constant-time compare |
| Builtins | `src/vo.h`, `src/check_expr.c`, `src/check_effects.c`, `src/rt_call.c` | `__sha256`, `__hmac_sha256`, `__pbkdf2_sha256`, `__bytes_same`, `__entropy` |
| Effect | `src/effects.c` and every table sized by the number of effects | `entropy` |
| Secret bytes | `src/rt_effects.c`, `src/replay.c` | `getrandom`; reproducible in tests; recorded and replayed |
| Library | `std/crypto.vo` (new), `Makefile` (`STD_FILES`), `src/doc.c` (`doc_modules`) | the six exported functions |
| Docs | `docs/reference.md`, `docs/errors.md` (if a code is added), `site/docs/guide/standard-library.md`, `site/docs/guide/apis.md`, `docs/roadmap.md`, `README.md`, `std/random` docs | R9 |
| Tests | `tests/cases/crypto/`, `tests/crypto_vectors.py` (new) | R10 |

## Technical Context

**Language/Version**: C (C11, freestanding, no libc) for the builtins; vo for the module.

**Primary Dependencies**: none new (Mbed TLS is not used: its hash is behind its own allocator and API;
`src/sha256.c` is 110 lines and already verified by the package hashes).

**Storage**: none.

**Testing**: golden cases with published vectors first; `make quick` between edits, `make test` once per
change; the gate only when asked.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64, byte-identical results.

**Project Type**: compiler toolchain (CLI) with its standard library.

**Performance Goals**: one password hash between 50 ms and 1 s (SC-003): 600,000 iterations at two
compression calls each, with the key pads precomputed.

**Constraints**: no fallback to a predictable source (FR-003); no option changes `random_bytes` outside
tests (FR-002); existing programs unchanged (FR-011); reference ≤ 15,000 tokens (+≤ 100 here).

**Scale/Scope**: about 200 lines of C, 120 of vo, no new error code expected (R7).

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | Seven functions, one per job; `bytes` in and out with no `str` twins; the password functions choose method, cost and salt themselves, so there is nothing to get wrong. | Pass |
| II. Machine-readable feedback | The effect uses the existing diagnostics (E0501, E0504, R0010) with their fixes; bad arguments use the existing runtime code for invalid arguments. | Pass |
| III. Determinism | `entropy` is an input from outside, like `env` and the clock: recorded and replayed; fixed in tests as the seed and clock are. Hashes are pure and identical on every platform. | Pass |
| IV. Test-first | Published vectors (FIPS 180-4, RFC 4231, RFC 7914 §11 for PBKDF2-HMAC-SHA256) as golden cases before the code. | Pass |
| V. Single source of truth, minimal surface | One module; one effect; the reference gains one table row each (+≤ 100 tokens); nothing speculative (no SHA-1, no encryption). | Pass |
| Vendored code | No new third-party code. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/035-std-crypto/
├── plan.md
├── research.md          # R1–R10
├── data-model.md        # the stored password text, the recording event
├── quickstart.md
├── contracts/
│   └── library.md       # std.crypto and the entropy effect
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
src/sha256.c, src/crypto.c (new), src/effects.c, src/check_expr.c, src/check_effects.c, src/rt_call.c,
src/rt_effects.c, src/replay.c, src/doc.c, src/vo.h, Makefile
std/crypto.vo (new), std/random.vo (a sentence)
docs/reference.md, site/docs/guide/, docs/roadmap.md, README.md
tests/cases/crypto/, tests/crypto_vectors.py
```

**Structure Decision**: one new C file for what is built on the hash; the hash itself stays in
`src/sha256.c`.

## Order of work

1. The hash core with a context; SHA-256 and HMAC builtins; vectors.
2. PBKDF2 and the constant-time comparison; `hash_password` / `verify_password` in vo (with a fixed salt
   path for the vector test).
3. The `entropy` effect through every effects table; `random_bytes`; tests, recording, replay.
4. Docs; `make test`; three platforms; timing; results.

## Complexity Tracking

No constitution violations.
