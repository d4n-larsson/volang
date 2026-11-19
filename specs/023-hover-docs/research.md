# Research: Documentation on hover

Sources: `src/query.c` (`q_type_at`, `q_note`, `q_note_sym`, `q_note_decl`, `q_decl_of`), `src/check.c`
and `src/check_expr.c` (the reference-log calls), `src/doc.c` (`doc_comments`, `doc_block`,
`doc_signature`, `doc_print_item`, `doc_print_builtin`, `cmd_doc`).

## R1. What a name refers to

The reference log keys each name (spec 006):

| Key | Meaning | Logged by |
|---|---|---|
| `(item decl, -1)` | a function, record or variant, also from `std` or a package | `q_note_sym`, `q_note_decl` |
| `(record decl, i)` / `(variant decl, i)` | field / case i (comments not counted) | `check_expr.c` 578, 872, 919; `check.c` 726; `q_load` 133 |
| `(record decl, -2)` | a record literal (vo edit add-field) | `check_expr.c` 837 |
| `(0, builtin id)` | a builtin call | `check_expr.c` 461 |
| `(name node, -1)` | a local, parameter or type parameter | `q_note_sym` |

**Decision**: `doc_record_for(key_a, key_b, sb)` returns 1 and writes a record when:
- `key_a` is an `N_FN`/`N_RECORD`/`N_VARIANT` node and `key_b == -1` (an item);
- `key_a` is an `N_RECORD`/`N_VARIANT` node and `key_b >= 0` (a member);
- `key_a == 0` and `key_b` is a builtin id that has a row in the reference's builtin table.

Otherwise it returns 0. A local's name node is an `N_NAME`, so it never matches. The literal key `-2` is
mapped to the record itself.

## R2. One record for `vo doc` and `type-at`

- **Decision**: split `doc_print_item` into `doc_item_record(sb, module, item, desc, example)`, which
  builds the JSON record, and the printing. Do the same for builtins (`doc_builtin_record` from
  `doc_print_builtin`).
- **Module name** of a user item: the file's import path, its path relative to the entry's directory
  without `.vo`, with `/` as `.` (`geo`, `util.text`; the entry file is its own name, `main`). For
  `std/x.vo` it is `std.x`. Package files keep the module name `vo doc` gives them.
- **Members** (new kinds):

      {"item":"field","module":M,"owner":"Point","name":"x","signature":"x: float","description":...}
      {"item":"case","module":M,"owner":"Shape","name":"circle","signature":"circle(float)","description":...}

- **Rationale**: the plugin parses one format, and `vo doc` and the hover cannot drift apart.

## R3. Reading doc comments

- **Description**: every description line in the comment block directly above, each without `# `,
  joined by single spaces. `doc_comments` kept only the topmost line; no `std` item has more than one
  (checked), so `vo doc`'s output does not change.
- **Example**: the topmost `#     ` line, as today.
- **Members**: the same rules over the declaration's member list. Fields and cases sit in it beside
  `N_COMMENT` nodes, and the member index `i` skips those comments, as `count_members` does.
- **No comment**: the record has no `description` field (spec FR-003). `vo doc` printed `""` for exported
  package items without a comment; it keeps doing so for its listing, and the hover leaves the field out.

## R4. `type-at` output

- **JSON**: after `"declaration"`, `,"doc":` and the record, without the outer `{"v":1,"kind":"doc",...}`
  head (a nested object: `item`, `module`, `name`, `signature`, …, the same fields).
- **Human**: today's line, then `    SIGNATURE` and `    DESCRIPTION` when present.
- **When**: only when the name wins (R1 applies to names); an expression answer (a call's whole span) is
  unchanged.
- **Generics**: the declared signature (`doc_signature` prints type parameters).

## R5. `vo doc ENTRY NAME`

- **Decision**: when `vo doc`'s first argument ends in `.vo` and is a file, load and check that program
  (as `vo query` does) and document the item NAME declared in any of its files, private or exported. The
  output is the same text and JSON as library items.
- **Unknown NAME**: T0010 "Nothing is documented as `NAME` in ENTRY" with the nearest name, as
  `doc_not_found` does.
- **Rationale**: the terminal gets the hover's text, and agents get it without JSON.

## R6. Speed

- `type-at` already loads and checks the whole program; the lookup scans one file's item list.
- **SC-003** is measured by a case on a generated 2,000-line program in `tests/hover_speed.py` (in `make
  gate`).

## R7. Reference

Two sentences, in "Querying and editing programs":
- The comment lines directly above a function, record, variant, field or case document it (a line
  `#     EXPR is VALUE` is its example).
- `type-at` adds that documentation and the declared signature (`doc` in JSON), and `vo doc ENTRY NAME`
  prints it for the program's own items.

Plus the `vo doc` line in "Command line" gains `ENTRY NAME`.
