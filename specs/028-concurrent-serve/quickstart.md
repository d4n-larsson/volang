# Quickstart: validating spec 028

Prerequisites: `make` (builds `build/vo`; `bin/vo` runs it), Python 3, a machine with at least 4 cores
for the throughput check.

## 1. A slow request does not delay fast ones

A program with `/slow` (sleeps 2 s) and `/fast`, served by `serve_workers("8080", 4, setup, handle)`:

```text
bin/vo run main.vo &
curl localhost:8080/slow & curl localhost:8080/slow & curl localhost:8080/slow &
time curl localhost:8080/fast      # well under 50 ms
```

With `serve` instead, the fast request waits for the slow ones (today's behavior).

## 2. Isolation, shutdown, state

`python3 tests/serve_workers.py` runs: a crashing, a timing-out and a memory-exhausting request among 20
(the others answered); SIGTERM with 5 slow requests in flight (all answered, exit 0); 1,000 increments of
a SQLite counter through 4 workers (exactly 1,000); the notes-api example under 1,000 concurrent requests
(no lost note); a recorded 200-request session replayed 20 times (identical responses).

## 3. Overrides and notes

`bin/vo run --workers 2 main.vo` runs 2 workers; on a `serve` program it prints the note of
contracts/cli.md and serves one request at a time.

## 4. Throughput

`python3 tests/serve_workers_speed.py` (in `make gate`): a computing handler, 1 vs 4 workers, ≥ 3×.
