# Results: Bits (spec 043)

Status: implemented. `make test` passed (run twice: the first run failed on the two module listings below). The gate was not run.

## SC-001: the operations agree with a reference

`python3 tests/bits.py --group ops`: 10,000 pairs (0, 1, -1, the extreme ints and powers of two
over-represented; counts 0 to 63) through the twelve operations, compared with Python computed with
64-bit masking: 0 differences. Golden cases `run/bits`, `run/bits_bad_shift*`, `run/bits_bad_rotate`,
`run/bits_overflow_unchanged`, `check/bits_pure`.

## SC-002: real algorithms

`--group algorithms`: `tests/bits/crc32.vo`, `fnv1a.vo`, `sha256.vo` on 1,000 random inputs each
(lengths 0 to 300, the SHA-256 block edges included) against `zlib.crc32`, a Python FNV-1a and
`hashlib.sha256`; `varint.vo` on 1,000 LEB128 sequences. 0 differences. The SHA-256 program also
compares each digest with `std.crypto.sha256` inside vo.

## SC-003: speed

| Program | 1 MiB, x86-64 (best of 3) | Bound |
|---|---|---|
| CRC-32, table-driven | 47 ms | 500 ms |
| FNV-1a, 64-bit | 23 ms | 500 ms |

T015 (`OPC_BITS`) was not needed and was not done: each operation is a call of a library function over
one primitive. The bound is not enforced when `CI` is set or on the emulated architectures (the times
are printed there).

## Three architectures (T020)

Golden areas `lex` (18), `fmt` (31), `run` (111), `check` (62), `const` (29) and `tests/bits.py` pass with
`build-aarch64/vo` and `build-riscv64/vo` under qemu. Times there, emulated, not bounded: CRC-32 531 ms
(ARM64) and 670 ms (RISC-V); FNV-1a 274 ms and 337 ms.

## SC-004: literals

`--group literals`: 10,000 random hex and binary literals of every length (1 to 16 hex digits, 1 to 64
binary digits, both letter cases) print Python's value read as a signed 64-bit pattern. Golden cases
`run/based_literals`, `lex/based_bad`, `fmt/based_literals`, `run/parse_hex`.

## SC-005: the reference

| Point | Tokens (words x 1.3) |
|---|---|
| Before | 14,986 |
| After the trim (T003) | 14,803 |
| At the end (T018) | 14,848 (bound 14,900) |

The trim aimed at 14,740 and stops at 14,803: rewording alone, with no rule, code, example or table row
removed, honestly gives 141 words. Going further would mean dropping sentences.

The table below was shown to the project owner and accepted before T018.

## Existing cases that changed

- `tests/cases/doc/all` and `all_json`: the module listing gains the `std.bits` line. Nothing else.

## Deviations

- The trim stops at 14,803, not 14,740 (above).
- E0101 is "Fixable: sometimes" in the catalog: the six bit operators have no fix, so a lone `&`, `|`,
  `^` or `~` loses the removal fix it had. After such an error the parser reads on as if `+` (unary `-`
  for `~`) had been written, which keeps it to one error each.
- `lex/based_bad` has no `_json` twin: lex cases already run `check --json`.
- T002's stub step was skipped: the primitive was written whole.
- The speed bound is not enforced when `CI` is set.

## Not verified

- The gate (`make gate`) was not run.
- Speed on real ARM64 or RISC-V hardware (only under qemu).
- The grammars in real editors (only through `tests/grammars.py`).

### Reference sentences shortened

