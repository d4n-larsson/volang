# Quickstart: validating spec 034

Prerequisite: `make`. Details: [contracts/](contracts/).

1. **Build and run.** In `examples/url-check`: `vo build main.vo -o /tmp/x/app`; in an empty
   directory `/tmp/x/app` replays `--vo-replay` of each recording exactly as `vo replay` does
   (`tests/cases/build/basic`, `tests/build_examples.py`).
2. **Effects.** `./app --vo-effects` prints `main uses console, net("localhost")`.
3. **Options.** `./app --vo-seed 3 --seed x` gives the program `--seed x`; `./app --vo-bogus` is T0001
   (`tests/cases/build/options`).
4. **Record and replay.** `./app --vo-record r.jsonl ARGS` then `./app --vo-replay r.jsonl` gives the
   same output (`tests/cases/build/record_replay`).
5. **Refusals.** Check errors and an effects record that disagrees write nothing; a truncated app is
   T0014 (`tests/cases/build/errors`, `record_mismatch`, `damaged`).
6. **Same bytes.** Two builds of the same program are identical (`tests/cases/build/same_bytes`).
7. **Other architectures.** `vo build --base build-aarch64/vo main.vo -o app-arm64` runs under
   `qemu-aarch64` with the same output.
8. **Unchanged.** `make test`; `python3 tests/ref_examples.py` reports the reference under 15,000
   tokens.
