# Research: Math functions

Sources: the CORE-MATH repository (https://gitlab.inria.fr/core-math/core-math, inspected at commit
`1851882b0f85ed581af9185cfa020992cec9e7a1`, 2026-11-17), its README, and vo's `Makefile`, shim
headers, builtin tables and `std/` modules.

## R1. Library and pin

- **Decision**: vendor CORE-MATH's binary64 `cr_sin`, `cr_cos`, `cr_tan`, `cr_asin`, `cr_acos`,
  `cr_atan`, `cr_atan2`, `cr_exp`, `cr_log` and `cr_pow`. The pin is the commit above; `VENDOR.md`
  records the commit and the SHA-256 of each kept file.
- **Kept files** (paths as upstream, 10 C files and 5 headers, about 12,700 lines):
  - `src/binary64/{sin,cos,tan,asin,acos,atan,exp,log,pow,atan2}/<name>.c`
  - `src/binary64/atan2/tint.h`, `src/binary64/log/dint.h`
  - `src/binary64/pow/{pow.h,dint.h,qint.h}`
  - `LICENSE`
- **Rationale**: every function is correctly rounded in all rounding modes and proved or exhaustively
  checked by its authors. The license is MIT. Each function is a standalone file. Parts of it are being
  merged into glibc and LLVM libc.
- **Why per-file hashes**: GitLab builds its archive tarballs on demand, and their bytes, and so their
  checksum, are not guaranteed stable. Hashes of the kept files are.
- **Alternatives**:
  - musl or fdlibm: not correctly rounded, so the result is the same only as long as the C is.
  - LLVM libc: correctly rounded for some functions only, with a large build.
  - Hand-written: proving correct rounding is research work.

## R2. Freestanding build

