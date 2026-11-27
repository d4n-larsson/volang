# Script interpreter

Write an interpreter for a small line-oriented scripting language. The program text is read from standard input. First the whole text is parsed; if it has a problem, report the first one and run nothing. Otherwise run it, printing what it prints, until it ends or hits a runtime error.

## Program structure

Split the program into at least three source files: the tokenizer (splitting a line into tokens), the parser (expressions, statements, block structure and the parse checks), and the evaluator (values, scopes, function calls and the runtime rules) each in their own file, plus the entry file that reads standard input and drives them.

## Lines and tokens

The input is split into lines at `\n`; lines are numbered from 1 and every line counts, including empty ones. Each line holds at most one statement. Lines contain only printable ASCII characters and spaces.

A line is split into tokens, left to right. Spaces separate tokens and are otherwise ignored. A `#` that is not inside a string starts a comment, which runs to the end of the line. A line without tokens is blank and ignored. The tokens are:

- an *integer literal*: a maximal run of digits `0`-`9` (leading zeros are allowed: `007` is 7); more than 18 digits is a syntax error;
- a *string literal*: `"`, any characters other than `"`, and a closing `"` on the same line (no escapes); a missing closing `"` is a syntax error;
- a *word*: a maximal run of the characters `a`-`z`, `0`-`9` and `_`, starting with a letter or `_`. The words `let set print if elif else while end func return break continue and or not true false` are *keywords*; every other word is a *name*;
- an *operator*: `==` `!=` `<=` `>=` `<` `>` `=` `+` `-` `*` `/` `%` `(` `)` `,` (two-character operators are tried first).

Any other character outside a string or comment (an uppercase letter, `!` alone, `.`, `;`, ...) is a syntax error.

## Expressions

From loosest to tightest binding:

| Level | Form | Notes |
|---|---|---|
| 1 | `a or b` | left to right |
| 2 | `a and b` | left to right |
| 3 | `not a` | `not a == b` means `not (a == b)`; `not not a` is allowed |
| 4 | `a == b`, `a != b`, `a < b`, `a <= b`, `a > b`, `a >= b` | at most one comparison: `a < b < c` is a syntax error |
| 5 | `a + b`, `a - b` | left to right |
| 6 | `a * b`, `a / b`, `a % b` | left to right |
| 7 | `-a` | unary minus; `- -a` is allowed |
| 8 | literal, `true`, `false`, name, call `name(e, e, ...)` (zero or more arguments), `(e)` | |

A comparison's operands are level-5 expressions and the operand of `not` is a level-3 expression, so `1 == not x` is a syntax error.

## Statements

The first token of a line decides its statement. Each form must use the whole line; anything else (missing parts, extra tokens, a keyword where a name is needed) is a syntax error.

| Form | Meaning |
|---|---|
| `let NAME = EXPR` | declare a variable |
| `set NAME = EXPR` | assign to an existing variable |
| `print EXPR, EXPR, ...` | one or more expressions |
| `if EXPR`, `elif EXPR`, `else`, `end` | `if` with any number of `elif` parts and at most one `else`, after the `elif` parts, closed by `end` |
| `while EXPR` ... `end` | loop |
| `break`, `continue` | leave the innermost loop, or go to its next condition check |
| `func NAME(NAME, NAME, ...)` ... `end` | define a function with zero or more parameters |
| `return` or `return EXPR` | leave the function |
| `NAME(...)` | a call statement: the line is exactly one call expression; its value is discarded |

## Parse checks

Lines are checked in order and parsing stops at the first problem; print `parse error at line N: <message>` and nothing else. A line is first checked for its syntax (message `syntax error`), then for its place in the block structure:

- `end` with no open block: `unexpected end`.
- `elif` or `else` when the innermost open block is not an `if`, or is an `if` that already had its `else`: `unexpected elif` or `unexpected else`.
- `func` while any block is open: `nested function`; then a function name used by a function defined on an earlier line, or equal to a builtin (`len`, `str`, `int`, `substr`): `duplicate function NAME`; then a parameter name that repeats an earlier parameter of the same function: `duplicate parameter NAME`, for the first such parameter from left to right (`func f(a, b, b, a)` reports `b`).
- `return` outside a function body: `return outside function`.
- `break` or `continue` not inside a `while` body (in the same function body, if any): `break outside loop` or `continue outside loop`.

If the input ends with blocks still open, the error is `missing end` at the line of the innermost open block's `if`, `while` or `func` line.

## Values and operators

