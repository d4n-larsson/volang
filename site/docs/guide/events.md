# Events and WebSockets

Some conversations stay open: a service reads a stream of events from another system (a model's
answer as it is written, a build's progress, a feed of changes), holds a WebSocket with a system that
offers nothing else, or sends a frontend its own progress as it happens. `std.sse` reads and answers
server-sent event streams; `std.websocket` connects to and accepts WebSockets. Neither needs parsing
or framing code of its own: a program handles events and whole messages.

Both work under the effects you already know: reading a stream or opening a WebSocket needs `net`
with the host (and port) in its scope, answering one needs `serve`. Everything received is recorded by
`vo run --record`, and `vo replay` opens no connection.

## Reading an event stream

`read_events(method, url, headers, body, last_id, timeout_ms, f)` sends the request and calls `f` with
each event as it arrives: its `name` (`"message"` when the stream gave none), its `data` (lines joined
with `"\n"`) and the last `id` the stream set. `f` answers `ok(true)` to go on, `ok(false)` to stop.
The result says why the stream ended and the id to go on from.

```vo
import std.http;
import std.sse;

fn main() uses console, net("localhost:8090") {
    let end = sse.read_events("GET", "http://localhost:8090/jobs/7/events", new list[http.Header], "", "", 0, fn(e) {
        print(e.name + ": " + e.data);
        return ok(e.name != "done");
    });
    print("last id: " + end.last_id);
}
```

`timeout_ms` bounds each wait for the next bytes, not the whole stream (0 means the most, ten
minutes); a server that means to stay silent sends comments, which keep the stream alive. Headers and
a body are sent as with any request, so a POST with a JSON question and an `Authorization` header is
one call.

`StreamEnd.why` is one of `ended` (the server ended it), `broken` (the connection was lost, or a
chunked body ended early), `timed_out`, `stopped` (`f` said so), `not_a_stream(status)`, `too_large`
(an event over 1 MiB) and `failed(text)`. `read_events` never reconnects by itself: when the program
wants to go on, it calls again with `end.last_id`, which is sent as `Last-Event-ID`, and the server
can continue after it.

`parse_events(text)` applies the same rules to text a program already has.

### Reading in a worker

To wait on a stream and something else at once, read it in a [worker](workers.md) and pass each event
on as a message: the program then uses `receive_first` over the stream and its other sources, and
`stop_worker` closes the connection.

```vo
import std.http;
import std.sse;
import std.workers;

fn reader(link: workers.Link[int, sse.StreamEvent]) uses net("localhost:8090") {
    let _ = sse.read_events("GET", "http://localhost:8090/feed", new list[http.Header], "", "", 0, fn(e) => ok(workers.emit(link, e) == ok()));
}
```

## A WebSocket client

`ws_connect(url, options, timeout_ms)` opens a `ws://` or `wss://` address; a `wss://` server's
certificate is checked as for `https://`. The socket sends text and binary messages and receives whole
ones; pings from the other side are answered for you.

```vo
import std.http;
import std.websocket;

fn main() -> result[void] uses console, net("localhost:9001") {
    let options = new websocket.ConnectOptions { headers: new list[http.Header], protocols: new list[str], max_message: 0 };
    let s = try map_err(websocket.ws_connect("ws://localhost:9001/echo", options, 5000), websocket.ws_error_text);
    try map_err(websocket.send_text(s, "hello"), websocket.ws_error_text);
    let reply = try map_err(websocket.receive_message(s, 5000), websocket.ws_error_text);
    match reply {
        case text(t) {
            print(t);
        }
        case binary(_) {
            print("binary");
        }
    }
    return map_err(websocket.close_socket(s, 1000, ""), websocket.ws_error_text);
}
```

Every failure is a `WsError`: `nothing_in_time` (the socket is still usable), `closed` with the other
side's code and reason (every later call says the same), `broken`, `protocol` when the other side
broke the rules, `not_text`, `too_large` (a message over `max_message`, 1 MiB unless set, at most
16 MiB), `refused(status)` and `connect(error)` from `ws_connect`. Nothing a peer sends stops the
program with a runtime error.

## Answering with a stream

`serve_events(request, options, f)` is called inside an ordinary handler. It sends the stream's head,
calls `f` with the stream, and ends the stream when `f` returns. `send_event` puts one event on its
way; `keep_alive` sends a comment that keeps proxies from closing a quiet connection.

```vo
import std.http;
import std.sse;
import std.time;

fn steps[E: effects](s: sse.EventStream[uses E]) -> result[void] uses E, clock {
    for i in 1..6 {
        try map_err(sse.send_event(s, new sse.StreamEvent { name: "step", data: str(i), id: str(i) }), sse.stream_error_text);
        time.sleep(1000);
    }
    return ok();
}

fn handle(r: http.Request) -> http.Response uses serve, clock {
    let options = new sse.StreamOptions { headers: new list[http.Header], max_open: 4 };
    return sse.serve_events(r, options, steps);
}

fn setup() -> result[int] {
    return ok(0);
}

fn main() -> result[void] uses clock, serve("8090") {
    return http.serve_workers("8090", 8, setup, fn(n, r) => handle(r));
}
```

A send fails with `client_gone` when the client has left (within a second of the next send),
`not_reading` when it took nothing for 30 seconds, and `stopping` when the server is told to stop;
`f` then returns. A client that closes its own side of the connection after sending its request is
taken to have left. The client's `Last-Event-ID` is an ordinary request header.

In a `std.web` application the route is an `on_request` route, whose operation gets the request:

```vo
import std.http;
import std.sse;
import std.web;

fn quiet[E: effects](s: sse.EventStream[uses E]) -> result[void] uses E {
    return map_err(sse.keep_alive(s), sse.stream_error_text);
}

fn events(n: int, r: http.Request, vars: map[str, str]) -> result[http.Response] uses serve {
    let options = new sse.StreamOptions { headers: new list[http.Header], max_open: 2 };
    return ok(sse.serve_events(r, options, quiet));
}

fn routes() -> list[web.Route[int, uses serve]] {
    return [web.on_request("GET", "/jobs/{id}/events", events)];
}
```

The application's steps run before the stream opens, so a sign-in step refuses a request before any
event is sent. A step that adds headers *after* the handler (CORS) comes too late for a stream, whose
head is already sent: give such headers in `StreamOptions.headers`.

## Accepting a WebSocket

`ws_accept(request, options, f)` checks the request, answers the handshake, calls `f` with the socket,
and closes the conversation when `f` returns (code 1000, or 1011 when `f` returns an error). `f` uses
the same operations as a client.

```vo
import std.http;
import std.websocket;

fn echo[E: effects](s: websocket.Socket[uses E]) -> result[void] uses E {
    while true {
        match websocket.receive_message(s, 0) {
            case ok(m) {
                match m {
                    case text(t) {
                        try map_err(websocket.send_text(s, t), websocket.ws_error_text);
                    }
                    case binary(b) {
                        try map_err(websocket.send_binary(s, b), websocket.ws_error_text);
                    }
                }
            }
            case err(_) {
                return ok();
            }
        }
    }
}

fn handle(r: http.Request) -> http.Response uses serve {
    let options = new websocket.AcceptOptions { origins: ["https://app.example"], protocols: new list[str], max_message: 0, max_open: 8 };
    return websocket.ws_accept(r, options, echo);
}
```

`origins` names the browser pages that may connect, exactly as browsers send them: a request from any
other origin gets 403 before `f` runs. `[]` admits only clients that send no `Origin` (other
programs), and `["*"]` admits every site, in words a reviewer can find. A request that is not a
WebSocket gets 400 or 426. When the client offers sub-protocols, the first one that `protocols` also
names is chosen, and `s.protocol` says which.

## How many conversations a server holds

A conversation holds one of `serve_workers`' processes for as long as it lasts, where a request holds
one for milliseconds. Each route therefore says how many of its conversations the server holds at once
(`max_open`; 0 means half the workers), and the server never gives away its last free worker: a
request beyond the bound is answered 503 with `Retry-After`, and ordinary requests go on being answered.
A server needs at least two workers to hold a conversation. Under `serve`, which answers one request at
a time, a conversation is possible but the server answers nothing else while it lasts.

When the server is told to stop (SIGTERM or Ctrl-C, with or without `--drain`), each WebSocket is
closed with code 1001 ("going away") and each stream ends; a handler that ignores this is ended at the
drain limit, like a slow request. `--timeout` limits a handler until it opens its conversation, not the
conversation itself, which its time limits, its client and the bound keep in check.

With `request_logs` a conversation is logged once, when it ends, with its duration and why it ended
(`end`: `done`, `client_gone`, `closed 1000`, ...). The gauge `http_conversations_open` counts the
conversations being held.

## Testing

Write the function that talks over any effect, `[E: effects](s: Socket[uses E])` or
`(s: EventStream[uses E])`, and a test runs it on a scripted socket or stream, with no port and no
effect: `scripted_socket(incoming)` gives the messages of `incoming` and then a close, and
`scripted_stream()` keeps what is sent.

```vo-test
import std.websocket;

fn shout[E: effects](s: websocket.Socket[uses E]) -> result[void] uses E {
    match websocket.receive_message(s, 0) {
        case ok(m) {
            match m {
                case text(t) {
                    return map_err(websocket.send_text(s, upper(t)), websocket.ws_error_text);
                }
                case binary(_) {
                    return ok();
                }
            }
        }
        case err(_) {
            return ok();
        }
    }
}

test "answers in capitals" {
    let s = websocket.scripted_socket([websocket.Message.text("hi")]);
    expect(shout(s) == ok());
    expect(websocket.sent_messages(s) == [websocket.Message.text("HI")]);
}
```

```output
PASS answers in capitals (0ms)
1 passed, 0 failed
```

The client side is tested on a recording: `vo test --record` keeps what a test received from a stream
or a WebSocket (and what it sent, with `Authorization` and `Cookie` values as `[redacted]`), and
`vo test --replay` runs it offline, reporting the first send that differs.

## Replay

A recording of a program holds every event and message it received, every time limit that passed and
how each conversation ended; `vo replay` gives them back in the same order without a network. For a
server, the recording holds each conversation with its request, and the handler runs again on it.
A conversation's part of a recording grows until it ends, so record conversations of bounded length.

## What is left out

No message for every connected client at once (a chat room: handlers run in separate processes that
share nothing), no automatic reconnection (the program reads again from `last_id`), no compression,
no fragments in the program's hands (messages are whole, at most 16 MiB), and no HTTP/2.
