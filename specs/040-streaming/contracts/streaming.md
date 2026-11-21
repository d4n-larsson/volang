# Contract: streaming functions

```text
std.fs.each_line[E: effects](path: str, f: fn(str) -> result[bool] uses E) -> result[void] uses file.read, E
std.fs.each_chunk[E: effects](path: str, size: int, f: fn(bytes) -> result[bool] uses E) -> result[void] uses file.read, E
std.http.send_stream[E: effects](method: str, url: str, headers: list[Header], body: str, timeout_ms: int,
    f: fn(Response, bytes) -> result[bool] uses E) -> result[Response] uses net, E
std.process.run_lines[E: effects](program: str, args: list[str], input: str, env: list[str],
    f: fn(str) -> result[bool] uses E) -> result[Output] uses process, E
```

## The passed function

Called once per piece, in order. `ok(true)`: go on. `ok(false)`: stop; the stream returns its success
value. `err(m)`: stop; the stream returns `err(m)`. Its effects are `E`: the caller declares them.

## Results and errors

| Function | Success | Errors (besides the function's own) |
|---|---|---|
| `each_line` | `ok()` | what `read_file` gives for the path; `line N: not UTF-8`; `line N: longer than 16 MiB` |
| `each_chunk` | `ok()` | what `read_bytes` gives; `the chunk size must be 1 to 16777216` |
| both | | `not a regular file: read it whole with read_file` for a pipe, FIFO or terminal |
| `send_stream` | `ok(Response)` with status and headers, `body` empty | what `send` gives for the URL, method and headers; connection errors in `std.tcp`'s words; `the response head is larger than 64 KiB`; `malformed response: ...`; `the response ended early` |
| `run_lines` | `ok(Output)` with `code` and `stderr`, `stdout` empty | what `run_with` gives; the line errors of `each_line` |

Stops the program: a path, host or program outside the grant (R0010), invalid process arguments
(R0007, as `run`), `--timeout` (R0009), `--max-memory` (R0006).

## Guarantees

- Memory does not grow with the size of the source.
- When the call returns, nothing is left open and no child is left running; the same holds when the
  passed function stops the program or the test.
- `send_stream`: `timeout_ms` bounds each wait for the server, not the transfer. Redirects are not
  followed. The body's framing (length, chunked) is removed.
- `run_lines`: a line is delivered when the child has written its line end. After the end of output
  the call waits for the child to end; a child stopped by the passed function is killed. There is no
  time limit per line: a silent child is bounded by `--timeout` only, as for `run`.

## Recording

`--record` writes each piece as it is delivered. `vo replay` needs no file, server or program: a file
stream must read the same path at the same offsets; an HTTP stream must connect to the same host and
port (the path and what is sent are not compared); a child stream must name the same program and
arguments (R0014 otherwise).
