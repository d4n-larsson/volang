# Contract: CLI Additions in v3

This is a delta to `specs/002-effects-functions-generics/contracts/cli-v2.md`.

```
vo run  … [--max-memory MiB] [--mem-stats]
vo test … [--max-memory MiB] [--mem-stats]
```

- **`--max-memory MiB`:** the live-data limit, a whole number from 1 to 65536 (default 512). For `test` it applies to every test in the run. Any other value is a usage error (T0001, exit 64).
- **`--mem-stats`:** report memory use.
  - **Human mode:** after the program ends (normally or with an error), one line on stderr: `memory: peak live <size>, allocated <size>, <n> collections (limit <size>)`. Sizes use B, KiB, MiB or GiB with one decimal.
  - **`--json`:** one `memory` record (contracts/output-schema-v3.md). For `test`, one record per test, right after the test's `test` record.
  - **Without the flag,** output is unchanged.
- **No other surface.** Stress mode is not part of the shipped toolchain. It exists only in a separate build for maintainers (`make STRESS=1`, contracts/memory-model.md). The shipped binary has no stress code and reads no stress setting, so there is nothing to document in the reference (constitution V).
- **Exit statuses** are unchanged: R0006 and R0009 are runtime errors (70; a timeout is 124).
