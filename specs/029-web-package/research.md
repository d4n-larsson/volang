# Research: std.web (spec 029)

Starting point (read from the code): a server is `std.http.serve`/`serve_workers` with one function from
`Request` to `Response`. `std.json` has a `Json` variant, `parse_json`/`write_json` and field readers
(`text_field`, `int_field`, ...); there is no conversion between records and JSON. Errors are always
`str` (`err("...")`); there is no optional or null type; annotations exist only on items (`@spec`,
`@allow`); type parameters take constraints (`T: ordered`, `E: effects`, checked at instantiation by
`check_targ`). `vo check` already checks string-literal arguments of chosen library functions
(`regex("...")`, spec 025; `sql("...")`, spec 026). At run time a generic function's type arguments are
known (`rt_concrete` over the frame's `targs`), and `src/task.c` already walks a value by its concrete type
to encode it (spec 016). Reference: ~13,413 tokens of 15,000.

## R1 Shape: a library over serve_workers, four toolchain additions

**Decision**: `std.web` is vo code in `std/web.vo`, built on `std.http` (Request, Response, serve_workers)
and `std.json`. The toolchain gains only what a library cannot do:

1. derived conversion between values and `Json` (`std.json.to_json`, `std.json.from_json`) with a new
   type-parameter constraint `json` (R2);
2. limits on record fields as annotations (`@size`, `@range`, `@pattern`, `@default`, `@header`) (R3);
3. `vo check` of route patterns given as string literals, and of the routes of one list literal (R6);
4. effect arguments of a generic call taken from the expected type before the arguments (R10);
5. `new Name {}` for a record without fields (R7).

**Rationale**: clarification Q3 (standard library, may get toolchain support) and Q2 (derived
conversion). Everything else (routing, binding, answers, steps, settings, health) is ordinary vo code that
agents can read with `vo doc`, test with `vo test`, and that keeps effects visible.

**Alternatives**: a C implementation of the router (faster, but opaque to `vo doc`/traces and harder to
evolve; the router is not the bottleneck, R17); code generation from annotations (ruled out by Q1).

## R2 Derived JSON conversion

**Decision**: `to_json[T: json](v: T) -> Json` and `from_json[T: json](j: Json) -> result[T]` in
`std.json`. A type satisfies `json` when it is `int`, `float`, `bool`, `str`, `bytes`, a list or a
`map[str, V]` of such types, or a record or variant (generic ones included) whose fields and case values
do; functions, `map[int, V]`, the ordered collections, `result` and records with effect parameters do not
(E0328 names the first part that does not, with its path: `Note.owner.callback`).

| vo | JSON |
|---|---|
| `int`, `float`, `bool`, `str` | number (integer for `int`), number, `true`/`false`, string |
| `bytes` | string, base64 |
| `list[T]` | array |
| `map[str, V]` | object, keys in map order |
| record | object, fields in declaration order, keys = field names |
| variant case without a value | string, the case name (`"high"`) |
| variant case with a value | object with one key, the case name: `{"circle": 1.5}` |

Decoding is strict: unknown keys, missing fields (without `@default`), wrong JSON types, an `int` that is
not an integer or overflows, an unknown case name, invalid base64, and broken limits (R3) are all
problems; every problem is collected with its path (`tags[2]`, `owner.name`), and `from_json` returns
`err` listing them (`"text: missing; tags: at most 5 items, got 6"`), in document order. Nesting beyond 512 levels is a
problem (`(root): nested too deeply`), not a recursion. `to_json` cannot
fail; a float that is NaN or infinite becomes `null` (R0007-free: JSON has no such numbers). The work is a
runtime primitive that walks the concrete type (as `task.c`'s transfer form does), so there is no
per-record code and no code generation.

**Rationale**: Q2. One mapping, no options (no renaming, no case styles): agents write JSON keys as field
names. A variant case as a one-key object is the common "externally tagged" form and round-trips.

**Alternatives**: deriving functions per record at check time (generated code in traces and `vo doc`);
lenient decoding (silently ignored typos, spec edge case); `null` for absent fields (vo has no null, Q2
answer requires declared defaults).

## R3 Field limits as annotations

**Decision**: annotations on the line(s) above a record field (the item annotation syntax `@name(args)`,
arguments literals):

| Annotation | Field types | Meaning |
|---|---|---|
| `@size(min, max)` | `str` (characters), `list`, `map` | length bounds, inclusive |
| `@range(min, max)` | `int`, `float` | value bounds, inclusive |
| `@pattern("re")` | `str` | the whole text matches (std.regex syntax, checked as `regex("...")`) |
| `@default(literal)` | `int`, `float`, `bool`, `str`, enum-like variant case, `[]` for lists | the value when the key is absent |
| `@header("Name")` | input records of std.web only: `str`, `int`, `float`, `bool` | bind from this request header (R7) |

At most one of each per field. Misuse is E0329 (wrong field type, `min > max`, a default of the wrong type
or breaking the field's own limits, an unknown annotation on a field, with the name-distance fix of E0118);
a bad `@pattern` gets the regex diagnostics of spec 025. Limits are checked by `from_json` only; `new
Note { text: "" }` in code is not checked (limits describe input, not invariants).

Only fields of these kinds can be optional: a nested record and a non-empty default list cannot have
`@default` (E0329). An optional nested record is written as a `list` of at most one item (`@size(0, 1)`
with `@default([])`). ponytail: a real optional type would replace this; vo has none.

**Rationale**: limits must live with the field (Q2); five annotations cover the spec's limits (allowed
values are an enum-like variant, a type, not a limit). `@default` is the only way to make a field
optional, so absence is always declared (US2 scenario 3).

**Alternatives**: one `@check("1..200")` mini-language (another syntax to learn); limits enforced on
`new` (a contract change for every program).

## R4 Errors: prefixed `err` texts, mapped in one place

**Decision**: operations return `result[Response]`. An error text of the form `kind: detail` whose `kind`
is in the application's error table answers that status with `detail`; the default table maps
`bad_request` 400, `unauthorized` 401, `forbidden` 403, `not_found` 404, `conflict` 409, `gone` 410,
`unprocessable` 422; the application adds or overrides entries (`error_status("quota", 429)`). Any other
`err` (a database failure passed on with `try`) answers 500 with title "Internal Server Error" and no
detail; the full text goes to stderr as a log line. A runtime error answers 500 the same way (the report
goes to stderr as today).

**Rationale**: vo errors are strings and flow with `try` through any depth, the role exceptions play in
Spring; a prefix convention keeps `@ExceptionHandler`'s "map once" without adding error types.

**Alternatives**: a `Problem` return variant (every helper function would have to return it; `try` on a
database call would not compose); user error types (a language change out of scope).

## R5 Routes and matching

**Decision**: a route is built by `on_get`, `on_post`, `on_put`, `on_patch`, `on_delete`
(`on_` because `std.http.get`/`post` are the client and a program may import both). A pattern is a string
literal: `/` then segments, each a literal (letters, digits, `-`, `_`, `.`, `~`) or a variable `{name}`;
no trailing `/` except the root `/`. Matching: split the path on `/` (an empty segment, from `//` or a
trailing `/`, matches nothing), compare segment by segment, a literal beating a variable at the first
position where two patterns differ; variables are percent-decoded after splitting (so `a%2Fb` is one
value). No match for the path: 404; a match for the path but not the method: 405 with `Allow`; `HEAD` runs
the `GET` route without the body; `OPTIONS` answers 204 with `Allow` (and CORS headers when that step is
on, R9).

**Rationale**: the rules of Spring 6's path matching without its options (no trailing-slash matching,
no regex variables, no wildcards), so there is one answer to "does this path match".

## R6 Checks at `vo check`

**Decision**: for each `on_*` call whose pattern is a literal (it must be: E0330 otherwise, as `sql` and
`regex` require): the pattern's syntax (E0330 with the position), each `{var}` having a field of the input
record with a path-bindable type and every non-body, non-header input field being a query parameter (no
error; documented), duplicate variables (E0330). For the elements of one list literal of routes (the usual
`[on_get(...), ...]`): two routes with the same method whose patterns match the same paths (E0331, naming
both, with related locations), and a route no request can reach (shadowed: the same, E0331). Anything a
literal cannot show (routes built in loops) is checked when the application starts (`serve_app` returns
`err`, `handle_request` answers 500 and the test fails).

## R7 Binding request input

**Decision**: an operation is `fn(ctx: C, input: I) -> result[Response] uses E` with `I` a record
(`I: json`). Field sources, by rule: a field named like a pattern variable takes the variable; a field
named `body` takes the JSON body; a field with `@header("Name")` takes that header; every other field
takes the query parameter of the same name (a `list` field takes all its values, another field a repeated
parameter is a problem). Path, query and header texts are converted to the field type (`int`, `float`,
`bool` as `true`/`false`, an enum-like variant by case name); the body goes through `from_json`. Missing
or malformed input, broken limits: the operation is not called; the answer is a problem listing every
problem (400; 422 when the body was well-formed JSON of the right shape and only limits failed). A body
on an operation whose input has no `body` field is 400; a body that is not `application/json` (when `body`
exists) is 415; the size limit is the server's 16 MiB (413, unchanged). `std.web` exports `record NoInput
{}` for operations without input. Records without fields can be declared today but `new NoInput {}` does
not parse (E0110); the parser accepts it from now on, so a test can call such an operation directly.

**Rationale**: one record per operation shows all its inputs in one place, and binding reuses R2/R3
entirely; the name rule replaces Spring's `@PathVariable`/`@RequestParam`/`@RequestBody` without new
syntax, `@header` covers `@RequestHeader`.

**Alternatives**: separate parameters per input (variadic route constructors, not possible); accessor
functions inside operations (bad input would reach the operation).

## R8 Answers

**Decision**: operations answer `std.http.Response` values made with `std.web` helpers: `json_reply(v)`
(200), `created(location, v)` (201 with `Location`), `no_content()` (204), `json_status(status, v)`; any
`Response` built by hand passes through unchanged (the escape hatch). JSON answers have `Content-Type:
application/json`.

## R9 Steps

**Decision**: `record Step[C, E: effects] { prefix: str; run: fn(C, Request, fn(Request) -> Response uses E)
-> Response uses E; }`: a step gets the context, the request and the rest of the chain; it may answer
without calling it, change the request it passes on, or change the response. Steps run in list order on
the way in (outermost first) for requests whose path starts with `prefix` (`"/"`: all). Ready steps:
`request_ids()` (an `X-Request-Id` on each response: the request's own header when present, else 16 hex
digits from `random_int`), `log_requests()` (one stderr line: method, path, status, duration),
`cors(origins)` (preflight and response headers for listed origins), `bearer_auth(prefix, check)` (the
`Authorization: Bearer` token given to `check(ctx, token) -> result[str]`; `ok(identity)` adds the request
header `X-Identity` for operations to bind with `@header`; `err` answers 401 problem), and
`api_key_auth(prefix, header, check)` (the same with the key read from `header`).

**Rationale**: Spring's filter chain as plain functions; passing identity through a request header keeps
requests immutable values and needs no per-request mutable context.

## R10 Effects of route lists

**Decision**: the application declares its effects once, in the route table's type:
`fn routes() -> list[Route[Notes, uses file.read, file.write]] { return [on_get(...), ...]; }`. Today a
generic call binds an effect parameter from the first argument that mentions it (spec 026), so `on_get`
with a pure operation gives a `Route[Notes]` and the list literal fails to type. Change: when a generic
call has an expected type, its effect parameters that the expected type determines are bound from it
first; the arguments are then checked against them, and a function value whose effects are covered fits
(E0502 when not, as for any function value). Type parameters keep today's order.

**Rationale**: the minimal inference change that lets operations with different effects share one table
whose declared effects are the union, as `vo.effects` will show. Existing programs: an expected type with
effect arguments only exists since spec 026 (Database); its cases are re-run.

## R11 Context and serving

**Decision**: `serve_app(address, workers, setup, app)` over `serve_workers`: `setup` builds the context `C`
(the program's record: database, settings, clients) once per worker; every operation and step receives it.
`App[C, E]` is a record the program builds: `new App { routes: routes(), steps: [...], errors: [] }`.
`serve_app` validates the application before serving (R6's start checks) and returns `err` on a problem.

## R12 Settings

**Decision**: `load_settings[T: json](file: str, prefix: str) -> result[T] uses env, file.read`: starts
from the JSON object in `file` when it exists, then for each field of `T` an environment variable
`PREFIX_FIELD` (upper-case) overrides it (converted from text as R7's query values), then decodes with
`from_json`, so defaults and limits are the record's annotations and every problem is reported at once.
Called in `setup` or `main`; its reads are recorded as any `env`/file read.

## R13 Health and info

**Decision**: `health(checks: list[HealthCheck[C, E]]) -> Route[C, E]` (`GET /health`: 200 `{"status":
"up"}` or 503 `{"status": "down", "checks": {"name": "reason"}}`) and `info(name, version) -> Route[C, E]`
(`GET /info`), placed in the route list like any route.

## R14 Testing

**Decision**: `handle_request(app, ctx, request) -> Response uses E` is the whole application (steps,
routing, binding, errors) as a function; `request(method, path, body)` builds a `Request`
(`Content-Type: application/json` when a body is given) and `with_header(request, name, value)` adds one;
`std.json`'s readers and `parse_json(r.body)` read answers. Tests use `open_memory()` databases.

## R15 Problem bodies

**Decision**: `application/problem+json`: `{"type": "about:blank", "title": <reason phrase>, "status": N,
"detail": "...", "errors": [{"field": "text", "reason": "missing"}]}`; `detail` and `errors` only when
there is something to say.

## R16 Documentation and budget

**Decision**: reference: one paragraph for derived JSON and field annotations (Types/Records), one
`std.web` bullet group in the standard-library notes and the std table row, within +700 tokens
(to ~14,100). The guide gets an "Building an API" page; `vo doc std.web` carries the details.

## R17 Performance

**Decision**: routing is a linear scan of the routes with segment comparison (a 20-route table: at most
20 comparisons of a few segments); binding walks the input type once. SC-006 is measured with `ab` against
a hand-written `std.http` API of the same 20 routes. ponytail: linear route scan; a segment trie if tables
of hundreds of routes appear.

## R18 Verification

- Golden cases: `tests/cases/json_derive/` (each mapping row, every decode problem, E0328), `annotations/`
  (E0329 each misuse), `web/` (routing, binding, answers, errors, steps, settings, health, E0330, E0331),
  inference (R10).
- Step cases `tests/cases/web_steps/`: a server built with `serve_app` answering real requests,
  replayed.
- `tests/web_fuzz.py` (SC-003): 1,000 generated malformed requests against a 10-operation app.
- `tests/web_speed.py` (SC-006, `make gate`).
- notes-api rewritten (SC-001), counted with `tools/count_lines.py`-style line counting of API code.
- Agent benchmark (SC-004): a CRUD task with an acceptance suite, isolated agents (no tools, empty
  directory, no user settings), 20 runs with the reference including `std.web` and 20 told to use plain
  `std.http`.
