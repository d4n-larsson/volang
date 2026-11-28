# Quickstart: Events (spec 063)

Signatures and errors: [contracts/events.md](contracts/events.md). Types: [data-model.md](data-model.md).

## Reading a stream

```vo
import std.http.Header;
import std.sse.read_events;

fn main() uses console, net("localhost:8000") {
    let end = read_events("GET", "http://localhost:8000/progress", new list[Header], "", "", 0, fn(e) {
        print(e.name + ": " + e.data);
        return ok(e.name != "done");
    });
    print("last id: " + end.last_id);
}
```

## A WebSocket client

```vo
import std.http.Header;
import std.websocket.ConnectOptions;
import std.websocket.close_socket;
import std.websocket.receive_message;
import std.websocket.send_text;
import std.websocket.ws_connect;

fn main() -> result[void] uses console, net("localhost:9001") {
    let options = new ConnectOptions { headers: new list[Header], protocols: new list[str], max_message: 0 };
    let s = try map_err(ws_connect("ws://localhost:9001/echo", options, 5000), ws_error_text);
    try map_err(send_text(s, "hello"), ws_error_text);
    match receive_message(s, 5000) {
        case ok(m) { print(str(m)); }
        case err(e) { print(ws_error_text(e)); }
    }
    return map_err(close_socket(s, 1000, ""), ws_error_text);
}
```

## Serving both

```vo
# Written over any effect E, so that a test can run it on a scripted stream.
fn steps[E: effects](out: EventStream[uses E]) -> result[void] uses E, clock {
    for step in 1..6 {
        try map_err(send_event(out, new StreamEvent { name: "step", data: str(step), id: str(step) }), stream_error_text);
        sleep(1000);
    }
    return ok();
}

fn progress(ctx: Ctx, r: http.Request) -> result[http.Response] uses serve, clock {
    let options = new StreamOptions { headers: new list[Header], max_open: 4 };
    return ok(serve_events(r, options, steps));
}
# in the route table: on_request("GET", "/progress", progress)
```

A test of the function, with no port:

```vo
test "five steps" uses clock {
    let out = scripted_stream();
    let _ = steps(out);
    expect len(sent_events(out)) == 5;
}
```

## Validate

| What | Command | Expect |
|---|---|---|
| Parsing, framing, every error value | `make quick AREA=events` | the cases under `tests/cases/sse/` and `tests/cases/websocket/` pass |
| Stories 1 to 6 against local peers | `python3 tests/events.py` | every check passes; 100 of 100 replays per role match |
| A stream by an independent client | `bin/vo run examples/job-queue/main.vo &` then `curl -N localhost:8000/jobs/1/events` | events appear one at a time; `curl` ends when the job does |
| The bound | `python3 tests/events.py --only bound` | the 5th stream gets 503; `/health/live` answers throughout |
| Stopping | `python3 tests/events.py --only stop` | 60 open conversations (64 workers); WebSockets get 1001; the server exits in its usual time |
| Hostile input | `python3 tests/events.py --fuzz` | 100,000 sequences per role: error values only |
| RFC 6455 conformance | `tests/autobahn.sh` (Docker) | sections 1 to 7, 9, 10: no failed case, both roles |
| Collector | `make STRESS=1 && tests/run.sh --stress 1 tests/cases/sse tests/cases/websocket` | pass |
| Everything | `make test` | pass; reference ≤ 15,000 tokens |
| Other architectures | `make ARCH=aarch64 quick AREA=events`, then `ARCH=riscv64` | pass |
