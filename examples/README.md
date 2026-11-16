# Examples

Small programs that show how vo is used. Each folder is one program; run it from its folder with
`../../bin/vo run main.vo` and its tests with `../../bin/vo test main.vo`. `make test` checks and tests
every example, and replays its recordings (`recordings/*.jsonl`).

| Example | Shows |
|---|---|
| [expenses](expenses/) | an expense report from JSON: scoped file effects, `vo.effects`, `std.json`/`std.time`/`std.text`, tests, a property and a contract |
| [status-board](status-board/) | a monitoring client and its service over plain HTTP: `std.http` `get` and `serve`, the `net` and `serve` effects scoped to a host and an address, exit statuses for scripts, and record/replay of both without the network |
| [notes-api](notes-api/) | a JSON service with `std.http.serve`: routing as a pure function tested without a network, the `serve` effect scoped to an address, file storage under scoped effects, 500 for a failing request, and record/replay of a whole server session |
