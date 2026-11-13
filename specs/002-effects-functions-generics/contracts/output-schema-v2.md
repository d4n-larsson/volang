# Contract: Output Additions in v2 (schema version 1, extended)

Only new kinds and fields are added. Consumers already ignore unknown fields.

## `effect_change`

Emitted by `vo effects --update`. `vo check` emits the same information as E0504, W0502 and W0503 diagnostics.

```json
{"v":1,"kind":"effect_change","change":"added","file":"report.vo","function":"save_report","effects":"file.write(\"out/reports/\")","before":"pure","after":"uses file.write(\"out/reports/\")","via":[{"function":"save_report","calls":"write_report_file","file":"report.vo","start":{"line":9,"col":5}}]}
```

- `change` is `added`, `removed`, `widened`, `narrowed` or `renamed`. A rename adds `old_function`.

## `capabilities`

Emitted by `vo effects`, one record per entry point (`main` and each test):

```json
{"v":1,"kind":"capabilities","entry":"main","file":"main.vo","uses":"console, file.write(\"out/\")"}
```

## `runtime_error` for R0010

The record gains these fields:
- `effect` (e.g. `"file.write"`)
- `target` (the path or variable)
- `allowed` (the scopes, as strings)
- `declared_by` (the function whose declaration was enforced)

## `test`

The record gains `uses` (the test's grant), so agents see what a test was allowed to do.
