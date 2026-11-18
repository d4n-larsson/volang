---

description: "Task list for vo-lang spec 020: Math functions"
---

# Tasks: Math functions

**Input**: Design documents from `specs/020-math/`

**Prerequisites**: plan.md, spec.md, research.md (R1-R9), data-model.md, contracts/std-math.md,
quickstart.md

**Tests**: REQUIRED (constitution IV). Golden cases and the vector file land first, and fail before each
piece.

**Organization**:
- Setup: the constitution amendment (done), vendoring and the reference vectors.
- Foundational: CORE-MATH builds freestanding on three platforms, with vo's libm functions and the
  software `fma`; the two primitives.
- One phase per user story: US1, US2 and US3 are P1; US4 is P2.
- Docs and the gate.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Golden cases** (`tests/cases/math/`): a `.vo` file with `.out`, and `.code` for runtime errors, as in
  `tests/cases/run/`. A runtime-error case also checks the JSONL report (`--json`) for the span.
- **Regression rule**: existing programs keep byte-identical results; nothing outside `std.math`
  changes. Run `make test` after every task.
- **vo's own C** (`src/math.c`): the strict warning set, no UB, explicit range checks before every
  float→int conversion (constitution: "Arithmetic whose result the language defines … MUST be computed
  with explicit checks").
- **Vendored C** (`third_party/core-math/`): unmodified except the recorded patch; compiled with the
  same constraints as the binary (`-ffp-contract=off`, freestanding, no assembly).
- **Vectors are never edited by hand.** A mismatch is a bug in vo's build, shims or `fma` until proved
  otherwise.
- **Shortcuts**: marked `ponytail:` (plan.md, Complexity Tracking).

---

## Phase 1: Setup

- [X] T001 Apply the constitution amendment 1.4.0 → 1.4.1 (vendoring pinned to a commit when the
  upstream has no releases, with per-file SHA-256) in `.specify/memory/constitution.md`, and re-check
  the gate in `specs/020-math/plan.md`.
- [ ] T002 [P] Create `specs/020-math/results.md` with an SC-001 to SC-006 table (status, evidence), a
  Verification table and a Size section, in the format of `specs/019-https/results.md`.
- [ ] T003 Vendoring (research R1, R4), `tools/vendor_core_math.sh`:
  - `tools/vendor_core_math.sh COMMIT` fetches
    `https://gitlab.inria.fr/core-math/core-math/-/raw/COMMIT/PATH` for each kept file into
    `third_party/core-math/PATH`, keeping upstream paths. The kept files:
    - `LICENSE`;
    - `src/binary64/{sin,cos,tan,asin,acos,atan,exp,log,pow,atan2}/<name>.c`;
    - `src/binary64/atan2/tint.h`, `src/binary64/log/dint.h`;
    - `src/binary64/pow/{pow.h,dint.h,qint.h}`.
  - It applies `third_party/core-math/patches/*.patch` in order, then writes
    `third_party/core-math/VENDOR.md`. The format follows `third_party/mbedtls/VENDOR.md`:
    - library, source, commit `1851882b0f85ed581af9185cfa020992cec9e7a1` (2026-11-17), license MIT;
    - a `## Files` list, one line per file, in the form `` - `PATH` SHA256 `` (the upstream hash, before
      patches);
    - a `## C files` list, one `` - `PATH` `` per `.c`, which the Makefile reads;
    - a `## Patches` list with each patch's reason;
    - an `## Updating` section.
  - `tools/vendor_core_math.sh --verify` refetches nothing. It re-hashes each file with the patches
    reversed, compares with `VENDOR.md`, and exits non-zero on any difference.
  - `patches/0001-no-inline-asm.patch`: in `src/binary64/exp/exp.c` (2 places) and
    `src/binary64/atan2/tint.h` (1 place), replace `__m128d r; asm("":"=x"(r):"0"(x));` with
    `__m128d r = _mm_set_sd(x);` (upstream's clang branch). The reason: constitution "no assembly"; same
    value.
  - Run it at the pinned commit and commit the result.
- [ ] T004 [P] Reference vectors (research R7):
  - `tools/math_vectors_gen.py` (dev only, Python 3 with mpmath). `make math-vectors` installs a pinned
    mpmath into `build/py/` (as `make tls-deps` does) and runs it.
  - It writes `tests/math/vectors.txt` with rows in the form `FN X_HEX [Y_HEX] EXPECTED_HEX`, as hex
    double bit patterns, for each of `sqrt exp log sin cos tan asin acos atan atan2 pow`:
    - about 9,000 seeded random inputs over the domain (log-uniform magnitude, both signs, plus inputs
      near domain edges and near multiples of π/2 for trigonometry);
    - the first 1,000 inputs of CORE-MATH's `src/binary64/<fn>/<fn>.wc` from the scratch clone (not
      vendored, read only by this script, and only rows inside the domain);
    - special values (±0, ±inf, NaN, smallest subnormal, largest double), only where the result is not
      R0007.
  - Expected results: mpmath at 200 bits, rounded to nearest-even double. When the 200-bit value is
    within 2^-150 relative of a rounding boundary, it recomputes at 1,000 bits.
  - Each row group is tagged with a `#` comment (random, worst, special). The file is gzipped to
    `tests/math/vectors.txt.gz` if it is over 2 MB.

---

## Phase 2: Foundational (blocks every story)

- [ ] T005 Shim headers (research R2):
  - `third_party/shim/fenv.h`: `FE_TONEAREST 0`, `FE_DOWNWARD`, `FE_UPWARD`, `FE_TOWARDZERO`,
    `FE_INVALID`, `FE_DIVBYZERO`, `FE_OVERFLOW`, `FE_UNDERFLOW`, `FE_INEXACT`, `FE_ALL_EXCEPT`, and
    `typedef struct { unsigned v; } fenv_t; typedef unsigned fexcept_t;`. The functions are
    `static inline`:
    - `fegetround` returns `FE_TONEAREST`;
    - `feraiseexcept`, `feclearexcept`, `feholdexcept` and `feupdateenv` do nothing and return 0;
    - `fetestexcept` returns 0.

    A comment says why: vo never changes the rounding mode or reads exception flags.
  - Add `int printf(const char *fmt, ...);` to `third_party/shim/stdio.h`. Implement it in
    `src/tls_libc.c` with the existing `vsnprintf`, writing to stderr.
  - Add `malloc`, `free` and `posix_memalign` declarations to `third_party/shim/stdlib.h` only if
    `<x86intrin.h>` (through `mm_malloc.h`) needs them to compile; no definition, since nothing calls
    them.
  - Update `third_party/shim/README.md` (CORE-MATH, and where `printf` is implemented).
- [ ] T006 Build in `Makefile`:
  - `CM := third_party/core-math`, and `CM_SRCS` read from `$(CM)/VENDOR.md`'s `## C files` list (as
    `MBED_SRCS` is).
  - `CM_FLAGS = -nostdinc -isystem $(gcc include) -I third_party/shim -I $(dir $<)`, so each file finds
    its own `dint.h`/`tint.h`.
  - The objects are compiled with `$(CFLAGS)` minus the strict warnings, without `-Werror`, and with
    `-ffp-contract=off`. They are added to `OBJS`, and also to the instrumented build with the
    sanitizers.
  - A no-assembly check, `make cm-noasm`, part of `all`: preprocess each `CM_SRCS` file with the
    platform's flags and fail if `asm` or `__asm__` appears in the output. It runs for all three `ARCH`
    values.
  - `make ARCH=aarch64` and `make ARCH=riscv64` must build. Record the undefined symbols that the
    `CM` objects need from outside (`nm -u`) for each platform in `results.md`.
