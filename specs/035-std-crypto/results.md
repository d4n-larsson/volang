# Results: Hashing and Secrets (`std.crypto`)

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | the published vectors pass as golden cases (`crypto/sha256`: FIPS 180-4; `crypto/hmac`: RFC 4231 cases 1, 2, 3, 4, 6, 7; `crypto/pbkdf2`: RFC 7914 §11) and `tests/crypto_vectors.py` agrees with Python's `hashlib` and `hmac` on 205 hashes and signatures and 5 stored passwords, on x86-64, ARM64 and RISC-V |
| SC-002 | met | `tests/crypto_vectors.py`: 1,000 16-byte tokens of one run all different, none shared with a second run; `crypto/random_run`: two runs differ, also with `--seed 1` and in a built executable with `--vo-seed 1` |
| SC-003 | met | `tests/crypto_vectors.py --time`: one `hash_password` takes 306 ms on this machine (about 1.8 s under qemu, inside a test's 10 s limit) |
| SC-004 | met | `crypto/record_replay`: a program using all six functions records and replays to identical output; `crypto/random_bytes` and `crypto/tasks_test` assert fixed byte values in tests |
| SC-005 | met | `make test` passes; the only changed expectations are `doc/all` and `doc/all_json` (the module list) |
| SC-006 | met | reference ~14,755 tokens of 15,000 (+49), 40 examples run; `doc_conformance.py` 208 library examples run; site 214 examples run |

## Verification

| Check | Result |
|---|---|
| `make test` (x86-64) | passed, 39 steps, 8m00s |
| ARM64, RISC-V (qemu) | `crypto`, `effects`, `effect_sets` cases 76/76, the 3 `crypto` step cases and `tests/crypto_vectors.py` with 0 failures on each |
| `catalog.py`, `ref_examples.py`, `site_examples.py`, `doc_conformance.py` | passed |
| `make gate` | not run |

## Deviations

- **A failing random source is R0007** with its own message, not a new code: it cannot be produced in
  a test, and the catalog requires a golden case per code (research R7; R0007's entry says so).
- **PBKDF2's loop works on raw blocks**: after the first HMAC every message is 32 bytes on a saved pad
  state, so an iteration is two compression calls on one prepared block; `sha256_compress` is exported
  from `src/sha256.c` for it. The time limit is polled every 1,024 iterations (`crypto/timeout`).
- **No `differ` step command**: `same` already prints `same stdout: no`, which the cases expect.
  `tests/steps.py` gained only `app~`.
- **`tests/doc_conformance.py`** learned the effect's place in the canonical order.
- **A replay mismatch on `random_bytes`** (another count than recorded) is reported at the call inside
  `std/crypto.vo`, with the user's call in the trace.
- **In tests the bytes come from a splitmix64 sequence** seeded from the test's seed and mixed with a
  task's path, apart from `random_int`'s generator (`crypto/random_bytes` shows `random_int` unmoved).
