# Examples

Small programs that show how vo is used. Each folder is one program; run it from its folder with
`../../bin/vo run src/main.vo` and its tests with `../../bin/vo test src/main.vo`. `make test` checks and tests
every example, and replays its recordings (`recordings/*.jsonl`).

| Example | Shows |
|---|---|
| [expenses](expenses/) | an expense report from JSON: scoped file effects, `vo.effects`, `std.json`/`std.time`/`std.decimal`/`std.text`, tests, a property and a contract |
| [status-board](status-board/) | a monitoring client and its service over plain HTTP: `std.http` `get` and `serve`, the `net` and `serve` effects scoped to a host and an address, exit statuses for scripts, and record/replay of both without the network |
| [notes-api](notes-api/) | a JSON service with `std.http.serve`: routing as a pure function tested without a network, the `serve` effect scoped to an address, file storage under scoped effects, 500 for a failing request, and record/replay of a whole server session, and operation: JSON logs with request ids, `/metrics`, liveness and readiness |
| [secure-notes](secure-notes/) | access control with `std.security`: the rules of a service as one table, signing in with a token (programs) or a session cookie (browsers, through a frontend's form), the user in operations, an owner check, and tests that run every route as every kind of caller and keep the table's description |
| [rates](rates/) | a service that calls another service: a currency converter on a public exchange-rate API over HTTPS, its route tested with a fixed rate, and the call itself tested on a recorded answer (`vo test --record` / `--replay`) |
| [helpdesk](helpdesk/) | the built-in collections: a `priority_queue` of records ordered field by field, an `ordered_map` with `key_at_or_below`, `ordered_set`s with `difference`, a `deque` as a bounded history; tests per requirement and a property |
| [url-check](url-check/) | checking URLs at the same time with `concurrent_map`: results and output in input order, the same `vo.effects` as the sequential version, record/replay of a concurrent run |
| [job-queue](job-queue/) | workers with `std.workers`: three processes that take jobs and answer as they finish, `receive_first` with a time limit, a job given up with `stop_worker` while the others go on, pure work tested without workers, record/replay of a run whose order depends on timing |
| [live-status](live-status/) | server-sent events: a service streaming a job's progress with `std.sse` (a route with `web.on_request`, a bound on open streams, going on from `Last-Event-ID`), a follower that reads it with `read_events` and reads again after a lost connection, the stream tested on a scripted stream, record/replay of both |
| [release-check](release-check/) | running git with `std.process` (no shell, the `process` effect naming the program), replacing a report under `file.write` and `file.delete`, record/replay of program runs |