- [ ] T007 vo's libm functions in `src/math.c` (research R2, R3). They are compiled only without
  `VO_HOSTED`; the hosted build uses glibc's.
  - `double fma(double, double, double)`, correctly rounded in software on x86-64, with the comment
    `ponytail: software fma, ~10x slower than vfmadd; cpuid dispatch with a -mfma build of CORE-MATH if
    SC-006 or the speed gate fails`. On ARM64 and RISC-V it is `__builtin_fma`, which is inline. The
    algorithm, in about 80 lines over `unsigned __int128`:
    - the exact 106-bit product;
    - alignment with `c` with a sticky bit;
    - add or subtract;
    - normalize, then round once to nearest-even;
    - subnormals, overflow to ±inf, NaN propagation, `inf*0` → NaN, and exact zero sums with the IEEE
      sign (+0, or -0 when both are -0).
  - `round` (halves away from zero), `roundeven`, `floor`, `fmax` and `fmin` (C semantics: a NaN
    operand returns the other), all exact, through bit manipulation.
  - The software `fma` is also built under the name `vo_soft_fma` so the self-test can call it.
  - The link must succeed on all three platforms with no undefined symbol.
- [ ] T008 [P] `make math-selftest` (research R3), the hosted program `tests/math/fma_selftest.c`:
  - built with `-mfma` and linked with `src/math.c`'s `vo_soft_fma` (compiled hosted, with a define
    that exposes it);
  - compares `vo_soft_fma(a, b, c)` with `__builtin_fma` (hardware) bit for bit:
    - 10^8 seeded random triples (random bit patterns, plus a set with exponents chosen so that `a*b`
      and `c` cancel);
    - all triples of 200 edge values (±0, ±smallest subnormal, ±largest subnormal, ±smallest normal,
      powers of two, 1±ulp, ±max, ±inf, NaN);
  - prints the count and the first 10 mismatches, and exits non-zero on any;
  - skips with a message when the host CPU has no FMA (`__builtin_cpu_supports("fma")`).

  Part of `make test` on x86-64.
