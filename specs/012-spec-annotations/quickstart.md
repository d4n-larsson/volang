# Quickstart: Spec Annotations on Tests

```sh
make && tests/run.sh tests/cases/spec tests/cases/fmt tests/cases/query
python3 tests/catalog.py && python3 tests/apply_fixes.py
```

1. Annotate two tests of `examples/notes-api/main.vo` with `@spec(...)`: `vo check` clean, `vo test`
   unchanged except the summary lines; `vo test --json` shows `spec` fields.
2. `vo query specs main.vo`: each ID with its tests.
3. Mistakes: `@spce`, `@spec("FR 2")`, `@spec()`, `@spec` on a `fn`, two `@spec`, a repeated ID: one check
   reports all, with fixes for E0116, E0119 and W0101.
4. `vo fmt` on `@spec( "B","A" ) test ...`: `@spec("A", "B")` on its own line.
