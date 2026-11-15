# Quickstart: Validating Spec 008

How to show that the C toolchain is the assembly toolchain, natively. Commands: [contracts/build.md](contracts/build.md).

## Prerequisites

```sh
sudo apt install gcc-riscv64-linux-gnu gcc-aarch64-linux-gnu qemu-user   # gcc 13 and make are already there
make check-tools
make asm                 # build-asm/vo: the last assembly release, the oracle
```

## During the port (stages 1-7)

```sh
make gate-step           # after every migration step: lint, suite, UB traps, 2,000-program diff
make gate                # at stage boundaries and every engine step: adds the stress suite and the speed check
```

Expected: every gate passes; `results.md` gains one line per step; the assembly line count falls.

## Native (stage 8)

```sh
make                     # native build/vo on x86-64; `file build/vo` says statically linked
make test                # every golden and step case, natively
make diff-asm COUNT=10000            # 0 disagreements (SC-002); for each platform and mode:
python3 tests/diff_asm.py --count 10000 --platform aarch64 --mode float   # modes mixed, float, intedge
make ARCH=aarch64 && make ARCH=aarch64 test         # the same suite on the other two, under qemu
make ARCH=riscv64 && make ARCH=riscv64 test         # (a non-host ARCH sets VO_BIN to build-<arch>/vo)
make instrumented        # ASan + UBSan: 0 reports (SC-005)
make speed-check         # SC-003 (tests/speed.py --native: >= 3x faster than build-asm/vo under qemu,
                         # no program slower) and SC-004
python3 tests/speed.py --baseline build-asm/vo --new build-riscv64/vo --max-geomean 1.10   # SC-004
python3 tests/gc/baseline.py         # 703 bench2 reference runs byte-identical
```

## Cross-platform recordings (SC-007)

```sh
python3 tests/cross_replay.py --record build/vo --replay build-riscv64/vo   # each replay case recorded natively,
python3 tests/cross_replay.py --record build-aarch64/vo --replay build/vo   # replayed on another platform: identical
```

## Size (SC-008)

```sh
make lint                # includes: each src/platform/*.S at most 300 lines, no other .s/.S file
```
