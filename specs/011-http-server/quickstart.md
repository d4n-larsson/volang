# Quickstart: HTTP Server Support in `std.http`

```sh
make && tests/run.sh tests/cases/serve && python3 tests/steps.py tests/cases/serve
```

1. A two-route service (`examples` or a step case): `vo run service.vo -- 8000`, then
   `curl -i localhost:8000/hello` (200), `curl -i localhost:8000/nope` (404), Ctrl-C (exit 0).
2. Without `uses serve("8000")`: `vo check` gives E0501 with the fix; with `serve("8001")` declared and
   `serve("8000", ...)` called: R0010 at run time.
3. A handler that divides by zero on `/boom`: the client gets 500, stderr has the R0001 report, the next
   request is answered.
4. `vo run --record r.jsonl service.vo`, some requests, Ctrl-C; `vo replay r.jsonl`: the same output;
   change a handler and replay: one note per changed response.
5. `vo test service.vo`: the handler's tests build `Request` values and need no effect.
6. SC-003: `tests/serve_endurance.py` (10,000 requests, memory stable).
