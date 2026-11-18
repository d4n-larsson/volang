# Contract: Command line

```text
vo run  [...] [--ca-file FILE]... FILE [-- ARG...]
vo test [...] [--ca-file FILE]... PATH...
```

- `--ca-file FILE` (repeatable): PEM file with one or more certificates of authorities to trust in
  addition to the machine's trust store, for this run only. Read at startup: T0002 if unreadable,
  T0013 if it holds no certificate. Not recorded; `vo replay` does not take it (T0001) and never needs
  it.
- No option, environment variable or function disables or weakens certificate verification.

Machine trust store: the first existing of `/etc/ssl/certs/ca-certificates.crt`,
`/etc/pki/tls/certs/ca-bundle.crt`, `/etc/ssl/cert.pem`, read on the first HTTPS request.
