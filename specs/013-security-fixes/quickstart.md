# Quickstart: Security Fixes from Review 1

```sh
make && tests/run.sh tests/cases/security && python3 tests/steps.py tests/cases/security
make security-check     # race and rename x1000, server attacks, the review's programs re-run
```

1. Each Critical's program from docs/security-review-1.md: a check error or R0010 (C1-C3), R0007 (C4),
   T0008 (C5); none reaches outside its grant.
2. `tests/security/race.sh` and `rename.sh`: 0 accesses outside the scope in 1,000 runs each.
3. `tests/security/server_attacks.py`: after every attack the server answers a normal request; memory
   stays under its limit.
4. The full gate on three platforms; `make instrumented` clean for C4, H10, H11, L10.
