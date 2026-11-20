# Quickstart: validating spec 032

Prerequisite: `make`. Details: [contracts/](contracts/).

1. **The type.** The example of [contracts/syntax.md](contracts/syntax.md) checks and prints `12` and
   `no file config.txt`; removing the `malformed` case from `describe` is E0401 with the fix that inserts
   it (`tests/cases/typed_errors/use*`).
2. **try.** `try load(...)` in `main` without `map_err` is E0412 naming `LoadError` and `str`; with
   `map_err` it checks (`tests/cases/typed_errors/try*`).
3. **Bad error types.** `result[int, void]` and `result[int, fn(int)]` are E0334.
4. **Reports.** A failing `expect_eq(load(""), ok(1))` shows `err(LoadError.missing("config.txt"))`; a
   `prop` over `result[int, LoadError]` generates both cases; `concurrent_map` returns typed errors in
   order (`tests/cases/typed_errors/report*`, `tests/cases/prop/typed*`, `tests/cases/tasks/typed*`).
5. **std.web.** The notes API with `NoteError` and `typed(...)` passes its tests with the same answers;
   a mapping missing a case is E0401 (`tests/cases/web/typed*`).
6. **std.option.** `vo doc std.option` documents `Option` and its helpers; the examples there run.
7. **Unchanged.** `make test`; `python3 tests/gc/baseline.py` reports 731 of 731 unchanged;
   `python3 tests/ref_examples.py` reports the reference under 15,000 tokens.
