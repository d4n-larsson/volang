# Contract: Tools

## `vo query`, `vo edit`, `vo impact`

TARGET forms added: `module.name`, `module.Record.field`, `module.Variant.case`. A lowercase first
segment is a module name bound by an import in the program's files. Existing forms (`name`,
`Record.field`, `Variant.case`, `FILE:LINE:COL`) are unchanged.

`vo query refs main.vo notes.add_note`:

```text
notes.vo:12:11 declaration in add_note: export fn add_note(...)
api.vo:40:17 call in create: let n = try notes.add_note(ctx.db, input.body.text);
```

(As built: a module import is not a reference of the items used through it, so no `import` role is
listed; a 0.3 import of one item still is.)

The span of a qualified use covers `notes.add_note`. `vo edit rename main.vo notes.add_note add` replaces
the declaration's name and the item part of each qualified use, checks the result, writes all files or
none.

## `vo lsp`

| Request | On the module part (`web` of `web.on_get`) | On the item part |
|---|---|---|
| hover | `module std.math (declared at std/math.vo:1:1)` and the first comment line of its file (`type-at` answers `"type":"module"`) | as today |
| definition | line 1 of the module's file (the `declaration` of that answer) | as today |
| references | not added | as today |
| rename | not added (renaming a module is renaming its file) | as today |
| completion | not part of this spec | |

## `vo doc`

`vo doc std.json.parse_json` prints the declaration as today plus `use: import std.json; then
json.parse_json` (exported items only). `--json` adds `"use": "json.parse_json"` (schema 1: a field added).

## `vo fmt`

Imports sorted by their dotted text, as today. Qualified names printed as written.

## Unchanged

`vo.effects` (functions named `FILE:fn`), recordings, traces, runtime error frames, coverage and mutation
reports, `vo build`, `vo package`.
