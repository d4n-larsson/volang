# Quickstart: validating concurrency (spec 016)

Prerequisites: a native build (`make`), Python 3, and for the other platforms the cross builds and
`qemu-riscv64`/`qemu-aarch64` as for the existing gate. The delaying route `GET /delay/MS` of
`tests/http_server.py` (research R16) must be in place.

## 1. Ten slow requests at once (US1, SC-001)

```sh
python3 tests/http_server.py 8080 &            # prints "port 8080"
cat > /tmp/fetch.vo <<'EOF'
import std.http.get;

fn fetch(i: int) -> str uses net("127.0.0.1") {
    match get("http://127.0.0.1:8080/delay/1000") {
        case ok(r) { return str(i) + " " + str(r.status); }
        case err(e) { return str(i) + " " + e; }
    }
}

fn main() uses console, net("127.0.0.1") {
    for line in concurrent_map([0, 1, 2, 3, 4, 5, 6, 7, 8, 9], 10, fetch) {
        print(line);
    }
}
EOF
time bin/vo run /tmp/fetch.vo
```

Expected: `0 200` … `9 200` in order, in under 3 s (`map` instead: at least 10 s). With limit 3: the same
lines, about 4 s.

## 2. Same output every run (US1 scenario 2, SC-003)

```sh
python3 tests/tasks_determinism.py --runs 100     # the tasks/ golden cases, server delays randomized
```

Expected: every run byte-identical to the golden output; prints `100/100 identical` per case.

## 3. Record and replay (FR-011)

```sh
bin/vo run --record /tmp/fetch.rec /tmp/fetch.vo > /tmp/a.txt
kill %1                                          # no server any more
bin/vo replay /tmp/fetch.rec > /tmp/b.txt && cmp /tmp/a.txt /tmp/b.txt
```

Expected: identical output, exit 0, no connection attempted. `tests/cross_replay.py` replays the same
recording on the other two platforms.

## 4. Copies, not sharing (US4, SC-004)

Golden cases in `tests/cases/tasks/`: a task appends to its list argument and changes a captured record;
the caller prints both afterwards, unchanged. The property `concurrent_map(xs, k, f) == map(xs, f)` for
generated `xs` and `k` runs under 10 seeds (`tests/tasks_props.py`).

## 5. Failure has one place to go (US3)

Golden cases: the task at position 2 of 5 divides by zero → output of tasks 0–2, then the R0002 report
naming the task's line with the trace through `concurrent_map`, exit 70; two failing tasks → the lower
one is reported; `--timeout 2` with a task fetching `/slow` → R0009, exit 124 after about 2 s; a task
calling `exit(3)` → exit 3. `ps` after each shows no leftover `vo` process.

## 6. A slow client does not stall the server (US2, SC-002)

Step case `tests/cases/serve_concurrent/`: start a server, `request-raw` half a request on one
connection, then 100 ordinary requests, each answered within 1 s; after 10 s the half request gets 408.
A handler counting requests in a captured record answers 1..100 in order of completion.

## 7. Review sees nothing new (US5, SC-005)

```sh
bin/vo effects --update examples/url_check/main.vo     # sequential version
# change map( to concurrent_map( ..., 8, ...)
bin/vo check examples/url_check/main.vo                # no E0504, no W0502
```

## 8. The gate

`make test` (suite, reference examples, catalog coverage incl. E0314/E0511/R0015), the fuzzer, the
determinism check, the instrumented build, on x86-64, ARM64 and RISC-V; `tests/ref_examples.py` reports
the reference under 15,000 tokens and at most 400 above the previous release.
