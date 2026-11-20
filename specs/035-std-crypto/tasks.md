---

description: "Task list for vo-lang spec 035: Hashing and Secrets (std.crypto)"
---

# Tasks: Hashing and Secrets (`std.crypto`)

**Input**: Design documents from `specs/035-std-crypto/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R10), data-model.md, contracts/library.md,
quickstart.md

**Tests**: REQUIRED (constitution IV). Each task's golden cases land first and fail before its code.

**Organization**:
- Setup: test wiring, the results file.
- Foundational: the hash core with a context, HMAC and the module file with its registration.
- US1 (P1) `random_bytes` and the `entropy` effect; US2 (P1) passwords; US3 (P2) `sha256`,
  `hmac_sha256`, `same_bytes` exported with their vectors; US4 (P3) documentation.
- Polish: comparison with Python, timing, three platforms, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Golden cases**: `tests/cases/crypto/<name>.vo` (`.cmd` `run {}`, `test {}` or `check --json {}`);
  step cases as folders with `steps`; accept after reading the output.
- **Signatures and texts**: contracts/library.md; the stored password text: data-model.md.
- **Unchanged programs**: no existing golden case may change except `doc/all*` (the module list) and
  the cases this list names (FR-011).
- **Reference budget**: `python3 tests/ref_examples.py` after the reference change; at most +100
  tokens (from about 14,706).
- **Between edits**: `make quick AREA=crypto` (T001); `make test` once, through
  `tools/progress.py run test`; the gate only when asked.

---

## Phase 1: Setup

- [X] T001 `tools/quick.sh`: a new area `crypto` (cases `crypto`, `effects`, `effect_sets`, `replay`,
  `tasks`, `doc`; scripts `catalog.py`, `ref_examples.py`, `doc_conformance.py`, `crypto_vectors.py`)
  listed among the known areas; `src/crypto.c`, `src/sha256.c`, `std/crypto.vo` and
  `tests/crypto_vectors.py` map to it.
- [X] T002 [P] `specs/035-std-crypto/results.md`: SC-001–SC-006 pending, Verification, Deviations (empty).

---

## Phase 2: Foundational (blocking)

- [X] T003 `src/sha256.c`, `src/vo.h`: a context `Sha256` (state, 64-byte buffer, buffered count, total)
  with `sha256_start(Sha256 *)`, `sha256_add(Sha256 *, char *, long)`, `sha256_end(Sha256 *, char *out)`;
  `sha256_init/update/final` and `sha256` keep their signatures on one static context. Check: the
  `packages` cases and `tests/cases/build` still pass (package hashes and executables unchanged).
- [X] T004 `src/crypto.c` (new), `src/vo.h`: `hmac_sha256(key, klen, msg, mlen, out)` per RFC 2104 (a key
  longer than 64 bytes is hashed first) built on two saved pad contexts (`Hmac` with inner and outer
  `Sha256`), so PBKDF2 can reuse them.
- [X] T005 `std/crypto.vo` (new, a module comment only for now), `Makefile` (`STD_FILES`), `src/doc.c`
  (`doc_modules`); internal builtin ids after `B_MAP_ERR` in `src/vo.h` (`B_SHA256`, `B_HMAC_SHA256`,
  `B_PBKDF2`, `B_BYTES_SAME`, `B_ENTROPY`; `B_LAST` and `B_COUNT` moved), names and signatures in
  `src/check_expr.c` (`__sha256`, `__hmac_sha256`, `__pbkdf2_sha256`, `__bytes_same`, `__entropy`),
  entries in `src/check_effects.c` (`builtin_effects`: none yet for `__entropy`) and `src/rt_call.c`
  (enum, static assertion, dispatch); `__sha256` and `__hmac_sha256` implemented, the other three in
  their stories. (`tests/cases/doc/all*` change as the module fills: accepted once, in T016.)

**Checkpoint**: `import std.crypto...` resolves; the hash builtins answer.

---

## Phase 3: User Story 1 - Values nobody can guess (P1) 🎯 MVP

**Goal**: `random_bytes(n)` under the `entropy` effect.

**Independent Test**: a program prints two different 16-byte tokens, different again on the next run;
without `uses entropy` it is E0501 with the fix.

- [X] T006 [P] [US1] Golden cases first, `tests/cases/crypto/`: `random_bytes` (`test {}`: lengths 0, 1,
  16 and 65536; two draws differ; printed hex identical on every run), `random_run` (a step case:
  `vo~ run main.vo` twice, `vo~ run --seed 1 main.vo` twice and, after `vo build`, `app~ app --vo-seed 1`
  twice (the hidden-output form of `app`), each printing a token, with a vo program
  check inside each run that two draws differ and have 32 hex digits; the hidden outputs must NOT be
  equal: a new steps command `differ`, the opposite of `same`), `effect_missing`
  (`check --json {}`: E0501 with the fix adding `entropy`; a function declaring only `random`),
  `effect_record` (a step case: `vo effects main.vo` shows `entropy`; `vo effects --update` then a
  second function gaining it is E0504; `vo build` and `app app --vo-effects` show it; `vo run --trace
  t.jsonl` runs with the call traced), `bad_count` (`run {}`: `random_bytes(-1)` is R0007 naming the
  range; a second case file for 65537), `record_replay` (a step case: a program using every function of the module, added to as the
  stories land: `vo~ run --record r.jsonl`, then `vo~ replay r.jsonl`, compared with `same`; the
  recording is not kept), `tasks` (`run {}`:
  `concurrent_map` drawing 8 bytes in each task: lengths right, results pairwise different), and in the `random_bytes` test file a property that draws bytes
  (reproducible across runs).
- [X] T007 [US1] `src/effects.c` (`es_names` gains `entropy`, bit 11) and every table, mask or loop
  sized by the number of effects (search for `process` and for the count 11 in `src/effects*.c`,
  `src/check_effects.c`, `src/rt_effects.c`, `src/effects_record.c`, `src/replay.c`, `src/doc.c`,
  `src/vm.c`); `builtin_effects[B_ENTROPY]`; existing effect cases unchanged.
- [X] T008 [US1] `src/rt_effects.c`, `src/rt_call.c`: `__entropy(n)`: R0007 "`random_bytes` needs a
  count from 0 to 65536, got $1." outside the range; `getrandom` looped over short reads and EINTR; any
  other failure R0007 "The system's random source failed: `random_bytes` cannot continue." (R7; a
  sentence in R0007's `docs/errors.md` entry); in tests a generator of its own seeded from the test's seed
  (`rt_effects_start`), separate from `random_int`'s state; in task children as R6.
- [X] T009 [US1] `src/replay.c`: an `entropy` event per call (count and bytes as hex) when recording;
  replay returns the next event's bytes and raises R0014 on another count (data-model.md).
- [X] T010 [US1] `std/crypto.vo`: `export fn random_bytes(n: int) -> bytes uses entropy` with its doc
  comment (a runnable example showing `len`); `tests/steps.py`: the `differ` command and `app~` (as `vo~`); accept T006.

**Checkpoint**: secrets can be drawn, reviewed in `vo.effects`, tested and replayed.

---

## Phase 4: User Story 2 - Passwords stored safely (P1)

**Goal**: `hash_password` and `verify_password`.

**Independent Test**: `verify_password("pw", hash_password("pw"))` is true, another password false,
two stored texts differ, the published PBKDF2 vectors verify.

- [X] T011 [P] [US2] Golden cases first, `tests/cases/crypto/`: `pbkdf2` (`run {}`: `verify_password`
  true for stored texts built from RFC 7914 §11's PBKDF2-HMAC-SHA256 vectors, first 32 bytes:
  "passwd"/"salt"/1 → `55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc`,
  "Password"/"NaCl"/80000 → `4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56`; false
  for the same texts with another password), `password` (`test {}`, several tests so none does more than two full-cost derivations: one hashes
  twice: the texts differ and start with `pbkdf2-sha256$600000$`; one hashes once and verifies the
  right password; wrong passwords and every other check use stored texts with a cost of 1 to 1,000,
  made by `tests/crypto_vectors.py`'s method and written into the case; malformed stored texts are
  false: empty, three parts, another method name, iterations `0`, `x`, `10000001`, a salt or key that is
  not base64, a key of 31 bytes), `timeout` (`run --timeout 1 {}`: verifying a stored text with
  10,000,000 iterations stops with R0009).
- [X] T012 [US2] `src/crypto.c`, `src/rt_call.c`: `__pbkdf2_sha256(password, salt, iterations)`: one
  32-byte block of PBKDF2 (RFC 8018) with the pads of T004 (two compressions per iteration), `POLL`
  every 1,024 iterations so `--timeout` applies; iterations outside 1 to 10,000,000 is R0007.
  `__bytes_same(a, b)` per R4 (lengths differ: false; else OR of XORs through a volatile accumulator).
- [X] T013 [US2] `std/crypto.vo`: `hash_password` ("`pbkdf2-sha256$600000$` + base64(salt) + `$` +
  base64(key)", salt `random_bytes(16)`, `uses entropy`) and `verify_password` (pure: "exactly
  `pbkdf2-sha256`", "decimal, 1 to 10,000,000", "standard base64 of exactly 32 bytes" for the key;
  anything else false; compared with `__bytes_same`), with doc comments and runnable examples (a fixed
  low-cost stored text for `verify_password`; `hash_password`'s example shows only the text's prefix);
  `record_replay` gains both; accept T011.

---

## Phase 5: User Story 3 - Hashes, signatures and safe comparison (P2)

**Goal**: `sha256`, `hmac_sha256`, `same_bytes` exported.

**Independent Test**: the FIPS and RFC 4231 vectors print the published digests.

- [X] T014 [P] [US3] Golden cases first, `tests/cases/crypto/`: `sha256` (`run {}`: empty, "abc", the
  56-byte FIPS message, one million `a`, each as hex), `hmac` (`run {}`: RFC 4231 cases 1, 2, 3, 4, 6
  and 7), `same_bytes` (`run {}`: equal, differing first byte, differing last byte, different lengths,
  both empty), `webhook` (`run {}`: the contract's `signed_by` accepts a body and rejects a changed
  one), `pure` (`check --json {}`: the three functions called from a function with no `uses`: clean).
- [X] T015 [US3] `std/crypto.vo`: `sha256`, `hmac_sha256`, `same_bytes` with doc comments and runnable
  examples; accept T014.

---

## Phase 6: User Story 4 - Documented where agents read (P3)

- [X] T016 [P] [US4] `docs/reference.md`: `entropy` row in the effects table (`std.crypto.random_bytes`,
  `hash_password`), `std.crypto` row in the module table, "not for secrets: `std.crypto.random_bytes`"
  on `random_int`'s row; the `--record` paragraph's list gains the bytes of `random_bytes`;
  `python3 tests/ref_examples.py` (≤ +100 tokens); `tests/doc_conformance.py` passes; re-accept
  `tests/cases/doc/all*` after reading the diff;
  `tests/cases/crypto/doc` (`vo doc std.crypto`).
- [X] T017 [P] [US4] `std/random.vo` module comment (not for secrets); `site/docs/guide/standard-library.md`:
  a `std.crypto` section with runnable examples (a hash, a webhook check, a password round trip shown
  by its boolean results, a token shown by its length); `site/docs/guide/effects.md` and
  `site/docs/tools/replay.md`: `entropy` where effects and recorded inputs are listed; `docs/security.md`:
  a recording holds the bytes `random_bytes` returned (keep recordings as you keep secrets);
  `python3 tests/site_examples.py`.
- [X] T018 [P] [US4] `docs/roadmap.md` (row and section for 035), `README.md`,
  `docs/review-2026-11/README.md` ("Fixed since the review" row).

---

## Phase 7: Polish & cross-cutting

- [X] T019 `tests/crypto_vectors.py` (R10) and its line in the Makefile's `test` script list: 200
  random inputs (lengths 0 to 300, and keys 0 to 200) hashed by one vo program and compared with
  Python's `hashlib.sha256` and `hmac`; 5 PBKDF2 cases at costs 1 to 2,000 through `verify_password`
  against `hashlib.pbkdf2_hmac`; SC-002: 1,000 tokens from one run all different and none shared with a
  second run; `--time`: SC-003, one `hash_password` between 50 ms and 1 s (median of five; the figure is this
  machine's, and the output says so).
- [X] T020 `make test` (via `tools/progress.py run test`); then `make ARCH=aarch64` and
  `make ARCH=riscv64` with `tests/cases/crypto` and `tests/crypto_vectors.py` under each; not the gate
  unless asked.
- [X] T021 `specs/035-std-crypto/results.md`: SC-001–SC-006 with measurements, Verification, Deviations;
  spec status.

---

## Dependencies & Execution Order

- Setup (T001–T002) → Foundational (T003–T005) → US1 (T006–T010).
- US2 (T011–T013) needs US1 (`random_bytes` for the salt) and T004. US3 (T014–T015) needs only the
  Foundational phase. US4 (T016–T018) needs US1–US3.
- Polish needs everything.

## Parallel opportunities

- T002 with T001; the golden-case tasks marked [P] (T006, T011, T014) touch only new files.
- US3 can be done before or alongside US1 and US2 (different functions; `std/crypto.vo` is shared, so
  its edits are sequential).
- T016, T017 and T018 are documentation in different files.

## Implementation Strategy

- **MVP**: Phases 1–3 (T001–T010): secret bytes with their effect.
- **Then** US2 (passwords), US3 (the exported hashes), US4 (docs), and the Polish phase.
- `make quick AREA=crypto` between edits; `make test` once at T020.