| Before | After | Words saved |
|---|---|---|
| A `#` after code on the same line is an error (E0101). There are no block comments. | A `#` after code is E0101. No block comments. | 8 |
| - There are no trailing commas anywhere (E0113). | - No trailing commas anywhere (E0113). | 2 |
| There are no implicit conversions, not even `int` to `float` or anything to `str`. Convert explicitly with | No implicit conversions, not even `int` to `float` or anything to `str`: convert with | 3 |
| A variable's type is inferred from its initializer. Add `: type` when you want to state it: | A variable's type is inferred from its initializer; `: type` states it: | 5 |
| so numbers must go through `str(...)`: | so numbers go through `str(...)`: | 1 |
| An item can use items declared later in the file. There are no global variables, no methods, no classes and no nested functions. | An item can use items declared later. No global variables, methods, classes or nested functions. | 8 |
| A function without a return type may use `return;`. There is no overloading, no default argument and no variadic function. | A function without one may use `return;`. No overloading, default arguments or variadic functions. | 6 |
| A record is a named set of fields. Each field ends with `;`. | A record is a named set of fields, each ending with `;`. | 1 |
| in any order. A record without fields is built with `new Name {}`. | in any order; without fields, `new Name {}`. | 5 |
| (use a record to carry several). Each case ends with `;`. | (use a record for several) and ends with `;`. | 2 |
| or, for a case without a value, `Name.case` | or, without a value, `Name.case` | 3 |
| The checker works it out once, with the same rules as at run time, so | The checker computes it once, by the run-time rules, so | 5 |
| It is usable wherever a value of its type is, and also where a literal is required: | It works wherever a value of its type does, and where a literal is required: | 2 |
| so it always returns the same result for the same arguments. | so the same arguments always give the same result. | 2 |
| (E0501, with a fix that adds exactly the missing effects). An effect nothing needs is W0501, with a fix that removes it. | (E0501; its fix adds exactly the missing effects). An effect nothing needs is W0501; its fix removes it. | 4 |
| (relative to the directory the program starts in, e.g. `"out/"`; `"./"` is that directory itself) | (relative to where the program starts, e.g. `"out/"`; `"./"` is that directory) | 3 |
| is E0504, with notes on the calls that brought it in. | is E0504, noting the calls that brought it in. | 2 |
| changes the record, and it prints exactly the changes it writes, | changes the record, printing exactly the changes it writes, | 2 |
| Statements that end with a block (`if`, `while`, `for`, `match`) take no `;` after `}`. Braces are always required, even around a single statement. | Statements ending with a block (`if`, `while`, `for`, `match`) take no `;` after `}`. Braces are required, even around one statement. | 3 |
| A local that is never read is warning W0201. | A local never read is W0201. | 3 |
| drop a value: calling `len(s);` alone is E0405 | drop a value: `len(s);` alone is E0405 | 1 |
| Parentheses around a condition are allowed but not needed. | Parentheses around a condition are optional. | 3 |
| repeats n times without naming the counter. The `..` form exists only in `for`. To loop over a map, use `for k in keys(m)`. | repeats n times without a counter. `..` exists only in `for`. Loop over a map with `for k in keys(m)`. | 4 |
| (E0401 if one is missing, with a fix that inserts it; E0402 if repeated). There is no default arm. | (E0401 if one is missing, whose fix inserts it; E0402 if repeated). No default arm. | 4 |
| Dividing an `int` by zero is runtime error R0002. | Dividing an `int` by zero is R0002. | 2 |
| or two `str` (byte order). They do not apply to `bool`. | or two `str` (byte order), not `bool`. | 4 |
| There are no methods: write `len(xs)`, not `xs.len()`. | No methods: write `len(xs)`, not `xs.len()`. | 2 |
| come in the order of `xs`, whatever order the tasks finish in. | come in the order of `xs`, however the tasks finish. | 2 |
| returns `result[T]`, which is either `ok(value)` or `err(message)`; the error is always a `str`. A function that can fail but has no value returns `result[void]` | returns `result[T]`, either `ok(value)` or `err(message)`; the error is always a `str`. One with no value returns `result[void]` | 7 |
| can never be ignored: calling `f();` alone or `let _ = f();` when `f` returns a result is E0404. | can never be ignored: `f();` alone or `let _ = f();` when `f` returns one is E0404. | 2 |
| **Maps**: there is no map literal. Create with `new map[str, int]`. | **Maps**: no map literal: create with `new map[str, int]`. | 2 |
| `m[k]` reads and stops the program with R0004 if the key is missing, so check `has(m, k)` first. | `m[k]` reads, stopping with R0004 if the key is missing: check `has(m, k)` first. | 4 |
| stands for code you have not written yet. | stands for code not written yet. | 2 |
| and `expect_eq(actual, expected)`. These two builtins work only inside tests. | and `expect_eq(actual, expected)`, which work only inside tests. | 2 |
| Test names must be unique in a file. Tests cannot use `try` (they do not return a result): | Test names are unique in a file. Tests cannot use `try` (they return no result): | 3 |
| discards an input that does not apply. It works only in a `prop` (E0604). | discards an input that does not apply, only in a `prop` (E0604). | 2 |
| relative to the directory of the importing file, | relative to the importing file's directory, | 2 |
| the others are private to their file, and importing one | the others are private, and importing one | 3 |
| Using a name defined in a sibling file without importing it is E0204, | A sibling file's name used without its import is E0204, | 3 |
| through the file that contains `main`: | through the file containing `main`: | 1 |
| `run` checks first and runs only if there are no errors. | `run` checks first and runs only without errors. | 3 |
| Memory: values that can no longer be reached are reclaimed automatically. | Memory: unreachable values are reclaimed automatically. | 5 |
| changes nothing and lists the files that are not canonical. | changes nothing and lists the non-canonical files. | 3 |
| Canonical formatting, as produced by `vo fmt`: | Canonical formatting (`vo fmt`): | 3 |
| the fix. The ranges are: E01xx | the fix. Ranges: E01xx | 2 |

45 sentences, 141 words, about 183 tokens.
