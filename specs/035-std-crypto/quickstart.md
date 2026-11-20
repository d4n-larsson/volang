# Quickstart: validating spec 035

Prerequisite: `make`. Details: [contracts/library.md](contracts/library.md).

1. **Vectors.** `tests/cases/crypto/sha256`, `hmac`, `pbkdf2` print the published digests;
   `python3 tests/crypto_vectors.py` agrees with Python on random inputs.
2. **Secrets.** A program printing `to_hex(random_bytes(16))` twice prints two different values, different
   again on the next run, also with `--seed 1`; without `uses entropy` it is E0501 with the fix.
3. **Passwords.** `verify_password("pw", hash_password("pw"))` is true; the two stored texts of one
   password differ; malformed texts are false (`tests/cases/crypto/password`).
4. **Determinism.** `vo test` of a file using `random_bytes` prints the same on every run; a recorded run
   replays to the same output (`tests/cases/crypto/random_bytes`, `record_replay`).
5. **Speed.** `python3 tests/crypto_vectors.py --time`: one password hash between 50 ms and 1 s.
6. **Unchanged.** `make test`; `python3 tests/ref_examples.py` under 15,000 tokens.
