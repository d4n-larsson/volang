# Quickstart: Visibility with `export`

```sh
make && tests/run.sh tests/cases/multi tests/cases/check tests/cases/parse tests/cases/doc tests/cases/fmt
python3 tests/catalog.py && python3 tests/apply_fixes.py && python3 tests/doc_conformance.py
```

1. Two files, `report.vo` with `export fn totals` and `fn money`; `main.vo` imports both: E0209 on
   `money` with a fix; after the fix the program checks and runs.
2. `import std.json.parse_value;`: E0209, "internal to `std.json`", no fix; `vo doc std.json` unchanged.
3. `export fn load() -> Url` with a private `record Url`: E0210 with the fix exporting `Url`.
4. `vo check` every multi-file program in `bench2/`, `tests/` and `examples/`: no E0209/E0210.
5. Plugin: `./gradlew test` in `../volang-intellij-plugin` (the `export` keyword test).
