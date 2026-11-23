# Contract: codes

| Code | Name | Exit | Fixable | When |
|---|---|---|---|---|
| E0335 | `html_template_not_literal` | 65 | no | the template of `html` is not a string literal or a constant |
| E0336 | `html_template_hole` | 65 | no | a hole with something inside (`{0}`, `{name}`, `{:x}`), a single brace outside `<script>`/`<style>`, or a different number of holes and values |
| E0337 | `html_unbalanced` | 65 | no | tags that do not nest, an element left open, an end tag for a void element, `<x/>` for a non-void element, a doctype not at the start |
| E0338 | `html_unsafe_hole` | 65 | sometimes | a hole inside `<script>`/`<style>`, an `on...` or `style` attribute, a tag or attribute name, a comment, an unquoted attribute value (fix: quote it), or a place in a URL attribute where the value could choose the scheme (`{}/x`, `java{}`) |
| E0339 | `html_markup_in_attribute` | 65 | no | an `Html` or `list[Html]` value in an attribute or URL hole; a value other than `str` in a URL hole |
| E0340 | `html_opaque` | 65 | no | `new Html`, or reading or setting a field of an `Html` value, outside `std.html` |

Retired by [spec 053](../../053-remove-html/spec.md) (2026-11-23): `std.html` was removed from the core.