- [ ] T009 Primitives (research R6), in `src/vo.h`, `src/check_expr.c` and `src/rt_call.c`:
  - builtin ids 66 `__math` (signature `"iff>f"`) and 67 `__math_int` (signature `"if>i"`);
  - `B_COUNT` and `B_LAST` (and the `_Static_assert` in `src/rt_call.c`) updated;
  - std-only, through the existing `__` rule (`std_only_ok`); no effect entry in `src/check_effects.c`;
  - dispatch in `src/rt_call.c` to `math_call(op, x, y, node)` and `math_int(op, x, node)`, declared in
    `src/vo.h`;
  - check every switch over builtin ids (`grep -n 'B_HTTPS\|BU_HTTPS' src/*.c`) for the new ids:
    compile, the VM, `vo doc` and query/edit (names that cannot be used for a rename).
- [ ] T010 `math_call` in `src/math.c` (research R5, contracts/std-math.md):
  - op 0: π as `0x1.921fb54442d18p+1`;
  - op 1: `__builtin_sqrt`;
  - ops 2-11: `cr_exp`, `cr_log`, `cr_sin`, `cr_cos`, `cr_tan`, `cr_asin`, `cr_acos`, `cr_atan`,
    `cr_atan2`, `cr_pow`, declared in `src/math.c`.

  Then the domain check:
  - R0007 when the result is NaN and no argument is NaN;
  - R0007 for the poles: `log(±0.0)`, and `pow(±0.0, y)` with `y < 0`.

  The messages are exactly those of `contracts/std-math.md`, with values in `str` text (`float_to_text`).
  The span must be the user's call, not `std/math.vo`:
  - find how a runtime error raised inside a std function is reported today (write a throwaway program
    that makes a std function raise R0003 or similar);
  - if the span lands in `std/`, add a helper that raises with the call node of the innermost frame
    outside a std file;
  - use it in `math_call` and `math_int`.

