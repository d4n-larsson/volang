# Data Model: Hashing and Secrets

## Stored password (text)

```text
pbkdf2-sha256$ITERATIONS$SALT$KEY
```

| Part | Rule |
|---|---|
| method | exactly `pbkdf2-sha256` |
| ITERATIONS | decimal, 1 to 10,000,000; `hash_password` writes 600000 |
| SALT | standard base64 of the salt; `hash_password` draws 16 bytes |
| KEY | standard base64 of exactly 32 bytes |

Anything that does not fit: `verify_password` answers false.

## Effect

`entropy`: bit 11, after `process`. No scopes. Carried by `random_bytes` and `hash_password`.

## Recording event

One event per `random_bytes` call: its kind (`entropy`), the count, and the bytes as hex. Replay returns
the bytes of the next such event; another count is R0014.
