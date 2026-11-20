# Contract: Tools (spec 031)

## vo query

```console
$ vo query refs main.vo max_items
config.vo:2:14 declaration: export const max_items = 100;
main.vo:1:15 import: import config.max_items;
main.vo:8:19 use in add: if len(xs) >= max_items {
main.vo:4:14 use in Note: @size(1, max_items)
4 references
$ vo query type-at main.vo main.vo:8:19
main.vo:8:19 max_items: int = 100 (declared at config.vo:2:14)
    The most notes a list may hold.
```

- A TARGET may be a constant's name or a position on its declaration or a use; a name declared in two
  files is T0005, as for functions.
- ROLE for a use of a constant is `use`; in a scope or an annotation the enclosing function, record or
  set is named after `in`.
- `type-at --json`: the existing `type_at` record with one optional field more, `"value"`, the value in vo
  syntax (`"100"`, `"0.25"`, `"\"out/\""`, `"true"`), present for constants only.
- `vo query callers ENTRY max_items`: the toolchain error for a target that is not a function, with the
  existing code.

## vo edit

`vo edit rename ENTRY max_items item_limit` changes the declaration, the imports and every use, in
expressions, scopes and annotations; the result is checked first (T0006 on a clash or a changed meaning);
all files are written or none. `add-field` is not affected.

## vo impact

A diff that changes a line of a constant's declaration lists every function, test and property that
references the constant, directly or through constants that use it, with the reason `uses max_items`.
Functions are then followed through calls as today.

## vo doc

```console
$ vo doc main.vo max_items
const max_items: int = 100
    The most notes a list may hold.
```

`vo doc ENTRY` and `vo doc NAME.module` list exported constants after effect sets. JSON: a `doc` record
with `"item":"const"`, `"signature":"const max_items: int = 100"`, `"description"`.

## vo test

- A failing `expect` or `expect_eq` lists a constant among its operands with its value:

  ```text
  FAIL limit holds (0ms)
    main.vo:14:5: `expect` failed: `len(xs) <= max_items` is false.
      len(xs):   101
      max_items: 100
  ```

- `--mutate` does not change a constant's value where it is used, and does not treat a constant's
  declaration as a mutation site.
- W0603 (a check that cannot depend on the program) applies to a check made only of literals and
  constants.

## vo effects, the effects record

Scopes are shown and recorded as values. A program whose literals are replaced by constants of the same
value has a byte-identical record and `vo effects` output.

## vo fmt

See [syntax.md](syntax.md), "Canonical form".