**Checkpoint**: `make`, `make ARCH=aarch64`, `make ARCH=riscv64` and `make math-selftest` pass; existing
`make test` unchanged.

---

## Phase 3: User Story 1 — Compute with roots, powers, logarithms and angles (P1) 🎯 MVP

**Goal**: `import std.math.sqrt;` and the other float functions work, with correctly rounded results and
R0007 for out-of-domain arguments.

**Independent Test**: `tests/run.sh tests/cases/math` passes.

### Tests (first, failing)

- [ ] T011 [P] [US1] Golden cases in `tests/cases/math/`:
  - `values`: `sqrt(2.0)` prints `1.4142135623731`, plus `pow(1.05, 10.0)`, `exp(1.0)`, `log(10.0)`,
    `sin(pi() / 6.0)`, `atan2(1.0, 1.0)`, `pi()`. Print them through `str` and through
    `std.json.write_json` (17 digits), with values from mpmath.
  - One case per R0007 message of `contracts/std-math.md`: `sqrt(-1.0)`, `log(0.0)`, `log(-1.0)`,
    `asin(2.0)`, `acos(-1.5)`, `sin(inf)` (`1.0 / 0.0`), `tan(-inf)`, `pow(-8.0, 1.0 / 3.0)`,
    `pow(0.0, -1.0)`. Each has `.code` 70 and checks with `--json` that the span is the user's call in
    the case file.
  - `nan_passthrough`: every function of NaN (`0.0 / 0.0`) prints `nan`, except `pow(nan, 0.0)` and
    `pow(1.0, nan)`, which print `1.0`.
  - `signed_zero`: `1.0 / sin(-0.0)`, `1.0 / sqrt(-0.0)`, `1.0 / atan(-0.0)` print `-inf`.
  - `overflow`: `exp(1000.0)` is `inf`, `exp(-1000.0)` is `0.0`, `pow(10.0, 400.0)` is `inf`, and
    `pow(-10.0, 401.0)` is `-inf`.
  - `huge_trig`: `sin` and `cos` of a value near 1.0e22, built by multiplication, with mpmath's values.
  - `int_arg`: `sqrt(2)` is the existing argument type error; record its code and fix.
  - `no_uses`: a program calling every function from a `fn` with no `uses` checks with no diagnostics
    (SC-005).
  - `name_clash`: a file defining `fn log(...)` that also imports `std.math.log` gives the existing clash
    error. A file that defines `log` and does not import it is unaffected.

### Implementation

