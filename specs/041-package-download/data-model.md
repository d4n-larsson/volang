# Data Model: Downloading Packages

## Manifest line (`vo.packages`)

```text
NAME DIRECTORY sha256:HASH [URL]
```

| Field | Rule |
|---|---|
| NAME | an identifier other than `std` (unchanged) |
| DIRECTORY | as given, or `packages/NAME` for a fetched package without `--dir` |
| HASH | SHA-256 over the `.vo` files (unchanged: paths and contents) |
| URL | optional; starts with `https://`; no space or control byte; may end in `#PATH` |

## URL

`https://HOST[:PORT]/PATH[?QUERY][#INNER]`. `INNER` is a folder inside the archive, never sent to the
server. No `user@`.

## Archive entry (after reading)

path (relative, safe: R3) and bytes; only regular files; only those ending in `.vo` are kept.

## Package report (`--json`, kind `package`)

Existing fields `name`, `change`, `directory`, `hash`, `old_hash`; added: `url` (when fetched),
`effects` (array of text such as `net("api.example.com")`; absent when unreadable), `gained`, `lost`
(on an update), `note` (when effects could not be read). `change` gains `restored` and `present`.

## Limits

| What | Limit |
|---|---|
| download | 16 MiB |
| unpacked | 64 MiB |
| one file | 16 MiB |
| entries | 10,000 |
| a path | 4,096 bytes; a component 255 |
| redirects | 5 |
| time | 60 s per download |
