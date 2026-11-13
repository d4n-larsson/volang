# Contract: CLI Additions in v2

This is a delta to `specs/001-vo-lang-core/contracts/cli.md`.

```
vo effects [--json] [--update] FILE          # capability summary; --update rewrites vo.effects
vo run     … [--seed N] [--clock-ms N]
vo test    … [--seed N] [--clock-ms N]       # defaults for test: --seed 1 --clock-ms 0
```

- `vo check` compares the program with `vo.effects` when the record exists (contracts/effects-record.md). E0504 makes it exit 65, like any other error.
- `--seed N` fixes `random_int`. `--clock-ms N` makes `now_ms` return N, N+1, … on successive calls.
- `vo effects` exits 0. With `--update`, it exits 0 after writing, and 65 when the program has check errors (nothing is written).
- Exit status 70 now also covers R0010.
