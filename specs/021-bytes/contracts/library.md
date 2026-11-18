# Contract: library additions

All new items are documented by `vo doc` with a checked example.

## std.bytes (new)

| Item | Signature | Effects | Errors |
|---|---|---|---|
| `from_text` | `(s: str) -> bytes` | — | — |
| `to_text` | `(b: bytes) -> result[str]` | — | `err("not UTF-8 at byte N")` |
| `from_ints` | `(xs: list[int]) -> bytes` | — | R0007 "Byte K is V, not 0 to 255." |
| `to_ints` | `(b: bytes) -> list[int]` | — | — |
| `to_hex` | `(b: bytes) -> str` | — | — |
| `from_hex` | `(s: str) -> result[bytes]` | — | `err("not hex at position K")`, `err("odd number of hex digits")` |
| `to_base64` | `(b: bytes) -> str` | — | — |
| `from_base64` | `(s: str) -> result[bytes]` | — | `err("not base64 at position K")`, `err("base64 length is not a multiple of 4")` |
| `read_stdin` | `() -> bytes` | input | — |
| `write_stdout` | `(b: bytes)` | console | — |

## std.fs (added)

| Item | Signature | Effects |
|---|---|---|
| `read_bytes` | `(path: str) -> result[bytes]` | file.read |
| `write_bytes` | `(path: str, b: bytes) -> result[void]` | file.write |
| `append_bytes` | `(path: str, b: bytes) -> result[void]` | file.write |

The errors, scopes and link rules are those of `read_file`, `write_file` and `append_file`.

## std.process (added)

`record BytesOutput { code: int; stdout: bytes; stderr: bytes; }`

| Item | Signature | Effects |
|---|---|---|
| `run_bytes` | `(program: str, args: list[str]) -> result[BytesOutput]` | process |
| `run_with_bytes` | `(program: str, args: list[str], input: bytes, env: list[str]) -> result[BytesOutput]` | process |

`run`/`run_with`: unchanged except the message `"output is not UTF-8 (run_bytes reads it)"`.

## std.http (added)

`record BytesResponse { status: int; headers: list[Header]; body: bytes; }`
`record BytesRequest { method: str; path: str; query: str; headers: list[Header]; body: bytes; }`

| Item | Signature | Effects |
|---|---|---|
| `get_bytes` | `(url: str) -> result[BytesResponse]` | net |
| `post_bytes` | `(url: str, content_type: str, body: bytes) -> result[BytesResponse]` | net |
| `serve_bytes` | `(address: str, handle: fn(BytesRequest) -> BytesResponse uses E) -> result[void]` | serve, E |

- `get`/`post`: a body that is not UTF-8 is `err("the body is not UTF-8 at byte N (get_bytes reads it)")`.
- `serve`: a request body that is not UTF-8 is answered `400` without calling the handler.
- In both APIs a header value that is not UTF-8 is an `err` (client) or a `400` (server).

## Builtins: behavior changes (strict text)

| Builtin | Non-UTF-8 input now |
|---|---|
| `read_file` | `err("not UTF-8 at byte N (std.fs.read_bytes reads any bytes)")` |
| `read_line` | `err("not UTF-8 at byte N (std.bytes.read_stdin reads any bytes)")` |
| `env` | `err("not UTF-8 at byte N")` |
| `read_all` | R0007 "Standard input is not UTF-8 at byte N: read it with `std.bytes.read_stdin`." |
| `args` | R0007 "Argument K is not UTF-8 at byte N." |
