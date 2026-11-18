# Research: Binary data

Sources: `src/vo.h` (type kinds, Str), `src/text.c` (UTF-8), `src/replay.c` (recordings), `std/fs.vo`,
`std/process.vo`, `std/http.vo`, the builtin tables in `src/check_expr.c`, and the reference.

## R1. Representation

- **Decision**: a new type kind `TY_BYTES` with the **same heap object as `str`** (length and data). The
  checker keeps them apart; at run time a `bytes` value is a `Str` whose data may be any bytes.
- **What it shares** with `str`: `len`, `slice` (without the UTF-8 boundary check), `+`, `==` and
  ordering (both compare bytes), and the GC.
- **What changes for it**: the checker and the places that branch on the static type kind.
  - Of the 35 `TY_STR` sites in `src/`, about 15 need a `TY_BYTES` case: rendering, `prop.c`, `task.c`
    (concurrent_map transfer), `rt_coll.c` ordering, `rt_test.c` `expect_eq`, and `check_expr.c`
    operators and overloads.
  - The others are text-only (`str()`, `chars`, map keys) and must refuse `bytes`.
- **Alternatives**: a list of ints (8 bytes per byte, no fast I/O), or a new heap kind (duplicates every
  string routine).

## R2. Keyword and type

- `bytes` is added to the lexer's keywords and the parser's type names, like `str`.
- No vo file in the repository (examples, tests, std, benchmarks) uses `bytes` as a name. Programs that
  do get the existing keyword error.
- The type is not a valid map key (keys stay `int`/`str`); a map with `bytes` keys is E03xx, the existing
  "key type" error, with a note suggesting `to_hex`.

## R3. Core operations

| Operation | Rule | Error |
|---|---|---|
| `len(b)` | the byte count | — |
| `b[i]` | an `int` from 0 to 255; reading only (bytes are immutable; `set b[i]` is the existing assignment error) | R0003 out of range |
| `slice(b, a, c)` | a new `bytes` | R0003 |
| `b + c` | joining | — |
| `==`, `!=`, `<`, … | byte by byte; a prefix is smaller | — |
| `sort`, `min`, `max`, `T: ordered`, ordered collections | as `str` (`C` gains `bytes`) | — |
| `expect_eq` | the first differing offset (R11) | — |
| `str(b)`, `chars(b)`, `print(b)`, `for x in b`, `b + "x"` | E0301 / E0302 with a fix naming `std.bytes.to_text` or `to_hex` where one is unambiguous | — |

- **Indexing** compiles to one new VM instruction, the byte read; slicing reuses the `str` slice
  primitive without the boundary check.
- **No `for`**: like `str`, `bytes` is not iterable. Use `to_ints(b)` or an index loop. The checker's
  message for `for x in b` names `to_ints`.

## R4. `std.bytes` (no new builtin names)

| Function | Notes |
|---|---|
| `from_text(s: str) -> bytes` | UTF-8 encoding, cannot fail |
| `to_text(b: bytes) -> result[str]` | `err("not UTF-8 at byte N")` |
| `from_ints(xs: list[int]) -> bytes` | R0007 "byte N is V, not 0 to 255" |
| `to_ints(b: bytes) -> list[int]` | |
| `to_hex(b) -> str`, `from_hex(s) -> result[bytes]` | lowercase out; either case in; err names the position (odd length: the last) |
| `to_base64(b) -> str`, `from_base64(s) -> result[bytes]` | RFC 4648 standard alphabet, padded; no whitespace accepted; err names the position |
| `read_stdin() -> bytes` uses input | all of standard input |
| `write_stdout(b: bytes)` uses console | no newline added |

- **Empty `bytes`**: `from_text("")`. No literal (FR-003).
- **In C**: all conversions are primitives in `src/bytes.c` (speed: 100 MB of hex or base64 in under
  1 s); `std/bytes.vo` wraps them with `vo doc` comments.
- **Primitives**: `__bytes(op, b: bytes, s: str) -> result[bytes]` and
  `__bytes_text(op, b: bytes) -> result[str]`, plus `__bytes_ints`/`__ints_bytes` for the list
  conversions. The signature table gains the codes `y` (`bytes`), `Y` (`result[bytes]`) and
  `I` (`list[int]`).

## R5. Strict UTF-8 at every input boundary

- **The validator** becomes exact. `utf8_seq_len` today checks lead and continuation bytes only; its
  `ponytail:` note says overlong 3- and 4-byte forms and surrogates pass. It now follows Unicode's
  Table 3-7 (well-formed sequences).
- **Source files are checked by the same function**, so a source file with an overlong form or a
  surrogate becomes the existing lexer error. The change states this.

Where outside data becomes a `str`, and what happens when it is not UTF-8:

