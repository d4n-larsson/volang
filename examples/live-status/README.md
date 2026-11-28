# live-status

A service that streams the progress of a job as server-sent events, and a client that follows it. The
service answers `GET /jobs/ID/events` with one event per step and a last `done` event; each event's id
is its step, so a client whose connection was lost reads again from the last event it saw and the
service goes on from there.

## Run it

```sh
cd examples/live-status
../../bin/vo run src/main.vo &                         # the service, on port 8090 of this machine
(cd follow && ../../../bin/vo run src/main.vo)         # follows job 7 to its end
curl -N localhost:8090/jobs/7/events               # the same stream, read by curl
../../bin/vo test src/main.vo                          # the progress and the stream, no network needed
kill %1                                            # stop the service (it exits with status 0)
../../bin/vo replay recordings/demo.jsonl          # a saved session of the service
```

```text
progress: job 7: step 1 of 5
progress: job 7: step 2 of 5
progress: job 7: step 3 of 5
progress: job 7: step 4 of 5
progress: job 7: step 5 of 5
done: job 7 is done
```

## How it works

### The stream

The route's operation gets the request itself (`web.on_request`), reads `Last-Event-ID`, and answers
with `sse.serve_events`: the server sends the stream's head and calls `stream_job` with the stream,
which sends each event and waits a step. The route holds one of the server's four workers while it
streams, so `max_open: 2` keeps the other two for ordinary requests; a third client gets 503 with
`Retry-After`.

<!-- code: src/progress.vo fn stream_job -->

### Tested without a network

`stream_job` is written over any effect, `[E: effects](s: sse.EventStream[uses E], ...)`, so the test
runs it on `sse.scripted_stream()` and compares what it sent with `events_from`; the events themselves
are a pure function.

### The follower

`sse.read_events` calls the program's function with each event as it arrives and returns why the stream
ended, with the id of the last event. The follower reads again after a lost connection, at most three
times; it never needs to parse the stream itself.

<!-- code: follow/src/main.vo fn main -->

### Effects

The service declares `serve("8090")` and `clock` (it waits between steps); the follower
`net("localhost:8090")`. A recording of either replays with no network: `vo replay` gives the
follower the events it received, and the service the requests and the sends that failed.
