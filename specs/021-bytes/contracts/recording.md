# Contract: recordings (schema 1, fields added)

| Event | New field(s) | When |
|---|---|---|
| `read_file` | `value_base64` | the program read the file as bytes (`std.fs.read_bytes`) |
| `read_all` | `value_base64` | the program read stdin as bytes (`std.bytes.read_stdin`) |
| `run` | `stdout_base64`, `stderr_base64` | the program used `run_bytes`/`run_with_bytes` |
| `http` | `request_base64`, `response_base64` | always from this spec on (the raw exchange is bytes), replacing `request`/`response` in new recordings |
| `request`, `response` (serve) | `request_base64`, `response_base64` | always from this spec on |

- Old recordings (text fields only) replay: a text field is read as its UTF-8 bytes.
- A text event read where the recording has a bytes event, or the reverse, is R0014 naming what the
  program asked for.
- Base64 is RFC 4648 standard with padding, as `std.bytes.to_base64`.
