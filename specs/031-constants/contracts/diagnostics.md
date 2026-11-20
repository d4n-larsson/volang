# Contract: Diagnostics (spec 031)

Every code has a `docs/errors.md` entry (a broken and a fixed example), a `vo explain` text and at least
one golden case in `tests/cases/const/`. Exit status 65 for errors, 0 for the warning.

## New codes

| Code | Name | Span | Message | Fix |
|---|---|---|---|---|
| E0122 | `const_in_function` | the word `const` | "Constants are declared at the top level of a file; inside a function use `let`." | replaces `const` with `let` |
| E0212 | `const_not_constant` | the offending node | "A constant's value is made of literals, constants and operators: it cannot call `NAME`." / "... : `NAME` is not a constant." / "... : this is not allowed here." (anything else: `new`, a list, an index, a field, `try`, an anonymous function) | none |
| E0213 | `const_cycle` | the name of the first constant of the cycle, in file order | "`a` is defined by itself: a → b → a." Related: each other constant of the cycle ("`b` uses `a` here."). | none |
| E0332 | `const_type` | the stated type, or the expression when no type is stated | "A constant is an `int`, `float`, `bool` or `str`, found `TYPE`." | none |
| E0333 | `const_arithmetic` | the operator's expression | "This overflows `int`." / "This divides by zero." | none |
| W0202 | `unused_constant` | the name | "`NAME` is never used." | removes the declaration and the comment above it |

E0333 is also reported for a constant expression written directly in a literal position (a scope, an
annotation argument). In an ordinary expression nothing is reported: the run stops with R0001 or R0002 as
today.

## Reused codes, with constants

| Code | When | Message / fix |
|---|---|---|
| E0208 | `set NAME = ...;` on a constant | "`NAME` is a constant and cannot be changed." No fix. |
| E0202 | a constant with the name of another item, an import or a builtin | as for other items, related note at the first definition |
| E0203 | a local, parameter, `for` variable, case binding or type parameter with the name of a visible constant | as today, related note at the constant |
| E0207 | a constant not in `snake_case` | fix: the converted name, at the declaration and at each use |
| E0204 | a name a sibling file declares as an exported constant | fix: the `import` line |
| E0209 | importing a private constant | fix: `export` at its declaration |
| E0301 | a stated type that is not the value's; a constant of the wrong type in an expression or a literal position | `expected` and `found` set |
| E0308 | operands an operator does not take, in a constant's value | as in any expression |
| E0506 | a scope whose value is empty, absolute or has `..` | at the scope; when it came from a constant the message ends with "(`NAME` is `"VALUE"`)" |
| E0329 | an annotation argument of the wrong type or count | unchanged |

## Reworded messages

Where the argument is neither a literal nor a constant expression:

| Code | New wording |
|---|---|
| E0315 | "`format` needs its template as a string literal or a constant, so the placeholders can be checked." |
| E0321 | "`regex` needs its pattern as a string literal or a constant; use `parse_regex` for a pattern from data." |
| E0324 | "`sql` needs its statement as a string literal or a constant." |
| E0330 | (route pattern not readable) "... a string literal or a constant." |
| E0506 | (a scope that is a name which is not a constant) "A scope is a string literal or a constant, found `NAME`." |

## Problems inside a constant's value at a literal position

When the template, pattern, statement, route or scope came from a constant (or a folded expression), the
diagnostic keeps its code, spans the whole use, quotes the value in the message, and has no fix:

```text
main.vo:9:18: error[E0316]: `format` has 2 placeholders but 1 argument (`row` is "{} of {}").
```

A written literal keeps today's exact spans and fixes.

## Effects review

E0504 and W0502 are reported as today from the values. When the changed scope was written with a constant,
the diagnostic has one more related note at the constant's declaration: "`out_dir` is `"./"` here."

## Cascades

A constant whose value is in error (E0212, E0213, E0332, E0333, E0301 at its declaration) has the error
type: its uses report nothing, in expressions and in literal positions alike.
