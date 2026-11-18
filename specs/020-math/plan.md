# Implementation Plan: Math functions

**Branch**: `020-math` | **Date**: 2026-11-18 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/020-math/spec.md`

## Summary

A new standard-library module `std.math` gives `sqrt`, `pow`, `exp`, `log`, `sin`, `cos`, `tan`, `asin`,
`acos`, `atan`, `atan2`, `pi`, and `floor`, `ceil`, `round` (to `int`). It is written in vo over two new
std-only primitives (research R6):
- `__math(op, x, y) -> float`, the float functions;
- `__math_int(op, x) -> int`, the three roundings.

The design:
- **Accuracy** (R1): the transcendental functions are **CORE-MATH**'s correctly rounded `cr_*` functions,
  vendored under `third_party/core-math/` from commit `1851882b0f85` (2026-11-17), with 15 files kept. A
  correctly rounded result is unique, so it is bit-identical on every platform by construction.
- **Freestanding build** (R2): CORE-MATH builds through the existing shim headers, plus a new `fenv.h`
  (round to nearest, flags ignored). vo supplies, in its own C:
  - the few libm calls GCC emits for builtins the hardware lacks: `fma` on x86-64, `round` and
    `roundeven` on x86-64 and RISC-V, `fmax` and `fmin` on x86-64;
  - `printf` and `exit` for CORE-MATH's "report this bug" path, routed to an internal error.
- **x86-64 FMA** (R3): baseline x86-64 has no FMA instruction, so vo has a correctly rounded software
  `fma` (about 80 lines over 128-bit integers). It is checked against the hardware instruction on many
  random and edge inputs.
- **No assembly** (R4): two empty `asm("")` register barriers, in `exp.c` and `atan2/tint.h`, become
  `_mm_set_sd(x)` (the upstream clang path). They are recorded as one patch.
- **Edges** (R5): out-of-domain and pole arguments stop with R0007 at the user's call, detected in vo's
  wrapper: a NaN result from non-NaN arguments, `log(±0)`, or `pow(±0, y<0)`. The roundings are exact C
  in `src/math.c`, with R0007 for NaN, infinity and out of range.
- **Tests** (R7):
  - golden cases for every function and edge;
  - a committed vector file of about 10,000 inputs per function, with the correctly rounded results
    computed once by a developer script (mpmath, 200 bits). Its rows include CORE-MATH's published
    worst cases. The suite compares bit for bit on all three platforms.
- **Governance** (R8): the vendoring rule allows only "a released version", and CORE-MATH has no
  releases. A PATCH amendment (1.4.0 → 1.4.1) allows pinning a commit when the upstream publishes no
  releases, with the SHA-256 of every kept file recorded. Applied 2026-11-18.

## Technical Context

**Language/Version**: C (C11, freestanding) for vo's code (`src/math.c`, the builtin tables); vendored C
(CORE-MATH); vo for `std/math.vo`; Python 3 for the vector generator (dev only).

**Primary Dependencies**:
- CORE-MATH, vendored at commit `1851882b0f85ed581af9185cfa020992cec9e7a1` (MIT).
- Dev only: `mpmath` (pure Python, BSD), installed into `build/py/` by a make target. It is used once to
  generate the committed vectors and is not needed by `make test`.
- No runtime dependency is added.

**Storage**: N/A.

**Testing**:
- golden cases `tests/cases/math/` (every function, edge and R0007 path, `vo doc std.math`, the name
  clash);
- `tests/math_vectors.py`, which runs the committed vectors through `vo` and compares the bits;
- `make math-selftest`, which compares the software `fma` with the hardware one on the x86-64 host;
- the reference examples, the fuzzer, the determinism check, and the instrumented build;
- all three platforms.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler and runtime of a programming language, as a CLI.

**Performance Goals**:
- SC-006: one million math calls in under 1,000 ms on native x86-64, even with the software `fma`;
- programs that do not import `std.math` pay nothing;
- the speed gate stays under 1.10.

**Constraints**:
- Principle III: bit-identical results; `-ffp-contract=off` for the vendored files too.
- Principle V: reference +1 table row (about 25 tokens).
- Implementation constraints: vendored code freestanding, without assembly, and passing the
  instrumented build.

**Scale/Scope**:
- vo's own C, about 250 lines:
  - `src/math.c`: the primitive dispatch, domain checks, roundings, and the libm functions GCC calls,
    with the software `fma` (about 200 lines);
  - the builtin tables (about 20).
- Vendored: 10 C files and 5 headers, about 12,700 lines.
- `std/math.vo`: about 120 lines, mostly `vo doc` comments.
- About 15 golden cases, 1 script, 1 vector file (about 150,000 rows, compressed if over 2 MB).
- The binary grows by about 250 KiB (tables).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Gate | Status |
|---|---|
| I. Agent-first: one form, no implicit conversion, no new syntax | PASS. Ordinary imported functions; `float` arguments only (no int overloads). |
| II. Machine-readable feedback | PASS. Out-of-domain is the existing R0007 with a structured report; no new code. |
| III. Determinism and defined behavior | PASS. Correct rounding gives one result for every input; every edge is defined (NaN passes through, domain errors are R0007, overflow is IEEE). |
| IV. Test-first through the CLI | PASS. Golden cases and vectors are written first and fail before `std.math` exists. |
| V. Single source, minimal surface | PASS. One table row in the reference; items documented by `vo doc`. YAGNI: no `log2`, `hypot`, `cbrt`, ... Software `fma` marked `ponytail:`. |
| Implementation: vendored code pinned (constitution 1.4.1) | PASS. CORE-MATH publishes no releases (R8); pinned to commit `1851882b0f85` with the SHA-256 of each kept file, as 1.4.1 allows. |
| Implementation: vendored code unmodified, patches recorded | PASS. One patch (R4) in `third_party/core-math/patches/`. |
| Implementation: vendored code without assembly, freestanding | PASS after the R4 patch. The other inline-asm paths are not compiled (R4). |
| Implementation: vendored code passes the instrumented build | PASS (to verify). R7 runs the vectors under it. |
| Implementation: permissive license | PASS. MIT. |

Post-design re-check: all gates PASS under constitution 1.4.1 (amended 2026-11-18).

## Project Structure

### Documentation (this feature)

```text
specs/020-math/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/std-math.md
└── tasks.md            (/speckit-tasks)
```

### Source Code (repository root)

```text
third_party/core-math/
├── LICENSE
├── VENDOR.md                      # commit, per-file SHA-256, kept files, patches
├── patches/0001-no-inline-asm.patch
└── src/binary64/{sin,cos,tan,asin,acos,atan,exp,log}/<fn>.c
    src/binary64/atan2/{atan2.c,tint.h}
    src/binary64/log/dint.h
    src/binary64/pow/{pow.c,pow.h,dint.h,qint.h}
