# Contract: Serving HTTP (`std.http`, spec 011)

```vo
export record Request { method: str; path: str; query: str; headers: list[Header]; body: str; }
export fn serve[E: effects](address: str, handle: fn(Request) -> Response uses E) -> result[void] uses serve, E
export fn request_header(r: Request, name: str) -> result[str]
```

## The effect

| Effect | Scope | Example |
|---|---|---|
| `serve` | an address: `"PORT"` (loopback only) or `"0.0.0.0:PORT"` (all interfaces) | `uses serve("8000"), file.read("site/")` |

- Calling `serve` needs `serve` (E0501, fix adds it); a scope must cover the callee's (E0503); any other
  scope form is E0506. At run time the address must be in the grant, else R0010 before listening.
- The function calling `serve` declares the handler's effects too (effect parameter `E`).

## Behavior

| Situation | Answer | Handler called |
|---|---|---|
| a well-formed request | the handler's response, plus `Content-Length` and `Connection: close` | yes |
| `HEAD` | the handler's response for the same request with method `HEAD`, without the body | yes |
| the handler stops with a runtime error (R0009 included) | `500`, empty body; the report on stderr | yes |
| malformed request line or headers | `400` | no |
| no complete request within 10 s | `408` | no |
| body over 16 MiB | `413` | no |
| head over 64 KiB or more than 100 headers | `431` | no |

- One request at a time, in arrival order. `--timeout` bounds each handler call, not the server.
- SIGINT or SIGTERM: the request in progress finishes, the socket closes, `serve` returns `ok()`.
- `serve` returns `err("...")` when it cannot listen (address in use, permission denied).