The build uses vo's `CFLAGS` and the `MBED_FLAGS` pattern: `-nostdinc`, the compiler's own headers,
`third_party/shim`, and `-ffp-contract=off` (the upstream README allows contraction "everywhere or
nowhere").

What the kept files need:

| Need | Where | Answer |
|---|---|---|
| `<stdint.h>`, `<inttypes.h>` | all | compiler / existing shim |
| `<fenv.h>`: `fegetround`, `feraiseexcept`, `feholdexcept`, `feupdateenv` | tan, atan2, exp, pow | new `shim/fenv.h` with static inline functions. vo never changes the rounding mode, so `fegetround()` returns `FE_TONEAREST`; exception flags are ignored, because vo never reads them. |
| `<errno.h>` | only under `CORE_MATH_SUPPORT_ERRNO` | not defined, so not needed |
| `<stdio.h>`/`<stdlib.h>`: `printf`, `exit` | pow, tan (the "unexpected worst case, please report" path) | `printf` added to the shim, writing to stderr. `exit` already exists in `tls_libc.c`. Reached only on a library bug, which then ends as an internal error report, never a silent exit. |
| `<x86intrin.h>`: `_mm_getcsr`, `_mm_setcsr`, `_mm_and_pd`, `_mm_add_epi64` | exp, pow, atan2 on x86-64 | compiler header. It pulls in `mm_malloc.h` and so `<stdlib.h>`, so the shim gains `malloc`/`free` declarations if the build asks for them (no call is emitted). The CSR writes only set sticky flags. |
| `__builtin_fma` | all | inline `fmadd` on ARM64 and RISC-V (D extension). A **libcall `fma` on baseline x86-64**, so see R3. |
| `__builtin_roundeven`, `__builtin_round`, `__builtin_floor` | sin, cos, tan, asin, acos, exp, pow | inline on ARM64 (`frintn`/`frinta`/`frintm`). Libcalls `round`/`roundeven`/`floor` on x86-64 without SSE4.1, and `round`/`floor` on RISC-V without Zfa. |
| `__builtin_fmax`, `__builtin_fmin` | atan2 | inline on ARM64 and RISC-V. Libcall on x86-64. |
| `__builtin_sqrt` | asin, acos, pow | inline `sqrtsd`/`fsqrt`/`fsqrt.d` (correctly rounded by IEEE 754), given `-fno-math-errno` |
| `_BitInt(128)` / `__int128` | sin, cos, tan | GCC 13 takes the `__int128` branch |

**Decision**: `src/math.c` (vo's code, under the strict warning set) defines `fma`, `round`,
`roundeven`, `floor`, `fmax` and `fmin` with exact C semantics. The link step decides which are used on
each platform; the unused ones cost nothing. They are compiled only in the freestanding build: the
instrumented (hosted) build links glibc's, which are exact too, so the tests compare like with like.

## R3. Fused multiply-add on x86-64

- **Decision**: a correctly rounded software `fma(a, b, c)`. The product is exact in 106 bits as
  unsigned `__int128`; it is aligned with `c` with a sticky bit, added or subtracted, then rounded once
  to nearest-even. Subnormals, infinities, NaN and signed zero are handled per IEEE 754. It is marked
  `ponytail: software fma, about 10x slower than vfmadd; upgrade to cpuid dispatch with a second
  -mfma build of CORE-MATH if SC-006 or the speed gate fails`.
- **Verification**: `make math-selftest` (hosted, x86-64 host with FMA, which the dev machine has)
  compares it with `__builtin_fma` compiled with `-mfma` on:
  - 10^8 random operand triples;
  - every combination of 200 edge values (zeros, subnormals, powers of two, maximum, infinities, NaN,
    and values that cancel);
  - the triples CORE-MATH's own code produces on its worst-case inputs.
- **Alternatives**:
  - `-mfma` / `-march=x86-64-v3`: vo would SIGILL on older x86-64 CPUs, an undocumented crash
    (Principle III).
  - `__float128` arithmetic: rounds twice, so it is not correct in all cases.
  - Vendoring musl's `fma`: a second vendored library for about 80 lines that the self-test fully checks.

## R4. Inline assembly

The kept files contain two kinds of inline `asm`:
1. `roundeven_finite` (`vroundsd`, `roundsd`, `frintn`), used only when `__builtin_roundeven` is
   unavailable. GCC 13 has it on x86-64 and ARM64. On RISC-V the guard (`__AVX__`, `__SSE4_1__`,
   `__ARM_ARCH`) selects the portable C. **Not compiled.**
2. `asm("":"=x"(r):"0"(x))` in `exp.c` (2 places) and `atan2/tint.h` (1 place), an empty register
   barrier on x86-64 with GCC. **Compiled.**

**Decision**: one patch, `patches/0001-no-inline-asm.patch`, replaces each barrier with
`r = _mm_set_sd(x);`, which is upstream's own clang branch and gives the same value. A build check
fails when any vendored object contains an `asm` statement: `-Werror=...` is not available for this, so
the check greps the preprocessed vendored sources for `asm`/`__asm__`.

## R5. Domain errors and edges

- **Decision**: in `src/math.c`, after calling `cr_*`, raise R0007 when:
  - the result is NaN and no argument is NaN (`sqrt` of a negative number, `log` of a negative number,
    `asin`/`acos` outside -1..1, `sin`/`cos`/`tan` of an infinity, `pow` of a negative base with a
    non-integer exponent); or
  - it is a pole: `log(±0.0)`, or `pow(±0.0, y)` with `y < 0`.
- The message names the function and the argument in vo's float text, with `$1` placeholders like the
  other R0007 messages. Examples:
  - "`sqrt` needs an argument of at least 0.0, got -1.0."
  - "`log` needs an argument greater than 0.0, got 0.0."
  - "`asin` needs an argument from -1.0 to 1.0, got 2.0."
  - "`sin` needs a finite argument, got inf."
  - "`pow` needs a non-negative base or an integer exponent, got -8.0 and 0.333333333333333."
  - "`pow` of 0.0 needs an exponent of at least 0.0, got -1.0."
- **The report points at the user's call.** The primitive runs inside `std/math.vo`. As implemented, the
  error is raised with the node of the nearest frame outside a standard-library file, and a golden case
  checks the span.
- **Rationale**: one generic check covers every Annex F domain error without a per-function table.
  Poles are the two cases where IEEE returns an infinity instead of NaN.
- `sqrt` uses `__builtin_sqrt` directly (hardware, correctly rounded), with no library call.

## R6. API surface

- **Decision**: `std/math.vo` exports 15 functions. They are written in vo over two primitives, added to
  `builtin_names` / `builtin_sigs` (`check_expr.c`), `vo.h` and `rt_call.c`:
  - `__math(op: int, x: float, y: float) -> float`, signature `"iff>f"`, with op 1 `sqrt` … 11 `pow`;
  - `__math_int(op: int, x: float) -> int`, signature `"if>i"`, with op 1 `floor`, 2 `ceil`, 3 `round`.
- `pi()` is written in vo as the literal `3.14159265358979`, which `str` prints, but the literal is not
  the nearest double. So `pi()` returns `__math(0, 0.0, 0.0)`: op 0 returns the double nearest to π,
  `0x1.921fb54442d18p+1`.
- The primitives need no effect, so the effect table gets no entry (`EF_NONE`).
- **Alternatives**:
  - 14 separate primitives: more table rows, no benefit.
  - Builtins: rejected in the spec's clarification.

## R7. Tests and reference vectors

- **Golden cases** (`tests/cases/math/`), written first:
  - the acceptance values of US1/US2;
  - each R0007 message, with its span pointing at the user's call;
  - NaN pass-through;
  - `pow(x, 0.0)` and `pow(1.0, nan)`;
  - signed zeros through `1.0 / f(-0.0)`;
  - `exp` overflow to `inf` and underflow to `0.0`;
  - a `sin` of a huge argument;
  - the halfway cases of `round`;
  - out-of-range `floor`/`ceil`/`round`;
  - `vo doc std.math` and one item;
  - importing `std.math.log` next to a local `log` (the existing clash error);
  - a program with no `uses` (SC-005).
- **Vectors**: `tools/math_vectors_gen.py` (dev only) writes `tests/math/vectors.txt`, one row per call
  in the form `fn x [y] expected_bits` (hex). For each function it takes:
  - about 9,000 random inputs over the domain (log-uniform magnitudes, both signs);
  - the first 1,000 rows of CORE-MATH's `<fn>.wc` worst-case file (the hard-to-round inputs);
  - the special values.
  
  Expected results come from mpmath at 200 bits, rounded to nearest-even double. The program also
  checks that the result is not within 2^-150 of a rounding boundary; when it is, it recomputes at 1,000
  bits. The file is committed, so `make test` needs no mpmath.
- `tests/math_vectors.py` turns the rows into one vo program per function that prints each result's
  bits, through `std.json`'s exact float text. It runs `vo` and compares every row. It is part of
  `make test` on every platform (under qemu on the non-native ones) and of the instrumented suite
  (SC-001, SC-002).
- **How bits are printed**: `str` keeps 15 digits, so it cannot show the last bit. `std.json.write_json`
  prints 17 digits, which round-trip exactly, so the test reads those; non-finite results (`inf`, `-inf`, `nan`) are compared through `str`. No new feature is needed.

## R8. Governance: pinning without releases

- **Finding**: the repository has no release tags. Its only tag is a work branch marker,
  `log2q-log10q-pre-rebase`. The project publishes the repository itself.
- **Decision**: amend the constitution 1.4.0 → 1.4.1 (PATCH, a clarification of an existing rule).
  "Pinned to a released version whose archive checksum … are recorded" becomes "pinned to a released
  version (or, when the upstream publishes no releases, a commit), whose archive checksum (for a commit:
  the SHA-256 of each kept file) …". It is applied through `/speckit-constitution` before tasks. The
  Constitution Check of `specs/019-https/plan.md` is unaffected (Mbed TLS is a release).
- **Updates**: `tools/vendor_core_math.sh COMMIT` fetches the files at a commit, applies the patches and
  rewrites the hashes in `VENDOR.md`. A security fix in CORE-MATH (for example a wrong result) is applied
  in the spec cycle in which it appears, as the rule requires.

## R9. Performance

- On ARM64 and RISC-V, the `cr_*` fast paths are about 20–60 cycles. On x86-64 each call also uses 3–17
  software `fma` calls, at about 15 ns each.
- The worst case is `pow`, at about 300 ns per call, so one million calls take about 0.3 s, plus about
  0.1 s of VM call overhead. This is within the 1,000 ms of SC-006.
- A `tests/perf` case measures it. If it fails, the upgrade is cpuid dispatch (R3).
