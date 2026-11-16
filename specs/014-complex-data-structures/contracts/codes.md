# Contract: Catalog Codes (spec 014)

No new codes. Writing a collection type the wrong way is a syntax error (E0110), as for `list` and
`map`.

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|

Changed messages (no new code): E0313 names the first field without an order when the type is a
record, and names the `_by` constructor when `new ordered_map[...]` gets a key type with no order.
