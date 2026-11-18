# Results: Math functions

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | `tests/math_vectors.py`: 109,517 rows (about 10,000 per function: 9,000 seeded random inputs, the first 1,000 of CORE-MATH's published worst cases, and special values), every result equal bit for bit to the correctly rounded value computed with mpmath at 200 bits (1,000 near a midpoint). 0 mismatches. |
| SC-002 | met | The same 109,517 rows give 0 mismatches on x86-64 (native, software `fma`), ARM64 and RISC-V (qemu), and under the instrumented build (ASan and UBSan, CORE-MATH included). |
| SC-003 | met | `tests/cases/math/`: 24 cases, one for every edge of the spec and every R0007 path (the 9 domain and pole messages of `sqrt`, `log`, `sin`, `cos`, `tan`, `asin`, `acos`, `pow`, and `floor` of NaN, `ceil` of inf, `round` and `floor` out of the int range); each runtime-error case checks with `--json` that the span is the user's call, not `std/math.vo`. |
| SC-004 | met | Reference: about 12,360 tokens, +29 (one row in the standard-library table; budget 15,000). |
| SC-005 | met | `tests/cases/math/no_uses`: a function with no `uses` calls all 15 functions; no diagnostics. |
| SC-006 | met | `tests/math_speed.py`: 1,000,002 calls of `pow`, `sin` and `log` in 436 ms on native x86-64 with the software `fma` (limit 1,000 ms; the loop alone 7 ms); 585 ms under ASan and UBSan. The `ponytail:` upgrade (hardware FMA by CPU dispatch) is not needed. |

## Verification

| Check | Result |
|---|---|
| `make test` (x86-64) | passed: golden 709, step 210 + 212 + 35, `tls-selftest`, `math-selftest` (180,199,121 checks, 0 mismatches), `cm-noasm` on three platforms, catalog 110 sections, reference 37 examples, site 44 pages / 192 examples, doc conformance (107 items, 107 examples, 0 failures), `math_vectors.py` 109,517 rows, fuzzers, examples, diff-engines 1,000 (0 disagreements) |
| `make lint` | clean: strict C on x86-64, ARM64, RISC-V; SESI; platform files; `tools/vendor_core_math.sh --verify` |
| `make instrumented` | 709 of 709 under ASan and UBSan; `math_vectors.py` 109,517 rows, 0 failures; `math_speed.py` 585 ms |
| ARM64 | `make ARCH=aarch64 test` (qemu): golden 709, step 210 + 212 + 35, doc conformance, `math_vectors.py` 109,517 rows (0 failures), diff-engines 1,000 (0 disagreements). A first run failed 29 cases only because a second `tests/run.sh` shared `build/run-work/` with it; alone it passes. |
| RISC-V | `make ARCH=riscv64 test` (qemu): golden 709, step 210 + 212 + 35, doc conformance, `math_vectors.py` 109,517 rows (0 failures), diff-engines 1,000 (0 disagreements) |
| Math determinism | `tests/run.sh --repeat 20 tests/cases/math`: 24 of 24, identical over 20 runs |
| `make gate` (partial, stopped) | passed: lint, test, ubtrap (709 of 709), diff-asm 2,000 (0 disagreements), tasks / files and processes / agent testing determinism (20 runs each, all identical), agent testing scale; stopped during https determinism (golden 8 of 8 had passed) at the user's request. Not run: https step cases and interop/fuzz/speed, the math step (run on its own: passed), gc-check, the RISC-V speed comparison |

## Size

- **Vendored**: CORE-MATH at commit `1851882b0f85`, 16 files (10 C files, 5 headers, LICENSE), 14,266 lines of C;
  one patch (3 lines).
- **vo's layer**: `src/math.c` (110 lines: the primitives, domain checks, roundings to int) and
  `src/math_libm.c` (232 lines: the software `fma` and the C functions the compiler calls), `std/math.vo`
  (95 lines, mostly documentation), shims (`fenv.h`, `math.h`, `errno.h`, `printf`).
- **Binary**: +210 KiB of code and tables (the CORE-MATH objects and vo's two files); `build/vo` text is
  now 1,000 KiB.
- **Tests and tools**: 24 golden math cases, 3 doc cases, `tests/math_vectors.py`, `tests/math_speed.py`,
  `tests/math/fma_selftest.c`, `tests/math/vectors.txt.gz` (2.1 MB), `tools/math_vectors_gen.py`,
  `tools/vendor_core_math.sh`, `tools/cm_noasm.awk`.

## Deviations

- **Line counts**: the plan estimated about 12,700 vendored lines; the kept files are 14,266. The plan
  listed 15 files; with `LICENSE` there are 16.
- **Two C files instead of one**: the C library functions are in `src/math_libm.c`, separate from
  `src/math.c`, so the hosted self-test can build them without `vo.h`.
- **More shims**: `math.h` (`NAN`, `INFINITY`), `errno.h` (`pow` sets `ERANGE` unconditionally) and two
  more `fenv.h` functions, beyond the plan's `fenv.h` and `printf` (research.md, implementation notes).
- **Undefined symbols** (T006): x86-64 needs `fma`, `fmax`, `fmin`, `roundeven`; RISC-V `floor`, `round`;
  ARM64 none. `fma` is defined only on x86-64 without FMA.
- **Vector inputs** go through `std.json` (exact), since `parse_float` keeps 15 digits.
- **The vector generator took the machine down twice** before its memory use was bounded (research.md);
  it now runs under a 4 GiB limit.
- **`make gate` was run only in part**: its full run (gc-check, diff-asm, speed against build-asm under qemu) takes
  long, and this session's machine went down three times. `make test`, `make lint`, `make instrumented` and
  the three-platform suites passed; the gate's new math step (`tests/cases/math` 20 times, `math_speed.py`)
  was run on its own and passed. A later `make gate` run passed every step up to https determinism and
  was then stopped (Verification); gc-check and the speed comparison remain to be run.
- **Fixed after the spec (`str` of a float)**: `str` scaled by a power of ten in floating point before
  rounding to 15 digits, so it rounded twice and printed a wrong last digit for about 4% of values
  (8,772 of 199,959 random doubles; `str(exp(1.0))` was `2.71828182845904`). It now rounds the exact
  value once, ties to even: in 128-bit integers for |x| from about 1e-8 to 1e34 (as fast as before:
  200,000 calls in 0.06 s), else with `std.json`'s big integers. 0 wrong of 399,924 values against C's
  `printf`; case `run/float_str_rounding`. The only existing output that changed is `math/values`
  (`2.71828182845905`), and the `exp` doc example is back to `2.71828182845905`.
