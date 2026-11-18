# Data Model: Binary data

## bytes (built-in type)

| Property | Value |
|---|---|
| kind | `TY_BYTES`; runtime object: the `str` object (length, data) |
| elements | integers 0–255, read with `b[i]` |
| mutability | immutable |
| equality / order | byte by byte; a proper prefix is smaller |
| map key | no |
| iterable | no |
| literal | none; empty value is `from_text("")` |
| rendering | `<N bytes: HEX...>` (first 32 bytes) |

Validation: `from_ints` elements must be 0–255 (R0007, naming the index and value); `to_text` requires
well-formed UTF-8 (Unicode Table 3-7), with an `err` naming the first invalid byte offset.

## New records

| Record | Fields | Module |
|---|---|---|
| `BytesOutput` | `code: int`, `stdout: bytes`, `stderr: bytes` | `std.process` |
| `BytesResponse` | `status: int`, `headers: list[Header]`, `body: bytes` | `std.http` |
| `BytesRequest` | `method: str`, `path: str`, `query: str`, `headers: list[Header]`, `body: bytes` | `std.http` |

## Recording fields (added)

| Field | In events | Holds |
|---|---|---|
| `value_base64` | `read_file` (bytes), `read_all` (bytes) | the bytes read |
| `stdout_base64`, `stderr_base64` | `run` (bytes) | the process output |
| `request_base64`, `response_base64` | `http`, `request`, `response` | the raw HTTP message |
