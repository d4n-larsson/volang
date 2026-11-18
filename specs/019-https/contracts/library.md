# Contract: std.http with HTTPS

The signatures do not change:

```vo
export fn get(url: str) -> result[Response] uses net
export fn post(url: str, content_type: str, body: str) -> result[Response] uses net
```

- `url` may start with `http://` or `https://` (any case). `https://` uses port 443 unless the URL
  names one. The `net` scope names the host for both schemes.
- An HTTPS request is sent only after the server's certificate chain is verified: it leads to a trusted
  authority (the machine's store plus `--ca-file`), every certificate is valid now by the machine's
  clock, and it is valid for the host (names, one-label wildcards, IP addresses). TLS 1.3 or TLS 1.2
  with forward-secret authenticated ciphers.
- Redirects are returned, not followed (unchanged); limits (16 MiB, `--timeout`) unchanged.
- The old result `err("https is not supported")` no longer occurs.

`err` messages added (HOST:PORT as in the existing network errors):

| Message |
|---|
| `the certificate of HOST:PORT is not trusted` |
| `the certificate of HOST:PORT is self-signed and not trusted` |
| `the certificate of HOST:PORT is for another host` |
| `the certificate of HOST:PORT has expired` |
| `the certificate of HOST:PORT is not valid yet` |
| `HOST:PORT offers no TLS version or algorithm that vo accepts` |
| `the connection to HOST:PORT was tampered with` |
| `the TLS handshake with HOST:PORT failed` |
| `no trusted certificate authorities found (looked in /etc/ssl/certs/ca-certificates.crt, /etc/pki/tls/certs/ca-bundle.crt, /etc/ssl/cert.pem)` |

Documentation comments in `std/http.vo` list these; `vo doc std.http.get` shows them.

## Reference text (docs/reference.md, about 150 tokens)

- Modules table: `| std.http | HTTP and HTTPS: \`get\` and \`post\` (net), \`serve\` (serve, HTTP only) |`
- A bullet in "Standard library and packages": "`https://` URLs are verified: the server's certificate
  must lead to an authority in the machine's trust store (or a file given with `--ca-file`), be valid
  now and name the host; otherwise `get`/`post` return an `err` saying why (`vo doc std.http.get` lists
  them). Nothing turns verification off. TLS 1.3 and 1.2 only. `--clock-ms` does not affect certificate
  dates. A recording keeps requests and responses, never keys; replay needs no network."
- `run` and `test` command lines gain `[--ca-file FILE]`.
