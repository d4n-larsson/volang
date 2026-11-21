# Data Model: editor grammars

## Token classes

One row per class the grammars must agree on. "Checked" means R5 compares the two grammars there.

| Class | tree-sitter node or capture | TextMate scope (prefix) | Vim group | Checked |
|---|---|---|---|---|
| comment | `comment` | `comment.line.number-sign` | Comment | yes |
| string | `string` | `string.quoted.double` | String | yes |
| escape | `escape_sequence` | `constant.character.escape` | SpecialChar | yes |
| integer | `integer` | `constant.numeric.integer` | Number | yes |
| float | `float` | `constant.numeric.float` | Float | yes |
| boolean | `true`, `false` | `constant.language` | Boolean | yes |
| keyword | each keyword token | `keyword.` or `storage.` | Keyword, Conditional, Repeat, Statement | yes |
| builtin type | builtin type tokens | `support.type` | Type | yes |
| type name | `type_identifier` | `entity.name.type` | Type | yes |
| annotation | `annotation` name | `entity.name.tag.annotation` | PreProc | yes |
| effect | effect name in `uses_clause` | `support.constant.effect` | Constant | yes |
| function definition | name in `function_definition` | `entity.name.function` | Function | yes |
| call | function in `call_expression` | `entity.name.function.call` | (none) | no |
| operator, punctuation | anonymous tokens | `keyword.operator`, `punctuation` | Operator, Delimiter | no |

## Word lists

`keywords`, `types`, `contextual`, `effects`, `literals`: sets of words, sourced per research R1, and
required equal in: the TextMate grammar, `grammar.js`, `syntax/vo.vim`, `pygments_vo.py`.

## Corpus

`parses`: files where `vo fmt --check` exits 0 or 1. `rejected`: the rest. Both from `tests/cases`,
`examples`, `std`.
