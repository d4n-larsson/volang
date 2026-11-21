# Contract: `vo package` with a URL

```text
vo package [--json] ENTRY NAME DIRECTORY                              (unchanged)
vo package [--json] [--dir DIRECTORY] [--ca-file FILE] ENTRY NAME URL
vo package [--json] [--ca-file FILE] --restore ENTRY
vo package [--json] --remove ENTRY NAME                               (unchanged)
```

## Fetching (`URL` starts with `https://`)

1. GET the URL (5 redirects, 60 s, 16 MiB, certificates verified).
2. gzip if it starts with the gzip magic, else tar as it is.
3. Refuse the archive if any entry is a link, device or FIFO, or has an unsafe path.
4. Drop a single common top folder; with `#PATH`, keep what is under it; keep `*.vo`.
5. Write into `--dir`, or `packages/NAME` beside the manifest; pin; record the URL.

Output, human form:

```text
added money packages/money sha256:1e5b... from https://example.com/money-1.2.0.tar.gz
  effects: none
updated money: sha256:1e5b... -> sha256:91c0...
  effects: net("rates.example.com")
  gains: net("rates.example.com")
  loses: file.read
unchanged money
```

`--restore`: one line per entry, `restored NAME`, `present NAME`, or the entry's error; the manifest is
not changed. With `--json` these are `package` records whose `change` is `restored` or `present`: two
new values of an existing field.

## Codes and statuses

| Code | When | Exit |
|---|---|---|
| T0001 | wrong operands; `--dir` or `--ca-file` without a URL; a URL that is not `https://`, has credentials, or has an unsafe `#PATH` | 64 |
| E0706 `package_fetch_failed` | the download failed (connection, certificate, status, redirects, size, time) or the archive was refused (corrupt, truncated, unsupported entry, unsafe path, a limit) | 65 |
| E0702 | no `.vo` file in the archive (under `#PATH`); `--restore`: a missing directory without a URL | 65 |
| E0701 | `--restore`: the fetched content, or a present directory, does not match the pin; an update: the directory to replace was changed since it was pinned | 65 |
| E0705 | the name clashes with a directory beside the entry file (unchanged) | 65 |

On every failure of the URL form the manifest and the package's previous directory are unchanged.

## Guarantees

- `check`, `run`, `test`, `build`, `doc` and every other command never open a connection for packages.
- After a successful add, the package is exactly what its hash says; the URL is a note of where it
  came from and what `--restore` asks again.
