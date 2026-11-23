# Data Model: PostgreSQL Package

## Records and variants (module `postgres.client`)

| Type | Fields | Rules |
|---|---|---|
| `Options` | `host: str`, `port: int`, `database: str`, `user: str`, `password: str`, `plain_text: bool`, `connect_ms: int`, `statement_ms: int` | `port` 1 to 65535 (5432 usual); both limits 1 to 600000 (the `std.tcp` range); `plain_text: true` is the only way to an unencrypted connection; every field is given (vo records have no defaults), and the guide shows the usual values |
| `Database[E: effects]` | `conn: Conn[E]`, `statement_ms: int`, `in_transaction: bool`, `failed: bool`, `closed: bool` | made only by `connect`; shared by reference, so the flags are one state for every holder |
| `Statement` | `text: str` | as `std.sqlite`: the text as the program wrote it, with `?` |
| `Value` | `integer(int)`, `real(float)`, `text(str)`, `blob(bytes)`, `null` | as `std.sqlite`, same case names and order |
| `Row` | `columns: list[str]`, `values: list[Value]` | as `std.sqlite` |
| `Changes` | `changed: int`, `last_id: int` | `last_id` is always 0 |

## Connection states

```text
connect ──► ready ──exec/query──► ready
              │  ▲
   transaction│  │commit / rollback
              ▼  │
        in_transaction ──a statement fails──► in_transaction + failed ──► rollback ──► ready

any state ──timeout / connection / protocol failure, or close──► closed   (every later call: `connection: closed`)
```

- `in_transaction` is set by `transaction` and cleared when it ends; a second `transaction` while set
  is an `err`.
- `failed` follows the status byte of the server's `ReadyForQuery` (`E`).
- A statement failure outside a transaction leaves `ready`.

## One statement on the wire

```text
client:  Parse  Bind  Describe  Execute  Sync            (one send)
server:  ParseComplete  BindComplete  RowDescription | NoData
         DataRow*  CommandComplete | EmptyQueryResponse
         ReadyForQuery(status)
   or:   ErrorResponse  ReadyForQuery(status)            (at any point)
```

## Connecting on the wire

```text
client:  TLS request (8 bytes)          server:  'S' | 'N'
         [TLS handshake, server verified for host]
client:  Startup(user, database, session settings)
server:  AuthenticationOk
   or:   AuthenticationSASL(SCRAM-SHA-256)
client:  SASLInitialResponse(n,,n=,r=NONCE)
server:  SASLContinue(r=NONCE+, s=SALT, i=ROUNDS)
client:  SASLResponse(c=biws, r=..., p=PROOF)
server:  SASLFinal(v=SIGNATURE)  AuthenticationOk
server:  ParameterStatus*  BackendKeyData  ReadyForQuery
```

## Failure text

`NAME: detail`. For a failure reported by the server: `NAME: SQLSTATE message` and, when the server
gives a position, ` (at position N)` counted in the program's text. Names: [contracts/client.md](contracts/client.md).

## What a recording holds

Nothing new: the `std.tcp` events (bytes received, per wait) and the `random_bytes` result of the
nonce. Not the password, and not what the program sent.
