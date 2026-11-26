# Contract: Syntax

## Grammar

```text
lambda     = "fn" "(" [ name { "," name } ] ")" ( block | "=>" expr )
name       = identifier | "_"
```

- `=>` is a token; it appears only between an anonymous function's parameter list and its body.
- `=> expr` is the only form of a body that is a single `return expr;`. A block is used for every other
  body, including one that returns nothing (`fn(s) { print(s); }`).
- Function **types** are unchanged: `fn(int) -> int uses console`, written in full wherever a type goes.
- Named functions are unchanged: every parameter type and the return type are written.
- Accepted for migration only (each is E0342): `fn(x: int)`, `fn(x) -> int`.

## Where an anonymous function may appear

Any expression position with an expected function type: a call argument, a typed `let`/`var`, `return`,
a `set` target, a `new` field, a list element of a typed list, a variant or `ok`/`err` payload. Elsewhere
it is E0343.

## Inference order (reference text)

> In a call, the arguments that are not anonymous functions fix the type parameters first; then each
> anonymous function, left to right, gets its parameter types from the call, and its return type, where
> the call leaves it open (`U` of `map`), from its body.

## Canonical form (`vo fmt`)

| Input | Output |
|---|---|
| `fn(p) { return p.age; }` | `fn(p) => p.age` |
| `fn(p)=>p.age` | `fn(p) => p.age` |
| `fn(a,b) {` ... (two or more statements) | `fn(a, b) {` block as today |
| a `=> expr` lambda past 100 columns | broken inside `expr` as any expression |

A block whose only statement is `return expr;` but that holds a comment is left as a block (the comment
has no place in `=> expr`); E0413 is not reported for it.
