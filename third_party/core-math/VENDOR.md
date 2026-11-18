# CORE-MATH (vendored)

Third-party code built into vo under constitution 1.4.1 ("Implementation Constraints").

| | |
|---|---|
| Library | CORE-MATH, correctly rounded binary64 functions (`cr_sin` ... `cr_pow`) |
| Source | https://gitlab.inria.fr/core-math/core-math |
| Commit | `1851882b0f85ed581af9185cfa020992cec9e7a1` (the project publishes no releases) |
| SHA-256 | of each file as upstream has it, below |
| License | MIT, see LICENSE |
| Kept | the files below, 14266 lines of C |
| Modified | one patch, below |

vo calls the functions from `src/math.c`, which also supplies the C math functions the compiler
emits calls to (`fma` on x86-64, `round`, ...). Other declarations are in `third_party/shim/`.

## Files

- `LICENSE` 81830875362f4d477637de58eecd6d458db108cc90c40904fca3694807b86de3
- `src/binary64/acos/acos.c` ecd6173837214367f0140c038c615a4f37d27795c0cc74e16bdaa59f7df8cff1
- `src/binary64/asin/asin.c` d9cc7c44922ac3caef5a25d0eb3d9e1e2aa94a0fea761737416d68d6f3a2a8e9
- `src/binary64/atan/atan.c` 2d62a4d780f233fd0460b8e6076880fcb9d2f55280ead5799180983b6b77c863
- `src/binary64/atan2/atan2.c` 404b0db90e536f8248101544d0fcad9141fd76b2a6cc3d189dd80dbfc865bd91
- `src/binary64/atan2/tint.h` 41264d54812a8ce2b3f8ca4c8c033094df7cbf2d214da1c7555783328171b921
- `src/binary64/cos/cos.c` 972c2d35266ffa872d7e5da26fcd2a5aa22b885a01dca8febbfaf289288e75e1
- `src/binary64/exp/exp.c` 65af7f572ce97327b63700dd76bd6bf753c087fe326c83c7dbbd674ab168a84d
- `src/binary64/log/dint.h` 9eb5878abe785c3ef4c232255ed6d9d71a80ea70252f983a38756f10abc7e515
- `src/binary64/log/log.c` 73f778e1ca7656fa473fc48e934ac5d359d4fd7d3c2d8ebcaa2edd5ff09b3eb5
- `src/binary64/pow/dint.h` c62a9788d872c314c743a821602d6819e5c13113b70f6d4a5d0076695dc413ef
- `src/binary64/pow/pow.c` b14c65a1d1367f49a0d3e00e65370c3e9dd0fd004fe00e30cdad2a9887954562
- `src/binary64/pow/pow.h` 8e28930b514f1f439011cb201c738c6b8940f321f03f4679362e7f6b7893fd6a
- `src/binary64/pow/qint.h` 32de9c10388056e2294339e3302c3695cdf4abc87ca790ad48917d9794bfb6e4
- `src/binary64/sin/sin.c` 336a684326c7ac4543fe2493938dc0c58c63f9f2a7d803610be97e1db781251c
- `src/binary64/tan/tan.c` dd662b7af88a86d85e01b88c57f9df4b6019ddfd02120ca381b5ba42283987ae

## C files

- `src/binary64/acos/acos.c`
- `src/binary64/asin/asin.c`
- `src/binary64/atan/atan.c`
- `src/binary64/atan2/atan2.c`
- `src/binary64/cos/cos.c`
- `src/binary64/exp/exp.c`
- `src/binary64/log/log.c`
- `src/binary64/pow/pow.c`
- `src/binary64/sin/sin.c`
- `src/binary64/tan/tan.c`

## Patches

- `patches/0001-no-inline-asm.patch`: Replace the empty register barriers asm("":"=x"(r):"0"(x)) on x86-64 with _mm_set_sd(x),

## Updating

    tools/vendor_core_math.sh NEW_COMMIT
    tools/vendor_core_math.sh --verify

Then review the diff of this directory, check that the patches still apply and are still needed,
and run the full gate (the vectors in `tests/math/` must still match bit for bit). A fix for a
wrong result is applied in the spec cycle in which it appears.
