# Quickstart: validating `std.math`

Prerequisites: the usual toolchain (`make`, GCC 13, the ARM64 and RISC-V cross compilers, `qemu-user`).
mpmath is needed only to regenerate the vectors (`make math-vectors`).

1. **Vendored source matches its record**

       tools/vendor_core_math.sh --verify     # SHA-256 of every kept file vs VENDOR.md, patches apply

2. **Build on three platforms, no assembly in vendored code**

       make && make ARCH=aarch64 && make ARCH=riscv64

3. **Software FMA is correct** (x86-64 host with FMA)

       make math-selftest                     # expect: 0 mismatches

4. **Acceptance values** (US1, US2)

       cat > /tmp/m.vo <<'VO'
       import std.math.sqrt;
       import std.math.round;
       fn main() uses console {
           print(str(sqrt(2.0)) + " " + str(round(-2.5)) + " " + str(round(2.5)));
       }
       VO
       build/vo run /tmp/m.vo                 # 1.4142135623731 -3 3

5. **Domain error** points at the user's call

       echo 'import std.math.log; fn main() uses console { print(str(log(0.0))); }' > /tmp/e.vo
       build/vo run --json /tmp/e.vo          # R0007, span on `log(0.0)` in /tmp/e.vo, exit 70

6. **Bit-identical everywhere** (SC-001, SC-002)

       make test                              # includes tests/math_vectors.py
       make ARCH=aarch64 test && make ARCH=riscv64 test
       make instrumented

7. **Speed** (SC-006): `make speed-check`, which includes the one-million-call math case, under 1,000 ms
   on native x86-64.

8. **Reference budget**: the change states the token increase (one table row).
