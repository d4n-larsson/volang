# Examples

Small programs that show how vo is used. Each folder is one program; run it from its folder with
`../../bin/vo run main.vo` and its tests with `../../bin/vo test main.vo`. `make test` checks and tests
every example, and replays its recordings (`recordings/*.jsonl`).

| Example | Shows |
|---|---|
| [expenses](expenses/) | an expense report from JSON: scoped file effects, `vo.effects`, `std.json`/`std.time`/`std.decimal`/`std.text`, tests, a property and a contract |
| [status-board](status-board/) | a monitoring client and its service over plain HTTP: `std.http` `get` and `serve`, the `net` and `serve` effects scoped to a host and an address, exit statuses for scripts, and record/replay of both without the network |
| [notes-api](notes-api/) | a JSON service with `std.http.serve`: routing as a pure function tested without a network, the `serve` effect scoped to an address, file storage under scoped effects, 500 for a failing request, and record/replay of a whole server session |
| [notes-pages](notes-pages/) | web pages with `std.html`: templates checked by `vo check` and escaped by place, pages composed from `Html` values, HTML forms bound by `std.web`, one note swapped in by a script the app serves itself |
| [secure-notes](secure-notes/) | access control with `std.security`: the rules of a service as one table, signing in for a token, the user in operations, an owner check, and tests that run every route as every kind of caller and keep the table's description |
| [helpdesk](helpdesk/) | the built-in collections: a `priority_queue` of records ordered field by field, an `ordered_map` with `key_at_or_below`, `ordered_set`s with `difference`, a `deque` as a bounded history; tests per requirement and a property |
| [url-check](url-check/) | checking URLs at the same time with `concurrent_map`: results and output in input order, the same `vo.effects` as the sequential version, record/replay of a concurrent run |
| [job-queue](job-queue/) | workers with `std.workers`: three processes that take jobs and answer as they finish, `receive_first` with a time limit, a job given up with `stop_worker` while the others go on, pure work tested without workers, record/replay of a run whose order depends on timing |
| [release-check](release-check/) | running git with `std.process` (no shell, the `process` effect naming the program), replacing a report under `file.write` and `file.delete`, record/replay of program runs |
