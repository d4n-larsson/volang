# Contract: Structure Diagnostics (spec 027)

Every diagnostic has the usual fields (code, name, severity, file, start, end, text, message, fixes,
related). Spans and messages below; `$N` are filled in. Each message is one sentence of finding and one of
instruction.

## W0701 effectful_logic

- **Span**: the function's name.
- **Message**: "`$name` has $n statements that use no effect: move that logic into pure functions,
  which tests can call without effects, and keep `$name` to the effects."
- **Related**: the three longest stretches of effect-free statements, longest first (ties by position),
  each spanning its first to last statement, with the message "$k statements that use no effect".

## W0702 file_too_long

- **Span**: line 1, column 1 (zero-width, as W0504).
- **Message**: "This file has $n code lines, more than 400: split it into modules, one concept each, and
  import what `main.vo` needs."
- **Related**: none.

## W0703 function_too_long

- **Span**: the name of the function (for a test or property, its `test`/`prop` keyword and name string,
  as W0602).
- **Message**: "`$name` has $n statements, more than 50: split it into functions that each do one thing."

## W0704 duplicate_code

- **Span**: the later run, its first to its last statement.
- **Message** (general): "These $n statements repeat $file:$line: make one function and pass what
  differs as parameters."
- **Message** (effect-only copy, on the later function's name): "`$later` repeats `$earlier` except for
  its effects: make one function with an effect parameter, `[E: effects]`, and use it for both."
- **Related**: the earliest equal run (or function), "the first copy".
- With three or more copies, every later copy is reported, each pointing at the first.

## W0705 test_elsewhere

- **Span**: the test's or property's keyword and name.
- **Message**: "This test calls only functions of `$file`: move it into `$file`, next to the code it
  tests (`vo test` runs it from there)."

## W0706 unused_allow

- **Span**: the annotation.
- **Message**: "`@allow(\"$code\", ...)` silences nothing here: remove it."
- **Fix**: remove the annotation's line (including its newline).

## E0121 invalid_allow

- **Span**: the annotation (for a bad argument, that argument).
- **Messages**:
  - "`@allow` takes a structure warning code and a reason: `@allow(\"W0703\", \"why it stays\")`."
  - "`$code` is not a structure warning: `@allow` accepts W0701 to W0705."
  - "The reason must say why the warning stays; it cannot be empty."

## Ordering and determinism

Structure warnings are sorted with all other diagnostics (file, position, code). Related locations are
in the order above. The same program gives the same diagnostics on every platform and run.
