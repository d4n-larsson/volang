# Contract: Catalog Codes (spec 033)

No new code. E0309 keeps its code and meaning ("this type is not a map key"); the set of key types grows
and the message names the part that is not a key (research R3). Messages: [diagnostics.md](diagnostics.md).

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0309 | `invalid_map_key` | 65 | sometimes | a map key type, or a type given to a `K: key` parameter, contains a part that is not a key |
