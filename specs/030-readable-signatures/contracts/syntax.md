# Contract: syntax and meaning (spec 030)

```text
item      := ... | ["export"] "effects" TypeName "=" effect ("," effect)* ";"
effect    := name ["(" scope ("," scope)* ")"] | TypeName        # TypeName: an effect parameter or a set
```

```vo
effects Storage = file.read("data/"), file.write("data/");
export effects Network = net("api.example.com");

fn save(db: Database[uses Storage], n: Note) -> result[void] uses Storage { ... }
fn sync() -> result[void] uses console, Network, Storage { ... }
```

| Written | Means |
|---|---|
| `uses Storage` | `uses file.read("data/"), file.write("data/")` |
| `uses console, Storage` | the union |
| `effects App = console, Storage;` | a set of both |
| `Database[uses Storage]` | `Database[uses file.read, file.write]` (scopes dropped in types) |
| `fn(Note) -> result[void] uses Storage` | that function type with the set's effects |

Canonical order in a clause (`vo fmt`): plain effects in today's order, then set names alphabetically.
