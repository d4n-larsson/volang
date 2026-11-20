# Results: One Executable per Program (`vo build`)

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | `tests/build_examples.py` (in `make test`): the 7 example programs built; their 5 recordings replayed by `vo replay` and by the executable from an empty directory with equal stdout, stderr and exit status; the expenses example run by its executable equals `vo run` |
| SC-002 | met | same script: a run recorded with `--vo-record` replays with `--vo-replay` from an empty directory to the same output; `tests/cases/build/record_replay` also replays a `vo run --record` recording with the executable |
| SC-003 | met | same script: two builds of each example are byte-identical; `tests/cases/build/same_bytes` |
| SC-004 | met within noise | `tests/build_examples.py --time`, the notes API's recording replayed, paired over 15 rounds: executable 17.5 ms against `vo replay` 17.6 ms (ratio 1.011), and 22.9 ms against 21.9 ms (1.015) in a second run; the script's limit is 1.05 |
| SC-005 | met | on x86-64, `vo build --base build-aarch64/vo` and `--base build-riscv64/vo` of the expenses example: each executable, run under qemu, prints exactly what `vo run` prints |
| SC-006 | met | reference ~14,706 tokens of 15,000 (+84), 40 examples run |

## Verification

| Check | Result |
|---|---|
| `make test` (x86-64) | passed, 38 steps, 7m53s (second run: the first stopped at the three changed expectations below) |
| ARM64, RISC-V (qemu) | `tests/cases/build` 13/13 step cases and `tests/build_examples.py` with 0 failures on each |
| `catalog.py`, `ref_examples.py`, `site_examples.py` (and `--options`), `doc_conformance.py` | passed |
| `make gate` | not run |

## Deviations

- **A file cut off at its end is not T0014**: the trailer is at the end, so a truncated executable has
  no program part to find and starts as the plain toolchain (a usage error, exit 64, nothing of the
  program runs). Bytes changed inside the part are T0014 (`tests/cases/build/damaged` shows both). The
  catalog and the site page say so.
- **Bad values of `--vo-` options are reported with `vo run`'s message**, which names the option without
  the prefix (`--vo-seed x`: "`--seed` and `--clock-ms` need a whole number."), because the executable
  hands its options to the one option parser. Unknown options and missing values name the `--vo-` form.
- **Package paths are rewritten**: packages load by absolute path, so files under the manifest's
  directory are kept under `/vo-app` in the executable; without that, two builds in different
  directories would differ and the build machine's paths would ship.
- **The site page is `site/docs/tools/build.md`**, one page per command as the site is organized, not a
  guide chapter `shipping.md`; it holds the container example.
- **`ld_app` is not cleared before the program runs** (research R4 said it would be): only the loader
  reads it, and the loader does not run after the check.
- **Changed expectations**: `cli/unknown_command` and `cli/unknown_command_json` (the command list gains
  `build`), `https/explain_t0013` (a blank line: T0013 is no longer the catalog's last entry).
- **`tests/steps.py` masks the size** in `built ...` lines and `build` records: it follows the
  toolchain's size, which differs by platform and changes with every toolchain build.
