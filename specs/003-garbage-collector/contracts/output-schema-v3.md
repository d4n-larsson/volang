# Contract: Output Additions in v3 (schema version 1, extended)

Only new kinds and fields are added.

## `memory`

Emitted only with `--mem-stats --json`.

```json
{"v":1,"kind":"memory","command":"run","peak_live":3355443,"allocated":1503238553,"collections":212,"limit":536870912}
```

- **For `test`:** the record adds `"test":"<name>"` and follows that test's `test` record.
- **Units:** all sizes are in bytes.

## `runtime_error` for R0006

The record gains:
- `in_use`: live bytes after the last reclamation pass;
- `limit`: the limit, in bytes.

```json
{"v":1,"kind":"runtime_error","code":"R0006","name":"out_of_memory","message":"The program holds 512.3 MiB of live data, over the 512 MiB limit.","file":"main.vo","start":{"line":4,"col":9},"end":{"line":4,"col":23},"in_use":537198592,"limit":536870912,"trace":[…]}
```