- [ ] T012 [US1] `std/math.vo`:
  - a header comment like `std/time.vo`'s;
  - `export fn pi() -> float`, `sqrt`, `exp`, `log`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`,
    `atan2(y: float, x: float)` and `pow(x: float, y: float)`, each returning `__math(op, x, y)` with the
    op codes of `data-model.md` (`0.0` for an unused `y`);
  - each preceded by a `vo doc` comment: one sentence, the domain ("R0007 if …"), the edges from
    `contracts/std-math.md`, and one example line `EXPR is VALUE`.

  Register the module wherever std modules are listed: grep for `"collections"` in `src/` and `tools/`.

**Checkpoint**: T011 passes.

---

## Phase 4: User Story 2 — Round floats to integers (P1)

**Goal**: `floor`, `ceil` and `round` return exact `int` values; NaN, infinity and out-of-range values
are R0007.

**Independent Test**: `tests/run.sh tests/cases/math` (the rounding cases).

### Tests (first, failing)

- [ ] T013 [P] [US2] Golden cases in `tests/cases/math/`:
  - `rounding`: `floor(-2.5) ceil(-2.5) round(-2.5) round(2.5) round(h)` prints `-3 -2 -3 3 0`, where
    `h` is 0.49999999999999994, built as `0.5 - 1.0 / float(18014398509481984)`. Float literals have at
    most 15 significant digits, so every value below with more digits is built with `float(int)`. Also: `floor` and `ceil` of exact integers, of ±0.0 (both `0`), of
    `float(4503599627370496) + 1.0` (2^52+1, exact), of the largest double below 2^63
    (`float(9223372036854774784)`), and of -2^63 (which is
    `-9223372036854775808`).
  - `round_range`: one case each for `floor` of NaN, `ceil` of inf, and `round` of 2^63 (built as
    `float(9223372036854775807)`, which rounds to 2^63). Each is R0007 with the message
    `` `floor` needs a finite value within the int range, got $1. `` (with that function's name), a
    span on the user's call, and `.code` 70.

### Implementation

- [ ] T014 [US2] `math_int(op, x, node)` in `src/math.c`, and `floor`, `ceil`, `round` in
  `std/math.vo` with `vo doc` comments:
  - R0007 unless `x` is finite and `-9223372036854775808.0 <= r < 9223372036854775808.0`, where `r` is
    the rounded value;
  - round in `double` first (exact: `floor`/`ceil`/`round` of a double is a double), check the range,
    then convert. That conversion cannot overflow after the check.
  - `round` rounds halves away from zero, without `floor(x + 0.5)`: use the `round` from T007, or
    `__builtin_round` in the hosted build.

**Checkpoint**: T013 passes.

---

## Phase 5: User Story 3 — Same answer on every machine (P1)

**Goal**: every vector row matches bit for bit on x86-64, ARM64 and RISC-V, and under the instrumented
build.

**Independent Test**: `python3 tests/math_vectors.py` on each platform.

### Tests (first, failing)

- [ ] T015 [P] [US3] `tests/math_vectors.py`:
  - reads `tests/math/vectors.txt` (or `.gz`) and, per function, writes one vo program to a temporary
    directory. The inputs are built from their bits; since float literals have no exponent, use a vo
    helper that parses hex digits into an `int` and then into a float by exact scaling, or pass the
    inputs on stdin as 17-digit decimal text, read with `parse_float`.
    - **First**, check whether `parse_float` and the `std.json` reader round-trip 17 digits exactly.
      Use the one that does, and record which in `research.md` R7.
  - prints each result with `std.json.write_json` (17 digits, exact), or with `str` for `inf`, `-inf`
    and `nan`;
  - runs `vo` (the `VO_BIN` env var, default `build/vo`; through `bin/vo` for other `ARCH`), parses the
    output back to doubles and compares bits;
  - reports the count per function and the first 10 mismatches (input, expected, got), and exits
    non-zero on any;
  - is part of `tests/run.sh` (so `make test`, `make ARCH=… test`, and the instrumented suite with
    `VO_BIN=build-instrumented/vo`).

### Implementation

- [ ] T016 [US3] Run T015 on x86-64 (native), ARM64 and RISC-V (qemu), and with
  `VO_BIN=build-instrumented/vo`. Every mismatch is fixed in vo's code (shim, `fma`, flags, input
  parsing), never in the vectors or the vendored files. Record the counts in `results.md` (SC-001,
  SC-002).
- [ ] T017 [P] [US3] Determinism: add `tests/run.sh --repeat 20 tests/cases/math` to the `gate-step`
  target in `Makefile` ("math determinism").

**Checkpoint**: T015 passes on three platforms and instrumented.

---

## Phase 6: User Story 4 — Find the functions without reading the whole reference (P2)

**Goal**: `vo doc std.math` and `vo doc std.math.NAME` give exact signatures, domains, edges and checked
examples.

**Independent Test**: the `doc` golden cases and `tests/doc_conformance.py`.

### Tests (first, failing)

- [ ] T018 [P] [US4] Golden cases in `tests/cases/doc/`:
  - `math_module`: `vo doc std.math`;
  - `math_item`: `vo doc std.math.pow` and `vo doc std.math.round`;
  - `math_json`: `vo doc std.math.log --json`.

  Update the `doc/all`-style cases that list every module.

### Implementation

- [ ] T019 [US4] Make every `std/math.vo` doc comment state the domain and the R0007 condition, and
  carry one example `EXPR is VALUE` that `tests/doc_conformance.py` and the doc-example runner execute
  (examples whose value has more than 15 digits use a rounded `str` form). Fix any conformance failure.

**Checkpoint**: T018 passes; `tests/doc_conformance.py` passes.

---

## Phase 7: Polish and the gate

- [ ] T020 [P] Reference (`docs/reference.md`): add the `std.math` row from `contracts/std-math.md` to
  the "Standard library and packages" table. Record the token growth (expected about +25; budget 15,000)
  in `results.md` (SC-004).
- [ ] T021 [P] Docs:
  - `site/docs/guide/standard-library.md`: a short math section (import, R0007 domains, correct
    rounding);
  - `docs/roadmap.md`: the "Math builtins" row is replaced by spec 020 in the Status table (**Done**),
    with a "Not done" list: `log2`, `log10`, `hypot`, `cbrt`, hyperbolic functions, and hardware FMA
    dispatch on x86-64;
  - `README.md`: the std list gains `std.math`, and the vendored-library sentence becomes "two vendored
    libraries (Mbed TLS for HTTPS, CORE-MATH for math)";
  - `docs/security.md`: CORE-MATH in the trust boundary, with how it is updated.
- [ ] T022 [P] Speed (SC-006): a `tests/speed/` case (or `tests/perf`) that calls `pow`, `sin` and
  `log` one million times in total and prints a checksum. It must run in under 1,000 ms on native
  x86-64; add it to `make speed-check`. If it fails, record the figure, and do the `ponytail:` upgrade
  (cpuid dispatch) only if it is over budget.
- [ ] T023 Gate in `Makefile`:
  - `make test` runs `math-selftest` (x86-64) and `tests/math_vectors.py`;
  - `make gate` includes the math determinism step (T017);
  - `make instrumented` builds the CORE-MATH objects with the sanitizers;
  - `tools/vendor_core_math.sh --verify` runs in `make lint`.
- [ ] T024 Full verification, recorded in `results.md`:
  - on x86-64: `make test`, `make gate` and `make instrumented`;
  - `make ARCH=aarch64 test` and `make ARCH=riscv64 test`;
  - the binary growth (`size build/vo` before and after);
  - fill SC-001 to SC-006 and any Deviations;
  - set the spec status to Implemented and mark every task [X].

---

## Dependencies & Execution Order

- **Phase 1** (T001-T004): T001 is done. T002, T003 and T004 in parallel; T004 reads the `.wc` files
  from a scratch clone, not from T003's output.
- **Phase 2** (T005-T010): T005, then T006 (needs T003). T007 after T006, which shows the undefined
  symbols. T008 after T007. T009 is independent of T005-T008. T010 needs T006, T007 and T009. Blocks
  every story.
- **US1** (T011-T012): after Phase 2. T011 first (fails), then T012.
- **US2** (T013-T014): after Phase 2 (T009); independent of US1, but shares `std/math.vo`, so after
  T012 or merged carefully.
- **US3** (T015-T017): T015 needs T004 and US1 (and US2 is not needed). T016 after T015.
- **US4** (T018-T019): after US1 and US2 (it documents all 15 functions).
- **Polish** (T020-T024): T020-T022 after the stories; T023 and T024 last.

## Parallel Examples

- Phase 1: T002, T003 and T004 together.
- Phase 2: T009 (builtin tables) alongside T005-T008 (build and libm).
- After Phase 2: T011 and T013 (cases) together; T015 (vector runner) as soon as T004 is done.
- Polish: T020, T021 and T022 together.

## Implementation Strategy

- **MVP**: Phases 1-3. `std.math`'s float functions, correctly rounded, with R0007 domains.
- **Next**: US2 (roundings) and US3 (the vectors on three platforms), the core promise.
- **Then**: US4 (doc completeness).
- **Last**: reference, docs, speed and the full gate on three platforms.
