# Results: Documentation on hover

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | `tests/cases/query/hover_*` on the two-file fixture `tests/cases/query/hover/`: a call, the declaration and the import of a documented function (the two description lines joined, the example), a `std` function (the same record as `vo doc --json std.math.sqrt`), a builtin (`len`), an undocumented function (signature, no description), a generic function (`[T]` as declared), a record, a commented and a plain field, a variant and a case; `doc/program_*` for `vo doc ENTRY NAME`. |
| SC-002 | met | Every existing `query` case keeps its output, except `type_at_field`, whose human answer gains the field's signature line (`    price: int`); `doc/two_names` shows the new usage text. `vo doc`'s own output is byte-identical (all `doc` cases, 129 doc examples). |
| SC-003 | met | `tests/hover_speed.py`: `type-at --json` with documentation on a 2,646-line two-file program: 6 ms (limit 100 ms). In `make gate`. |
| SC-004 | met | Reference: about 12,542 tokens, +69 (limit +80): two sentences and two command lines. |

## Verification

| Check | Result |
|---|---|
| `make test` (x86-64) | passed: golden 768, step 211 + 235 + 35, the self-tests, catalog, reference, site (194 examples), doc conformance, `math_vectors`, `bytes_fuzz`, examples, diff-engines 1,000 (0 disagreements) |
| ARM64, RISC-V | `make ARCH=aarch64 test` and `make ARCH=riscv64 test` (qemu): the same counts, all passed |
| `make gate` | not run (only when asked); its new step, `tests/hover_speed.py`, was run: 6 ms |

## Size

- `src/doc.c`: `doc_item_fields` and `doc_builtin_fields` (split from the printers), `doc_record_for`,
  `doc_module_of`, `doc_program` (`vo doc ENTRY NAME`), and `doc_comments` joining description lines;
  about 170 lines. `src/query.c`: about 30 lines in `q_type_at`. `src/check_expr.c`: `builtin_name_of`.
- Tests: 17 `hover_*` query cases, 4 `program_*` doc cases, the fixture program, `tests/hover_speed.py`.

## Deviations

- **The human `type-at` answer on a field** gains the field's signature line (FR-006 asks for the
  signature on every documented-or-not item); one existing case (`query/type_at_field`) shows it.
- **`vo doc`'s usage error** names the new form, `vo doc main.vo NAME` (`doc/two_names`).
- **Example lines on fields and cases** are read like any comment but not returned (only items have an
  `example` field in `vo doc`'s format).
