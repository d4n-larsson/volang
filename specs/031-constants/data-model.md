# Data Model: Constants (spec 031)

## Constant (item)

| Field | Meaning |
|---|---|
| name | `snake_case` identifier, unique among the names visible in the file |
| exported | `export const ...` |
| stated type | optional: `int`, `float`, `bool` or `str` |
| expression | a constant expression, as written (what `vo fmt` and diagnostics show) |
| value | the literal the expression folds to, worked out once; or the error value |
| state | not started, in progress, done (cycle detection, research R4) |
| documentation | the comment lines directly above the item |

Rules: the type is the value's (stated type checked, E0301); only the four types (E0332); the value is
made of literals, constants and operators (E0212); no cycle (E0213); no overflow or division by zero
(E0333); never assigned (E0208); a private constant must be referenced (W0202).

In the tree: `N_CONST` (A name node, B type node or 0, C expression, folded in place; flags `NF_EXPORT`);
symbol kind `SYM_CONST` (its declaration is the `N_CONST`, its type one of the four builtin types).

## Constant expression

A literal; a constant's name; `( e )`; `-e`, `not e`; `e op e` for `+ - * / %`, `== != < <= > >=`, `and`,
`or`, with the operand types ordinary expressions require. Its value is a literal of type `int`, `float`,
`bool` or `str`.

## Folded use

A use of a constant, after checking: a literal node (`N_INT`, `N_FLOAT`, `N_STR`, `N_BOOL`) with the
constant's value, the use's own span, the flag `NF_CONST`, and the constant's declaration. Written
literals have none of the last two. Consumers that must tell them apart: reports with operand values
(show it), mutation testing (skip it), position-exact diagnostics inside template text (whole-use span).

## Literal position

A place where the checker reads a value: an effect scope; an argument of `@size`, `@range`, `@pattern`,
`@default`, `@header`; the template of `format`; the pattern of `regex`; the statement of `sql`; the
pattern of a `std.web` route; the target argument compared with the caller's scopes (E0503). Each takes
a constant expression of the type it needs. `@spec` and `@allow` are not literal positions in this sense.

## Unchanged

The effects record (values only), `EffSet`, bytecode and the instruction set, the JSON schema version,
recordings and their fingerprints for programs that declare no constant.
