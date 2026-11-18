# Contract: Codes

## New

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| T0013 | `invalid_certificate_file` | 64 (usage) | no | `--ca-file FILE` names a file with no valid PEM certificate; message: "`FILE` holds no certificate vo can read: give a PEM file with one or more `BEGIN CERTIFICATE` blocks." |

Catalog entry (docs/errors.md): an example `vo run --ca-file key.pem main.vo` (a private key, not a
certificate) and the fixed command with the authority's certificate. Golden case
`tests/cases/https/bad_ca_file` and `tests/cases/https/explain_t0013`.

## Existing codes, new triggers

| Code | New trigger |
|---|---|
| T0001 | `--ca-file` with a command other than `run` or `test` (including `replay`) |
| T0002 | `--ca-file FILE` cannot be read |
| R0010 | an `https://` host outside the running grant, before any connection |
| R0014 | a replayed request whose scheme differs from the recorded one (http against https) |