Values are integers, strings and booleans (`true`, `false`). Every integer must lie between -1000000000000000000 and 1000000000000000000 (minus and plus 10^18) inclusive; a result of `+`, `-`, `*` or unary `-` outside that range is the error `overflow`.

- `+` adds two integers or joins two strings.
- `-`, `*`, `/`, `%` and unary `-` take integers. `a / b` truncates toward zero (`-7 / 2` is `-3`); `a % b` is `a - b * (a / b)` (`-7 % 2` is `-1`, `7 % -2` is `1`). A zero `b` is the error `division by zero`.
- `==` and `!=` compare any two values: values of different types are never equal; strings are equal when their characters are.
- `<`, `<=`, `>`, `>=` compare two integers or two strings (strings by byte order: `"B" < "a"`, `"ab" < "b"`, `"a" < "ab"`).
- `and`, `or` and `not` take booleans. `and` evaluates its left operand; if that is `false` the result is `false` and the right operand is not evaluated; otherwise the result is the right operand (which must be a boolean). `or` likewise stops at `true`.
- Any other operand type is the error `type error`. Both operands of a binary operator (other than `and` and `or`) are evaluated, left first, before its types are checked; then the zero check, then the range check.

The conditions of `if`, `elif` and `while` must be booleans, else `type error`.

## Variables and scopes

A *scope* holds variables. The *global scope* exists for the whole run. Each time a function is called, a new *function scope* is created holding its parameters. Each time the body of an `if`, `elif` or `else` part is entered, and each time the body of a `while` loop is entered for one iteration, a new *block scope* is created; it is discarded when that body is left.

The *visible scopes* at a point are, from innermost to outermost: the block scopes currently entered, then (inside a function) that call's function scope, then the global scope. Code in a function does not see the block scopes or function scopes of its caller.

- `let NAME = EXPR` evaluates the expression, then declares `NAME` in the innermost visible scope. If that scope already has `NAME`, the error is `already declared NAME`. A variable in an outer scope may be shadowed.
- `set NAME = EXPR` evaluates the expression, then assigns it to `NAME` in the innermost visible scope that has it, else the error is `undefined NAME`.
- Reading a name that no visible scope has is the error `undefined NAME`.

## Functions and calls

Functions are defined by `func` blocks, which never execute: every function in the text can be called from anywhere, also before its definition line. Variables and functions are separate: a variable may have the same name as a function.

A call `NAME(args)` first finds the function: if `NAME` is neither a defined function nor a builtin, the error is `unknown function NAME`. Then the arguments are evaluated left to right; then a wrong number of arguments is the error `wrong argument count`. For a defined function, if 25 function calls are already active (builtins do not count), the error is `call depth exceeded`; otherwise its body runs with its parameters set to the arguments. `return EXPR` makes the call's value the expression's value; `return` alone, or reaching the end of the body, makes it `0`.

Builtins:

- `len(s)`: the number of characters of string `s`.
- `str(v)`: `v` as a string, as `print` shows it.
- `int(v)`: an integer is returned unchanged; a string that is an optional `-` followed by 1 to 18 digits gives that integer (`int("-012")` is -12), any other string is the error `bad int`; a boolean is `type error`.
- `substr(s, start, count)`: the `count` characters of `s` starting at index `start` (0-based). Wrong argument types are `type error`; unless `start >= 0`, `count >= 0` and `start + count <= len(s)`, the error is `out of range`.

## Running

The statements outside function bodies run from top to bottom. `print` evaluates all its expressions left to right, then prints their values on one line separated by single spaces: integers in decimal with a leading `-` when negative, strings as they are, booleans as `true` or `false`. `if` runs the body of the first part whose condition is `true` (evaluating conditions in order and stopping there), or the `else` body if there is one. `while` evaluates its condition before every iteration and stops when it is `false`; `break` leaves the innermost loop, `continue` goes on to its next condition check.

**Steps.** Each executed statement counts one step, counted before it runs: `let`, `set`, `print`, a call statement, `return`, `break` and `continue` each time they run; `if`, `elif` and `while` each time their condition is evaluated. `else`, `end` and `func` lines never count. The 10001st step is the error `step limit exceeded` at that statement's line (it does not run).

**Runtime errors.** A runtime error prints `error at line N: <message>`, where `N` is the line of the statement being executed when it happened (inside a function, the statement in that function's body; for a condition, the `if`, `elif` or `while` line), and stops the program. Lines printed earlier stay. Every output line ends with a newline.
