# Contract: Syntax of Imports and Qualified Names

## Grammar (changes only)

```text
import_item   = "import" segment { "." segment } ";" ;        (* binds the last segment *)
module_name   = ident ;                                       (* lowercase; an imported module of this file *)
qualified     = module_name "." ( ident | TypeIdent ) ;
type_name     = TypeIdent | module_name "." TypeIdent ;       (* + type arguments as today *)
case_ref      = type_name "." ident [ "(" expr ")" ] ;
record_new    = "new" type_name "{" fields "}" ;
effect_item   = effect | TypeIdent | module_name "." TypeIdent ;
```

`module_name "." x` is a qualified name only when `module_name` is bound by an import of the same file;
otherwise `a.b` is a field access, as today.

## Paths

| Import | File loaded | Binds |
|---|---|---|
| `import notes;` | `notes.vo` beside the importer | `notes` |
| `import lib.notes;` | `lib/notes.vo` | `notes` |
| `import std.json;` | the embedded `std/json.vo` | `json` |
| `import postgres.client;` | `DIR/client.vo` of package `postgres` | `client` |
| `import std.json.parse_json;` (0.3) | `std/json.vo` (old form, R3) | `parse_json`, with E0215 |

## Examples

```vo
# file: main.vo
import geometry;
import std.json;

fn main() -> result[void] uses console {
    let p = new geometry.Point { x: 3, y: 4 };
    print(str(geometry.dist2(p)));
    let doc = try json.parse_json("{\"n\": 1}");
    match doc {
        case object(_) {
            print("object");
        }
        case other {
            print("not an object");
        }
    }
    return ok();
}
```

(`case other` stands for the remaining cases of `json.Json`, written out in the real example.)

Canonical form: `vo fmt` sorts imports by their text, one per line, before all items.
