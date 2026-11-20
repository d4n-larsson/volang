# Quickstart: validating std.web (spec 029)

Prerequisites: `make` (build/vo), the spec 028 server, `ab` for the speed check.

1. Derived JSON: `vo run` a program printing `write_json(to_json(note))` and `from_json[...]` of a body
   with a missing field and a too-long text: one `err` naming both (tests/cases/json_derive/).
2. Annotations: `vo check` a record with `@size(5, 1)` and `@range` on a `str`: E0329 twice
   (tests/cases/annotations/).
3. Routing in a test: `vo test examples/notes-api/main.vo`: every operation driven through
   `handle_request` with an in-memory database, no `serve` effect.
4. Serving: `vo run examples/notes-api/main.vo`, then
   - `curl -i localhost:8080/notes/abc` → 400 problem naming `id`;
   - `curl -i -X PUT localhost:8080/notes/1` → 405 with `Allow`;
   - `curl -i -d '{"text": ""}' localhost:8080/notes` → 422 problem: `text: at least 1 characters`;
   - `curl -i localhost:8080/health` → 200 `{"status":"up"}`.
5. Check-time route errors: a route table with `/notes/{id}` and `/notes/{name}` for GET: E0331 naming
   both (tests/cases/web/).
6. Malformed input: `python3 tests/web_fuzz.py` (SC-003): 1,000 requests, 0 problems.
7. Speed: `python3 tests/web_speed.py` (SC-006): std.web within 10% of the hand-written server.
8. Replay: record a notes-api session, `vo replay` it: same responses.