third_party/shim/fenv.h            # new: FE_* macros, fegetround = to nearest, flags ignored
third_party/shim/stdio.h           # + printf declaration
src/math.c                         # __math / __math_int, domain checks, roundings, libm calls, soft fma
src/check_expr.c, src/vo.h, src/rt_call.c   # two builtin ids and signatures
std/math.vo                        # the module, with vo doc comments
tools/vendor_core_math.sh          # fetch at a commit, verify hashes, apply patches
tools/math_vectors_gen.py          # dev only: mpmath reference vectors
tests/cases/math/                  # golden cases
tests/math_vectors.py, tests/math/vectors.txt
docs/reference.md                  # one row in the std table
```

**Structure Decision**: one new C file (`src/math.c`) and one std module. The vendored files keep their
upstream paths, because `log/dint.h` and `pow/dint.h` differ.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|---|---|---|
| Vendoring ~12,700 lines | Correct rounding is the only way to bit-identical transcendental results on three platforms | Hand-written approximations: proving correct rounding is research-grade work. A non-correctly-rounded port (musl, fdlibm) gives one result only as long as every platform runs the identical C, which is fragile, and it is wrong in the last bit, which agents' golden tests then enshrine. |
| Software `fma` on x86-64 | CORE-MATH's algorithms need a fused multiply-add, and baseline x86-64 has none | `-mfma` makes vo crash with SIGILL on CPUs before 2013. Runtime CPU dispatch needs two builds of CORE-MATH. Deferred: `ponytail:` comment, upgrade when SC-006 or the speed gate fails. |
| Constitution PATCH for commit pinning | Upstream has no releases | Waiting for a release: the project publishes none, and no plan for one is visible. Another library: none is correctly rounded and maintained. |
