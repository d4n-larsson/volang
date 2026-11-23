# Web pages

`std.web` answers JSON for programs; `std.html` answers HTML for people. A page is a template: HTML with
`{}` holes, checked by `vo check` and filled with values that are escaped for the place they go, so a
value can never become markup. Pages are built from smaller templates, HTML forms are read into the
same input records as JSON, and a route can answer a piece of a page for the page to swap in.

## Templates

`html(template, values...)` returns an `Html` value. The template is a string literal or a constant;
each `{}` takes the next value, as in `format`, and `{{` and `}}` are braces.

```vo
import std.html.html;
import std.html.html_text;

fn main() uses console {
    let name = "Ada & <friends>";
    print(html_text(html(`<p class="greeting">Hello, {}! You have {} notes.</p>`, name, 3)));
}
```

```output
<p class="greeting">Hello, Ada &amp; &lt;friends&gt;! You have 3 notes.</p>
```

`html_text(h)` is the markup as text, for a response body or a test.

## Where a value may go

A hole has a place, decided by `vo check` from the template:

| Place | Example | The value is |
|---|---|---|
| text | `<li>{}</li>` | escaped as text |
| a quoted attribute value | `<a title="{}">` | escaped so it cannot end the attribute |
| a whole URL attribute | `<a href="{}">` | a `str`; replaced by `#` unless it is relative or `http`, `https`, `mailto` |
| after the start of a URL | `<a href="/notes/{}">` | escaped as attribute text: the template fixed the scheme |

Every other place is [E0338](../errors/E0338.md): inside `<script>` or `<style>`, in an `on...` or
`style` attribute, in a tag or attribute name, in a comment, or in an unquoted attribute value. No
escaping makes a value safe there. To hand data to a script, put it in a quoted `data-...` attribute.

```vo
import std.html.html;
import std.html.html_text;

fn main() uses console {
    let id = "n7";
    print(html_text(html("<button onclick=\"open('{}')\">open</button>", id)));
}
```

```error E0338
```

`vo check` also reads the template as HTML: every element it opens must be closed, in order
([E0337](../errors/E0337.md)), so a template is always a balanced piece that can go into another
template's hole.

## Building a page

An `Html` value goes into a text hole as it is, and so does a `list[Html]`, one after another. That is
how pages are built: there are no loops or conditions inside templates, only vo code.

```vo
import std.html.Html;
import std.html.html;
import std.html.html_text;

fn item(text: str, done: bool) -> Html {
    if done {
        return html(`<li class="done">{}</li>`, text);
    }
    return html("<li>{}</li>", text);
}

fn page(title: str, content: Html) -> Html {
    return html("<!DOCTYPE html><html><head><title>{}</title></head><body><h1>{}</h1>{}</body></html>", title, title, content);
}

fn main() uses console {
    let items = [item("water the plants", true), item("buy <bread>", false)];
    print(html_text(page("Notes", html("<ul>{}</ul>", items))));
}
```

```output
<!DOCTYPE html><html><head><title>Notes</title></head><body><h1>Notes</h1><ul><li class="done">water the plants</li><li>buy &lt;bread&gt;</li></ul></body></html>
```

An `Html` value is made only by templates (`new Html` is [E0340](../errors/E0340.md)), which is why it
is safe to insert as it is. `no_html()` is the empty one, for a hole that shows nothing.

## Answering a page

In a [`std.web`](apis.md) operation, `html_reply(status, page)` answers the page with
`Content-Type: text/html; charset=utf-8`, `nosniff`, and a content security policy that allows scripts
only from the site itself (inline styles keep working) and forbids framing.

```text
fn list_page[D: effects](app: Pages[D], input: NoInput) -> result[Response] uses D {
    return ok(html_reply(200, page("Notes", list_body(try all_notes(app.db)))));
}
```

## Forms

A browser posts a form as `application/x-www-form-urlencoded`. `std.web` reads its fields into the
operation's input record by name, as it reads query parameters: text becomes the field's type, the
field's limits (`@size`, `@range`) apply, and an unknown or repeated field is a 400. A checkbox sends
`on` when checked and nothing when not, so a `bool` field from a form is `true` or `false`.

```text
record NewNote {
    @size(1, 200)
    text: str;
    urgent: bool;
}
```

An operation that changes something usually answers `303` with `Location` back to a page, so a reload
does not post the form again. Form errors are answered like JSON input errors (400 or 422 with the
fields); to show them on the page instead, take `str` fields and check them in the operation.

## Swapping a piece of a page

A route can answer part of a page, for example one updated list item, and a small script on the page
can put it in place of the old one. Use the same template for the item in the page and alone, so both
are the same markup. The script must be served by the app (a route answering its text with
`Content-Type: text/javascript`), since the page's policy allows no scripts from other sites and none
inline. The [notes-pages](../examples/notes-pages.md) example does this in a few lines, and keeps a
plain form under the button so the page works without the script.

## Testing pages

Pages are values, so tests check them without a browser: call the app with `handle_request` and look
at the status and the body.

```vo-test
import std.html.html;
import std.html.html_text;
import std.text.contains;

fn main() {
}

test "a note's text is shown as text" {
    let page = html_text(html("<ul><li>{}</li></ul>", "<script>alert(1)</script>"));
    expect(contains(page, "&lt;script&gt;"));
    expect(not contains(page, "<script>"));
}
```

```output
PASS a note's text is shown as text (0ms)
1 passed, 0 failed
```
