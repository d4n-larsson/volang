# Data Model: Application Structure Standards (spec 027)

All entities live in the checker during `vo check`; nothing is stored.

## Thresholds (fixed, FR-008)

| Name | Value | Rule |
|---|---|---|
| R | 15 effect-free statements | W0701 |
| F | 400 code lines | W0702 |
| S | 50 statements | W0703 |
| D | 6 sibling statements | W0704 |

## Program file

The entry and the files it imports (not std, not packages). Rules W0701–W0706 report only in these
files; duplicates are compared only within one program.

- **Code lines**: lines that are not blank and not only a comment.

## Statement

A node in a block of a function, test or property body (at every depth; the bodies of anonymous
functions included). A compound statement (`if`, `while`, `for`, `match`) is one statement; its nested
blocks contain further statements.

- **Requires effects** (bit): its own expressions (the condition, iterable or scrutinee of a compound
  statement; the whole statement otherwise) contain a call whose required effects are not empty: an
  effectful builtin, a function with declared effects, an effect-generic function whose effects at that
  call are not empty, or a function value whose type has effects. `todo(...)` and creating an anonymous
  function do not require effects.
- **Effect-free**: not requires-effects.

## Effect-free stretch

A maximal sequence of consecutive effect-free sibling statements (descending into effect-free compound
statements). The three longest of an effectful function are W0701's related locations; ties by position.

## Duplicate run

A sequence of at least D consecutive sibling statements in a non-test function. Its **normal form** is its
token sequence with each local variable and parameter replaced by its position of first appearance in the
run and each literal by its kind (`int`, `float`, `str`, `bool`); everything else as written. Two runs with
the same normal form, in different blocks, are duplicates. Overlapping duplicate windows merge into maximal runs.

- **Effect-only copy**: two whole function bodies that are duplicates and whose signatures are equal after
  erasing `uses` clauses and the effect arguments of parameter and return types.

## Accepted warning (`@allow`)

| Field | Rule |
|---|---|
| code | a string literal, one of W0701–W0705 (else E0121) |
| reason | a non-empty string literal (else E0121) |
| declaration | the `fn`, `test` or `prop` right below (else E0117); for W0702, the file's first `fn`, `test` or `prop` |

- **Used**: the declaration (or file, for W0702) has that warning. **Unused** → W0706 with a fix removing
  the annotation's line.
- Several `@allow` lines may stand above one declaration, one per code; with `@spec` lines, in any order.

## Check summary

`E errors, W warnings` gains ` (A accepted)` when A > 0; the JSON summary gains `"accepted": A` when
A > 0. Accepted warnings are not counted in W.
