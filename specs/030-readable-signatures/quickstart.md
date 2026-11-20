# Quickstart: validating spec 030

1. `vo fmt` on a file with a 120-character header: the clause moves to its own line; a header with long
   parameters: one per line (tests/cases/fmt/signatures*).
2. `effects Storage = file.read("data/"), file.write("data/");` and `uses Storage`: checks, runs; `vo
   effects --update` writes the expanded effects (tests/cases/effect_sets/).
3. Change the set to read only: `vo check` reports E0504-style changes for each function using it.
4. Misuses: `uses Storge` (E0509, fix `Storage`), a cycle (E0513), an unused set (W0505).
5. `examples/notes-api`: `Storage` declared once; tests pass; its record unchanged
   (`tests/effect_sets_records.py`).
6. `make test`.
