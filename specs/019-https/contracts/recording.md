# Contract: Recording

Schema stays 1. The `http` event gains an optional field:

```json
{"kind":"http","host":"api.example.com","port":443,"scheme":"https","request":"GET /v1/items","ok":true,"value":"HTTP/1.1 200 OK\r\n..."}
```

- `scheme` is written only for HTTPS; absent means `http` (all existing recordings).
- Replay matches scheme, host, port and request line; any difference is R0014 with both descriptions,
  e.g. "`GET /v1/items` to `https://api.example.com:443`" against "... `http://...`".
- Nothing about the TLS connection is recorded.
