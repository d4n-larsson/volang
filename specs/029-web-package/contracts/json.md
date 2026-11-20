# Contract: derived JSON conversion and field annotations (spec 029)

## std.json additions

```text
# The JSON form of any value whose type is `json` (see the table): records as objects, lists as arrays, ...
export fn to_json[T: json](v: T) -> Json

# The value of type T that j describes, checking every field's limits; err lists every problem with its path.
export fn from_json[T: json](j: Json) -> result[T]
```

`T: json` is a new constraint (beside `ordered` and `effects`): `int`, `float`, `bool`, `str`, `bytes`,
`list[T]`, `map[str, V]`, and records and variants made of them. Anything else is E0328 at the place the
type is bound, naming the part: "`Note` cannot be converted to JSON: field `on_save` is a function."

| vo | JSON |
|---|---|
| `int` / `float` / `bool` / `str` | integer / number / `true`,`false` / string |
| `bytes` | base64 string |
| `list[T]` / `map[str, V]` | array / object (map order) |
| record | object, keys = field names, declaration order |
| case without a value | `"case"` |
| case with a value | `{"case": value}` |

`from_json` errors: `"<path>: <reason>"` joined by `"; "` in document order. Reasons: `missing`, `unknown
field`, `expected <JSON type>, got <JSON type>`, `not an integer`, `out of range for int`, `unknown case
"x" (expected one of: a, b)`, `not base64`, `at least N characters`, `at most N items, got M`, `at least N`,
`at most N`, `does not match <pattern>`. Paths: `text`, `tags[2]`, `owner.name`, `(root)`.

NaN and infinite floats encode as `null` (and `null` decodes to nothing: a problem).

## Field annotations

On the lines above a record field, at most one of each:

```text
record NewNote {
    @size(1, 200)
    text: str;
    @size(0, 5)
    @default([])
    tags: list[str];
    @range(1, 5)
    @default(3)
    priority: int;
    @pattern("[a-z0-9-]+")
    slug: str;
}
```

| Annotation | Allowed on | Notes |
|---|---|---|
| `@size(min, max)` | `str` (characters), `list`, `map` | int literals, `0 <= min <= max` |
| `@range(min, max)` | `int`, `float` | literals of the field's type, `min <= max` |
| `@pattern("re")` | `str` | whole-text match, std.regex syntax, checked as `regex("...")` |
| `@default(v)` | scalars, enum-like variants (`Priority.normal`), lists (`[]` only) | must satisfy the field's other limits |
| `@header("Name")` | `str`, `int`, `float`, `bool` fields of a std.web input record | see web.md |

Annotations affect `from_json` (and std.web binding) only. Misuse is E0329.

## `new Name {}`

A record without fields is built with `new Name {}`.
