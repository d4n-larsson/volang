# Examples

Small programs that show how vo is used. Each folder is one program; run it from its folder with
`../../bin/vo run main.vo` and its tests with `../../bin/vo test main.vo`. `make test` checks and tests
every example, and replays its recordings (`recordings/*.jsonl`).

| Example | Shows |
|---|---|
| [expenses](expenses/) | an expense report from JSON: scoped file effects, `vo.effects`, `std.json`/`std.time`/`std.text`, tests, a property and a contract |
| [status-board](status-board/) | a monitoring client over plain HTTP: `std.http`, the `net` effect scoped to a host, exit statuses for scripts, and record/replay of a run without the network |
