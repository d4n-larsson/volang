# Rates

A small service that calls another one: `GET /convert?amount=100&from=EUR&to=SEK` answers the amount in
the other currency at the latest rate from [api.frankfurter.dev](https://api.frankfurter.dev) (public,
no key). It shows how code that talks to an outside service is tested in vo: without mocks, on an
answer the real service gave once.

## Run it

```sh
cd examples/rates
../../bin/vo run main.vo                                   # serve on http://localhost:8080 (Ctrl-C stops)
../../bin/vo test --replay recordings/tests main.vo        # every test, offline
../../bin/vo test --record recordings/tests main.vo        # ask the real service again and save its answer
```

## How it is tested

The converter's route takes its rate from a function in the context (`Rates`), so its tests in
`convert.vo` pass a fixed rate and need no network:

<!-- code: convert.vo test "converts an amount at the rate" -->

The call to the real service is tested once, in `rates.vo`:

<!-- code: rates.vo test "the service gives a positive rate from EUR to SEK" -->

`vo test --record recordings/tests main.vo` runs it against the service and saves what it received in
`recordings/tests/rates/the_service_gives_a_positive_rate_from_EUR_to_SEK.jsonl`: the request it sent
and the answer, as readable JSON lines. `vo test --replay recordings/tests main.vo` runs it on that file
with no network, in a millisecond, the same every time; `make test` does that. If the code asks the
service something else (another currency, another path), the replayed test fails with R0014 naming the
request it made and the one recorded, until it is recorded again (`--test` records one test only).

A recording never holds the values of `Authorization`, `Cookie` or `Set-Cookie`, nor of the headers,
query parameters and variables named with `--redact`, so it is safe to commit; this API needs no key.

## Effects

`main` declares `serve("8080")` and `net("api.frankfurter.dev")`: the service can reach that host and no
other, and the recorded test declares the same scope.
