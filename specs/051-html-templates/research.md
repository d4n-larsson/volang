# Research: HTML Pages (`std.html`)

Code read on 2026-11-23: `src/check_format.c`, `src/check_expr.c` (calls of standard-library
functions), `src/compile.c` (`cformat`), `std/web.vo` (`bind`, `query_values`), `src/json_derive.c`
(the request form, mode bits), `src/check_generic.c` (`json_why`).

## R1. `html(template, values...)` is a library item whose calls the checker turns into a builtin

**Finding**: vo functions have fixed parameters, and `format`, the only call with a variable number of
values of mixed types, is a builtin (`B_FORMAT`), checked by `check_format_call` and lowered by
`cformat`. Builtins are global names; `Html` should be imported from `std.html` like every other item
of a module. Calls of library functions already get literal checks by declaration
(`check_regex_call`, `check_sqlite_call` in `check_expr.c`).

**Decision**: `std/html.vo` declares `export fn html(template: str) -> Html` with a body that is never
run (it returns the empty value). In `check_expr.c`, before the argument count is checked, a call whose
declaration is that function goes to `check_html_call(call)`, which checks the template and every value
(any count), stores the pieces on the call (`call->e.p`, as `format` does) and sets `call->d.i =
B_HTML`. From there it is a builtin call: the compiler lowers it, `vo doc` and hover still show the
declaration (with a note that it takes the template's values after it).

**Alternatives**: `html` as a global builtin (no import, but `Html` would come from a module anyway:
two places for one feature); values as a list of a variant (`[text(a), markup(b)]`: verbose, and the
variant would be a second way to say what the hole's place already says); template files (read at
check and run time; out of scope in the spec).

## R2. Lowering: `format`'s pieces with an escape per hole

**Finding**: `cformat` stores the literal pieces and emits one append per piece and per value, with the
value converted to text (`FmtPiece` and `B_FMT_PIECE`).

**Decision**: the same lowering, with each hole's piece carrying its place (text, attribute, URL) and
its value's kind: a `str` (or `int`, `float`, `bool`, through `str(x)` first) is appended through
`__html_escape(place, s)`; an `Html` value's text is appended as it is; a `list[Html]` is appended
element by element. The result is wrapped as an `Html` record. `{{` and `}}` become braces, as in
`format`. Options after a colon (`{:x}`) are E0336: format first.

## R3. The template tokenizer

**Decision**: a tokenizer in `src/check_html.c` over the decoded template, enough for the rules and no
more (not a full HTML5 parser, which the template does not need: the template is written by the
program's author and checked; only the values are hostile, and they never reach the tokenizer):

- **States**: text; tag open (`<name`, `</name`); attribute name; after `=`; quoted value (`"` or `'`);
  unquoted value; comment (`<!-- ... -->`); doctype (`<!DOCTYPE html>`, only at the start); raw text
  inside `script` and `style` (to their end tag); escapable raw text inside `textarea` and `title`.
- **Nesting**: a stack of open elements. Void elements (`area base br col embed hr img input link meta
  source track wbr`) are never pushed; their end tag is E0337. The end of the template with an element open is E0337, also for `p`, `li`,
  `td` and the others whose end tags HTML lets a page leave out: one rule for every element, so a
  fragment inserted into a hole is always balanced. A template may be a fragment: several top-level
  elements and text.
- **Places of a hole**: text (also inside `textarea` and `title`); a quoted attribute value; a URL
  attribute value (`href`, `src`, `action`, `formaction`, `poster`, `cite`, `data` on `object`) when the
  hole is the whole quoted value; anything else is E0338 with the reason: inside `script`/`style`, an
  `on...` or `style` attribute, an unquoted value (fix: add `"`), a tag or attribute name, a comment, a
  URL attribute where the hole is only part of the value (the scheme check needs the whole value).
- Attribute and tag names are compared in ASCII lower case.

## R4. Escaping

**Decision**: one builtin, `__html_escape(place, s)`, in C:
- text and attribute: `&` `<` `>` `"` `'` as `&amp;` `&lt;` `&gt;` `&quot;` `&#39;`;
- URL: first the scheme check: strip leading ASCII whitespace and control bytes, take the text before
  the first `:` if it comes before any `/`, `?` or `#`; if there is one and it is not `http`, `https` or
  `mailto` (ASCII case-insensitive), the value becomes `#`; then escaped as an attribute.
Non-ASCII text is copied as it is (vo strings are valid UTF-8). The rules are the ones of the OWASP
cheat sheet for these three places, which is why the other places are refused rather than escaped.

## R5. `Html` is opaque outside `std.html`

**Finding**: records are mutable and shared, and their fields are public: `new Html { text: s }`,
`set page.text = s` or `from_json` would each make unchecked markup.

**Decision**: in `check_expr.c`, outside `std/html.vo`, `new Html`, reading or setting a field of a
value of type `Html` is E0340 ("`Html` is made only by templates: use html(...) or html_text(h)").
`json_why` in `check_generic.c` answers for `Html` "markup cannot be read from or written as JSON", so
`T: json` with an `Html` inside is E0328. Workers, tasks and `==` treat it as any record (it stays safe:
the only way in is a template).

## R6. Forms in std.web

**Finding**: `bind` builds the request form `{path, query, headers, body}` and `__from_json(form, 6)`
binds the input record; mode bit 1 already converts text to the field's scalar type for query values.

**Decision**: when the body is `application/x-www-form-urlencoded`, `bind` decodes it with the code of
`query_values` (refactored to take the text) into a `form` part instead of `body`; `json_derive.c`'s
`web_record` looks a field up in path, then headers, then form, then query, with the same conversion.
A name in both form and query is E-shaped 400 "sent twice". For form fields only, a missing `bool`
field is `false` and the texts `on` and `true` are `true`. A record with a `body` field and a form body
is a 415, as is a content type that is neither JSON nor form (multipart included).

`html_reply(status, page)` in `std.web`: the headers of FR-007; the policy text is
`default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; frame-ancestors 'none'`
(inline styles in a template's `<style>` keep working; inline scripts do not, which matches the
template rule that refuses holes in scripts).

## R7. Tests

- **Golden cases** `tests/cases/html/`: each acceptance scenario of US1, US2 and US5; each diagnostic
  with its JSON; the fix of an unquoted attribute hole applied by `tests/apply_fixes.py`.
- **`tests/cases/web/`**: `html_reply` headers; form binding twins of the JSON binding cases (SC-007);
  multipart 415.
- **`tests/html_safety.py`** (SC-002): 1,000+ hostile strings (generated from parts: quotes, `<`,
  `>`, `&`, entity forms, `javascript:` and `data:` with case, whitespace, tabs, NUL, `&#x6a;` forms,
  ` `, long runs) rendered into every place of a fixed set of templates by one vo program; each
  output parsed by `html.parser` and, when available, `html5lib`; the element and attribute structure
  must equal the template's, every URL attribute's scheme is one of the three or none.
- **`tests/html_speed.py`** (SC-005).
- **Example** `examples/notes-pages/` with its tests, run by `make test` as every example is.

## R8. The reference at no growth

The reference is at about 14,999 of 15,000 tokens (`tests/ref_examples.py`). Needed, about 150 tokens:
the `std.html` row (~45), the template rules in the Types/Formatting area (~70), forms in the `std.web`
bullet (~25), `html_reply` (~10).

**Decision**: one task shortens other text by at least that much, measured, before the new text goes
in. Candidates, largest sections first (Standard library and packages ~1,590, Builtins ~1,460,
Declarations ~1,230, Tests ~1,170): rows that list functions `vo doc` already prints (the reference
says so itself: "its items are not listed here"), e.g. the long `std.http`, `std.fs`, `std.process`,
`std.sqlite`, `std.tcp` rows cut to what the module is for and its effects; repeated explanations of
"the caller declares the effects" in bullets. The change states the before and after counts.

## R9. Documentation

`docs/errors.md` (E0335–E0340), `vo doc std.html` with checked examples, a site guide page
`site/docs/guide/pages.md` (templates, places, composition, forms, fragments) linked from `apis.md`,
the review's Templates row, roadmap, changelog.
