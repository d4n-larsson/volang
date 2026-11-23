# Contract: `std.html` and the `std.web` additions

## std.html

| Item | Signature | Notes |
|---|---|---|
| `Html` | record (opaque outside the module) | markup from a checked template |
| `html` | `(template: str, values...) -> Html` | `template` a string literal or constant; `{}` holes take the values in order; `{{` `}}` braces; checked by `vo check` |
| `html_text` | `(h: Html) -> str` | the markup |
| `no_html` | `() -> Html` | the empty markup (a hole that shows nothing) |

```text
let item = html(`<li class="note"><a href="{}">{}</a></li>`, "/notes/" + str(n.id), n.text);
let page = html(`<!DOCTYPE html><html><body><h1>{}</h1><ul>{}</ul></body></html>`, title, items);
```

## std.web

| Item | Signature | Notes |
|---|---|---|
| `html_reply` | `(status: int, page: Html) -> Response` | `Content-Type: text/html; charset=utf-8`, `X-Content-Type-Options: nosniff`, `Content-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; frame-ancestors 'none'` |

Input binding: a body with `Content-Type: application/x-www-form-urlencoded` binds the input record's
fields by name with the rules for query parameters (text converted to the field's type, field
annotations, 400/422 answers). Form `bool` fields: absent is `false`; `on` and `true` are `true`. A
name both in the form and the query string is 400. Other content types (multipart included) are 415.

# Contract: diagnostics

| Code | Name | When | Fix |
|---|---|---|---|
| E0335 | html_template_not_literal | the template is not a string literal or constant | — |
| E0336 | html_template_hole | a hole with an option (`{:x}`), an index or a name, a single brace, or a wrong number of values | — |
| E0337 | html_unbalanced | a tag that closes another, an element left open, an end tag for a void element, a doctype not at the start | — |
| E0338 | html_unsafe_hole | a hole in a place no value can be safe (with the place) | an unquoted attribute hole: quote it |
| E0339 | html_markup_in_attribute | an `Html` or `list[Html]` value in an attribute or URL hole; a non-`str` value in a URL hole | — |
| E0340 | html_opaque | `new Html`, or a field of an `Html` value, outside `std.html` | — |

A value of a type that cannot be inserted at all is E0301 with the allowed types. Exit statuses and the
exact wording of each case: [codes.md](codes.md).
