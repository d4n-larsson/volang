# Data Model: HTML Pages

## `Html` (std.html)

| Field | Type | Rule |
|---|---|---|
| `text` | `str` | markup from a checked template; readable and writable only inside `std.html` (E0340 elsewhere) |

Not a `json` type. Made only by `html(...)` and `no_html()`.

## A template, as the checker sees it

| Part | Meaning |
|---|---|
| literal piece | template text between holes, `{{`/`}}` turned into braces; copied as it is |
| hole | `{}`; has a place and the type of its value |

| Place | Where | Values allowed | Escape |
|---|---|---|---|
| text | element content, also `textarea`, `title` | `str`, `int`, `float`, `bool`, `Html`, `list[Html]` | text (markup values as they are) |
| attribute | the whole or part of a quoted attribute value | `str`, `int`, `float`, `bool` | attribute |
| URL | the whole quoted value of `href`, `src`, `action`, `formaction`, `poster`, `cite`, `object data` | `str` | scheme check, then attribute |
| refused (E0338) | `script`/`style` content, `on...`/`style` attributes, unquoted values, names, comments, part of a URL value | — | — |

## Tokenizer states

```text
text ──<name──► tag ──name──► attribute name ──=──► value ──"/'──► quoted value ──"/'──► tag
  ▲                │                                   └──other──► unquoted value
  │                └──>──► text, or raw text (script, style) / escapable raw text (textarea, title)
  └──<!--──► comment ──-->──┘
```

Open elements are a stack; a void element is never pushed.

## The request form of std.web (extended)

```text
{ "path": {name: text}, "query": {name: [text]}, "headers": {name: text},
  "body": json        (application/json)
  "form": {name: [text]} (application/x-www-form-urlencoded) }
```

A field is taken from path, then headers (`@header`), then form, then query; a name given in both form
and query is a 400. Form `bool` fields: absent → `false`, `on` or `true` → `true`.
