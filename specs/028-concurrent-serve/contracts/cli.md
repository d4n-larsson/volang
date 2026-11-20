# Contract: CLI (spec 028)

## `vo run --workers N`

`N` from 1 to 64 (else T0001). Replaces the `workers` argument of every `serve_workers` and
`serve_bytes_workers` call in the run. On a program that serves with `serve`/`serve_bytes` only, it
changes nothing; the first such call prints to stderr:

```text
note: --workers has no effect: this program serves one request at a time (serve_workers runs several)
```

`--workers` is accepted by `vo run` only; `vo replay`, `vo test` and the other commands reject it (T0001).

## Notes on stderr while serving with workers

```text
note: worker N ended unexpectedly; it was replaced
```

With `--json`, notes are JSON records of kind `note` with a `message`, as other notes.
