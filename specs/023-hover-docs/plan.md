# Implementation Plan: Documentation on hover

**Branch**: `023-hover-docs` | **Date**: 2026-11-19 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/023-hover-docs/spec.md`

## Summary

`vo query type-at` already resolves the name at a position to a reference-log entry whose keys say what
it is (research R1):
- `(item declaration, -1)` for functions, records and variants, including `std` and package items, which
  are items of their files;
- `(record or variant declaration, index)` for fields and cases;
- `(0, builtin id)` for builtins;
- a name node for locals and parameters.

The plan adds one function, `doc_record_for(key_a, key_b)` in `src/doc.c`. It writes the same JSON record
`vo doc --json` writes for that item (R2), by splitting `vo doc`'s printing into "build the record into a
buffer" and "print it". `type-at` appends the record as `"doc"` to its JSON, and two indented lines
(signature, description) to its human output (R4). `vo doc ENTRY NAME` loads the program the way queries
do and documents its items with the same function (R5).

Doc comments keep their syntax. Two changes in how they are read (R3):
- The description becomes all the description lines joined. `vo doc` used only the first; no `std` item
  has more than one, so existing output is unchanged.
- Fields and cases read the comment directly above them in their declaration.

## Technical Context

**Language/Version**: C (C11, freestanding).

**Primary Dependencies**: none.

**Storage**: none.

**Testing**:
- golden cases `tests/cases/query/hover_*` (multi-file programs: a call, a declaration, an import, `std`,
  a builtin, an undocumented function, a generic function, a record, a field, a variant, a case, a local,
  a parameter, human and JSON);
- `tests/cases/doc/program_*` for `vo doc ENTRY NAME`;
- the existing `query` and `doc` cases, unchanged except the added `doc` fields;
- `make quick AREA=query,docs` between edits; `make test` once.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler and toolchain of a programming language, as a CLI.

**Performance Goals**: SC-003. `type-at` with documentation on a 2,000-line program takes under 100 ms.
The lookup is a scan of one file's items, negligible next to checking the program.

**Constraints**:
- Principle II: fields added only, schema 1.
- Principle V: reference ≤ +80 tokens.
- One source: `vo doc` and `type-at` build the record with one function.

**Scale/Scope**: about 150 lines of C (`src/doc.c` split and the member and builtin records, `src/query.c`
wiring, `cmd_doc` entry form), about 16 golden cases, 2 reference sentences.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Gate | Status |
|---|---|
| I. Agent-first | PASS. No syntax change; agents get the same documentation the editor shows. |
| II. Machine-readable feedback | PASS. `type-at` JSON gains an optional field (schema 1). The `doc` record is `vo doc`'s existing format, with two added kinds (`field`, `case`) for members. |
| III. Determinism | PASS. A pure function of the source. |
| IV. Test-first | PASS. Golden cases first. |
| V. Minimal surface | PASS. Two sentences in the reference; one function shared by `vo doc` and `type-at`. YAGNI: no markup, tags or parameter sections. |
| Implementation constraints | PASS. C only. |

Post-design re-check: unchanged.

## Project Structure

```text
specs/023-hover-docs/  plan.md research.md data-model.md quickstart.md contracts/cli.md
src/doc.c      doc_item_record (from doc_print_item), doc_member_record, doc_builtin_record, doc_record_for,
               doc_comments joining description lines, `vo doc ENTRY NAME`
src/query.c    q_type_at: the `doc` field and the indented lines
src/vo.h       declarations
tests/cases/query/hover_*, tests/cases/doc/program_*
docs/reference.md  doc comments for programs; type-at and vo doc ENTRY NAME
```

**Structure Decision**: the documentation logic stays in `src/doc.c`; `src/query.c` only asks for the
record.

## Complexity Tracking

None.
