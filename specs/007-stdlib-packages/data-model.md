# Data Model: Standard Library and Content-Pinned Packages

## Library values (vo, contracts/library.md)

- **Json**: variant `null | boolean(bool) | integer(int) | number(float) | text(str) | array(list[Json]) | object(map[str, Json])`. Objects keep key order (map insertion order). Invariant after `parse`: no NaN or infinity; nesting ≤ 512.
- **DateTime**: record of UTC `year` (1–9999), `month` (1–12), `day` (1–days_in_month), `hour` (0–23), `minute`, `second` (0–59), `ms` (0–999), `weekday` (1 = Monday … 7). `to_ms` validates every field but `weekday`.
- **Header**: `{name, value}`, names as received. **Response**: `{status, headers (order kept), body}`.

## Embedded standard files (runtime data)

`std_files`: a table of `{path cstr ("std/json.vo"), text, length}` in `src/data.s`, filled with `.incbin "std/json.vo"` etc. `load_file` registers a `std/...` path from it instead of the disk (research R2). File-table entries of standard files carry a flag (`FILE_STD`) used by the effects checker (R6), the effects record (not listed) and the `__` primitive gate (E0201 outside `std`).

## Manifest and packages (research R8)

| Entity | Fields |
|---|---|
| Manifest | path (the `vo.packages` file, registered with `file_add` so its lines have spans), entries |
| Package entry | name (str), directory (str, relative to the manifest), pinned hash (32 bytes), line span, verified (0/1), actual hash |

- Lifecycle: found on the first import whose first segment is not a local file → parsed (E0703, E0704) → on the first use of a package: directory listed recursively, files sorted, hashed (E0702 when there is no `.vo` file, E0701 on mismatch) → imports into it resolve to its directory.
- File-table entries of package files carry the package's name (`FILE_PKG`), used by `rec_relpath` to write `@NAME/relative/path.vo`.

## SHA-256 state (src/sha256.s)

`{h[8] u32, buffer[64], buffered, total length}`; `sha256_init`, `sha256_update(p, n)`, `sha256_final(out32)`; one-shot `sha256(p, n, out32)` for the tests.

## Documentation records (vo doc, contracts/cli.md)

For each item: module, name, kind (`fn`, `record`, `variant`), type parameters, parameters `{name, type}` or fields/cases, return type, effects (rendered effect set), description (first comment line above the item), example (comment lines indented four spaces).

## Recording events (spec 006 data model, extended)

`EVK_HTTP` (8): `EV_NAME` = host, a new `EV_PORT`, `EV_RESULT` ok/err, `EV_VALUE` = the raw response or the error text. Replay matches kind, host and port.

## Runtime primitives (research R5)

| Builtin | Signature | Effect | Note |
|---|---|---|---|
| `__http_exchange` | `(host: str, port: int, request: str) -> result[str]` | net | host checked against the frame's grant; resolve, connect, send, read to close; ≤ 16 MiB |
| `__float_text` | `(f: float) -> str` | — | 17 significant digits |

Both are rejected outside files flagged `FILE_STD` (E0201).