| Where | Result | Message |
|---|---|---|
| `read_file(path)` | `err` | `"not UTF-8 at byte N (std.fs.read_bytes reads any bytes)"` |
| `read_line()` | `err` | the same, naming `std.bytes.read_stdin` |
| `read_all()` | R0007 | "Standard input is not UTF-8 at byte N: read it with `std.bytes.read_stdin`." |
| `args()` | R0007 at the call | "Argument K is not UTF-8 at byte N." |
| `env(name)` | `err` | `"not UTF-8 at byte N"` |
| `std.process.run` text output | `err` | `"output is not UTF-8 (std.process.run_bytes reads it)"` (wording extends spec 017's) |
| `std.http` text body | `err` | `"the body is not UTF-8 at byte N (get_bytes reads it)"` |
| HTTP header values | `err` | `"a header is not UTF-8"` (headers are text in both APIs) |
| `serve` text request body | 400 answer | the handler is not called; the report says why |

- Directory listings keep leaving non-UTF-8 names out (spec 017).
- A **replayed** run applies the same checks to the recorded data, so it behaves like a live one.

## R6. Files

- `std.fs` gains `read_bytes(path) -> result[bytes]` (`file.read`),
  `write_bytes(path, b) -> result[void]` and `append_bytes(path, b) -> result[void]` (`file.write`).
- They use new `__fs` operations through a typed primitive `__fsb(op, path, data: bytes) -> result[bytes]`.
- Effects are checked by operation in `src/fs.c`, as for the existing `__fs`. Paths, scopes, link
  handling and errors are exactly those of the text functions (one shared code path; only the
  conversion differs).

## R7. Processes

- `run_bytes(program, args) -> result[BytesOutput]` and
  `run_with_bytes(program, args, input: bytes, env) -> result[BytesOutput]`.
- `BytesOutput { code: int, stdout: bytes, stderr: bytes }`.
- `__run` gains a sibling `__run_bytes` with the same argument checks, which returns the raw output.
  The text `run` keeps its UTF-8 error, now with the hint.

## R8. HTTP

- **Raw exchange**: `__http_exchange`, `__https_exchange` and `__serve_next` return `result[bytes]`, and
  `__serve_reply` takes `bytes`.
- **`std/http.vo` parses on bytes**: `__http_part` splits head and body as `bytes`. The head goes
  through `to_text` (header values must be UTF-8), and the body stays bytes.
- **Text API, unchanged in shape**: `get`/`post` return `Response { body: str }`, converting the body
  with `to_text` (the R5 message on failure). `serve` handlers keep `Request`/`Response` with `str`
  bodies; a non-UTF-8 request body is answered 400 without calling the handler.
- **Bytes API**:
  - `get_bytes(url) -> result[BytesResponse]`;
  - `post_bytes(url, content_type, body: bytes) -> result[BytesResponse]`;
  - `serve_bytes(address, handle: fn(BytesRequest) -> BytesResponse)`;
  - `BytesResponse`/`BytesRequest` mirror `Response`/`Request` with `body: bytes`.

  The effects (`net`, `serve`) and scopes are the same as the text functions'.
- **Rejected**: a `body_bytes` field on the existing records, which would give two bodies in one value,
  and a generic `Response[B]`, which adds generics to the most-used records for one use.

## R9. Rendering

- In traces, runtime error locals, `vo test` reports and JSON output, a `bytes` value renders as
  `<N bytes: 0a1b2c...>`: hex of the first 32 bytes, then `...` when longer.
- It is never a vo literal (none exists), so an agent cannot copy it into code.
- The same text appears in `--json` value fields.

## R10. Recordings

- **Lossy today**: recordings store text payloads as JSON strings, and `json_escape_into` turns invalid
  UTF-8 into U+FFFD.
- **Decision**: events whose payload is bytes use new fields:
  - `value_base64` for `read_file`-as-bytes and `read_all`-as-bytes;
  - `stdout_base64` and `stderr_base64` for process output read as bytes;
  - `request_base64` and `response_base64` for HTTP exchanges, both text and bytes APIs, since the raw
    exchange is now bytes.

  Text payloads keep `value`/`request`/`response`. Fields are only added (schema 1, Principle II), so
  existing recordings replay.
- Replay checks the kind (`read_file` text vs bytes) like any other event mismatch (R0014), naming what
  the program asked for.

## R11. `expect_eq`, property tests, concurrent_map

- **`expect_eq`** on two `bytes`: "differ at byte 1000: 41 (expected 42)", with up to 8 bytes of context
  each side in hex. This is the spec 018 "first difference" path, with a byte offset instead of a list
  index.
- **Property tests**: generation is like `list[int]`, with sizes from the existing schedule and elements
  0–255 (biased to 0, 255 and ASCII); shrinking shortens, then lowers elements.
- **`concurrent_map`**: values crossing between processes are serialized by kind; `bytes` goes like
  `str` (length and data), with no UTF-8 assumption.

## R12. Performance

- Reading and writing bytes is the text path without the UTF-8 check, so SC-004 is met by construction.
  A 100 MB case in `tests/bytes_scale.py` measures both.
- Hex and base64 are single-pass C loops into one allocation.
- HTTP bytes and text share the exchange; text adds one UTF-8 check.

## R13. Reference budget

Estimated additions, about 170 tokens:
- a `bytes` row in the types table;
- a short paragraph of operations;
- a strict-text sentence;
- `std.bytes` in the modules table, and the bytes functions named in the `std.fs`/`std.process`/
  `std.http` rows.
