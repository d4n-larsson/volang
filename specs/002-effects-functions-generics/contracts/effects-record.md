# Contract: The Effects Record (`vo.effects`)

## Location

The project root is the nearest ancestor directory of the entry file (including its own directory) that contains `vo.effects`. When no such directory exists, `vo effects --update` creates the record in the entry file's directory.

## Format

The file is UTF-8 text with one entry per line, sorted byte-wise by path and then name. Lines starting with `#` are comments, and `--update` keeps them at the top.

```
# vo effects record: update with `vo effects --update`
geom.vo:area pure
main.vo:main uses console, file.write("out/")
main.vo:test "loads fixtures" uses file.read("fixtures/")
report.vo:save_report uses file.write("out/reports/")
```

- `path` is relative to the project root and uses `/`.
- `name` is the function name, or `test "<name>"` for a test.
- The effects use the canonical `uses` text, or `pure`. Effect parameters render as their names.

## Comparison (in `vo check`)

When a record exists, the program's functions (the entry and its imports) are compared with it:

| Situation | Diagnostic |
|---|---|
| A function's effects grew: a new effect, a widened scope or a removed scope | E0504 `effects_grew` (error): "`save_report` gains file.write(\"out/\"); approve with `vo effects --update`". One `related` entry per call path that introduced the growth. |
| A new function with effects | E0504 (a new pure function is fine) |
| Effects shrank, or a function was removed | W0502 `effects_reduced` |
| One function removed and one added in the same file with identical effects | W0503 `function_renamed` (no E0504) |
| No record | W0504 `no_effects_record`, once per program, only when the program spans more than one file. Single-file scripts are not projects, and a warning there would add noise to every check. |

E0504 has no fix: approving growth is a human decision, made with `vo effects --update`.

## `vo effects [--json] [--update] PATH`

- **Without `--update`**: prints the capability summary of the program at PATH. That is the grant of `main` and of each test, plus the per-function lines that would be written.
- **With `--update`**: rewrites the record and prints exactly the changes it made (the same records as the check reports, as `kind: "effect_change"`).
