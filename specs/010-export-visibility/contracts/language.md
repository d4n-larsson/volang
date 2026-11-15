# Contract: `export`

## Syntax

```text
item := ["export"] ("fn" ... | "record" ... | "variant" ...) | "test" ... | "prop" ... | "import" ...
```

`vo fmt` prints `export` and one space before the item keyword. `export` is a keyword: it cannot be a name.

## Rules

1. An item declared with `export` can be imported by any file; any other `fn`, `record` or `variant` is
   private: every item and test of its own file can use it, no other file can import it.
2. An exported record exposes all its fields; an exported variant all its cases (as today).
3. An exported function's parameter and result types, an exported record's field types and an exported
   variant's case types must not name a private record or variant of the same file.

## Diagnostics

| Code | At | Message | Fix |
|---|---|---|---|
| E0209 | the imported name | ``` `NAME` is not exported by `FILE`. ``` | insert `export ` before the declaration in FILE |
| E0209 | the imported name (library) | ``` `NAME` is internal to `std.MODULE`. ``` | none |
| E0209 | the imported name (package) | ``` `NAME` is internal to package `PKG`. ``` | none |
| E0210 | the type name | ``` `export ITEM` uses `TYPE`, which is not exported. ``` | insert `export ` before TYPE's declaration |
| (syntax) | the token after `export` | ``Expected `fn`, `record` or `variant` after `export`, found ...`` | none |

When several imports (or exported items) need the same declaration exported, one diagnostic carries the
fix and the other places are its related notes ("also needed here").
