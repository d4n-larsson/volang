# Contract: diagnostics (spec 029)

| Code | Name | When |
|---|---|---|
| E0328 | not_json | A type bound to a `json` parameter (`to_json`, `from_json`, std.web) contains a function, `map[int, V]`, an ordered collection, a `result`, or a record with effect parameters; the message names the path to it. |
| E0329 | bad_field_annotation | A field annotation that is unknown (fix: the nearest name), repeated, on a field of the wrong type, with arguments of the wrong kind, `min > max`, or a `@default` that breaks the field's limits; `@header` outside a record used as std.web input. |
| E0330 | bad_route_pattern | An `on_*` pattern that is not a string literal, is malformed (position given), repeats a variable, or names a variable the input record lacks or cannot bind from a path. |
| E0331 | route_conflict | Two routes of one list literal with the same method that match the same paths, or a route that an earlier one hides; related locations name both. |

No runtime codes are added: binding failures are answers (400/415/422), not runtime errors.
