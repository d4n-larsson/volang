# Contract: Building and Gates

What a developer runs. Command names are fixed; flags are research R3.

```sh
make                      # build/vo: the host build (ARCH defaults to `uname -m`)
make ARCH=riscv64|aarch64|x86_64   # build-<arch>/vo for a non-host ARCH, run under qemu (bin/vo)
make asm                  # build-asm/vo from ASM_REV (the last assembly release)
make test                 # unchanged suite; VO_BIN selects another build
make gate                 # one migration step's gate (research R10), stops at the first failure
make diff-asm COUNT=N     # tests/diff_asm.py: N generated programs vs build-asm/vo
make ubtrap               # build-ubtrap/vo and the suite under it
make instrumented         # (stage 8) build-instrumented/vo and the suite under ASan + UBSan
make speed-check          # tests/speed.py against build-asm/vo (enforced from stage 7, SC-003/SC-004;
                          # `make gate` times build-riscv64/vo, both under qemu)
make lint                 # SESI over src/platform/riscv64.S (the only pack); strict-warning C build
```

- Per edit: `tools/cc_check.sh` runs as a PostToolUse hook on `src/*.c` and `src/vo.h` (strict warnings, `-fsyntax-only`), beside the SESI hook for assembly.
- `bin/vo`: runs `build/vo` directly when it is a native binary for the host; through
  `qemu-riscv64` (or `qemu-aarch64`) when it is a foreign one. `VO_BIN` overrides, as today.
- Build dependencies: GCC 13 (`gcc`, `gcc-riscv64-linux-gnu`, `gcc-aarch64-linux-gnu` for cross
  checks), binutils for the platform layers, `make`, Python 3 (embedding, header generation,
  tests), `qemu-user` for foreign builds and the oracle. `make check-tools` names what is missing.
- The shipped binary: static ELF, no interpreter, no shared libraries (`readelf -d` shows no
  dynamic section), embedding the reference, catalog and standard library.
