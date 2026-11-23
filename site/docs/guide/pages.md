# Web pages

`std.web` answers JSON for programs; `std.html` answers HTML for people. A page is a template: HTML with
`{}` holes, checked by `vo check` and filled with values that are escaped for the place they go, so a
value can never become markup. Pages are built from smaller templates, HTML forms are read into the
same input records as JSON, a route can answer a piece of a page for the page to swap in, and people
sign in with a form and a session cookie.

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

## Signing in

People sign in through a form and stay signed in with a cookie. In `std.security` that is one more way
of signing in beside tokens and Basic: `browser(b)`, listed in the policy, with the same rules and the
same `require_user` in operations. The application says in one `Browser` record how it keeps accounts
and sessions (functions over its context, usually its database), which page asks for the password, the
two paths and how long a session lasts; `browser_routes(b, p)` adds the sign-in page, the sign-in form
and sign-out to its routes.

```text
fn notes_browser[D: effects]() -> Browser[Pages[D], uses D, clock, entropy] {
    return new Browser {
        find_account: fn(app: Pages[D], name: str) -> result[Option[Account]] { return account(app.db, name); },
        keep: fn(app: Pages[D], s: Session) -> result[void] { return keep_session(app.db, s); },
        find: fn(app: Pages[D], key: str) -> result[Option[Session]] { return find_session(app.db, key); },
        end: fn(app: Pages[D], key: str) -> result[void] { return end_session(app.db, key); },
        random: random_bytes,
        page: sign_in_page,                  # fn(message: str, next: str) -> Html
        sign_in: "/sign-in",
        sign_out: "/sign-out",
        lifetime_s: 8 * 3600,
        origin: ""
    };
}

let p = policy([rule("/sign-in", Access.anyone), rule("/sign-out", Access.anyone), rule("/", Access.signed_in)],
               [browser(notes_browser())], clock_ms);
let routes = browser_routes(notes_browser(), p);    # then the application's own routes; steps: [secure(p)]
```

The sign-in page is the application's template: a form posting `name`, `password` and the hidden
`next` it is given to the sign-in path. A sign-out button is a form posting to the sign-out path. The
[notes-pages](../examples/notes-pages.md) example keeps accounts and sessions in its SQLite database,
in two tables and five short functions.

What the library does, so the application does not:

- **The cookie** is `__Host-session`, `HttpOnly` (scripts cannot read it), `Secure` (HTTPS only, and
  `http://localhost` in Chrome and Firefox) and `SameSite=Lax`. Its value is 16 random bytes made at
  each sign-in; storage gets only its SHA-256, so a copy of the database signs nobody in.
- **Pages and APIs**: a `GET` for a page (one that accepts HTML) that needs a user is sent to the
  sign-in page with the path to return to; the same request from a program still gets 401. The return
  path is followed only when it is a path of the application.
- **Other sites' forms**: a request signed in by the cookie that changes something (any method but
  `GET`, `HEAD`, `OPTIONS`), and every post of the sign-in form, must come from the application's own
  pages (the browser's `Sec-Fetch-Site`, else `Origin`, compared with the host the request names), or
  it is refused with 403 before the operation runs. Nothing goes into forms for this. Behind a proxy,
  set `origin` to the address people use, such as `"https://notes.example"`.
- **Ending sessions**: sign-out ends the session in storage, so a copied cookie stops working at once;
  a session also ends when its lifetime is over, and the account is looked up on every request, so
  removing it signs its sessions out.
- **Events**: signing in, refusals (with reasons such as `wrong password`, `expired`, `cross-site`) and
  signing out go to the policy's `on_event`, never with a password or a cookie value. Nothing limits
  repeated guessing; count the `refused` events by name to slow or lock an account.

`review` names the browser way with its settings, and reports a sign-in page that the rules close and
an `origin` starting with `http://` (Safari keeps `Secure` cookies only over HTTPS, so developing there
needs `origin: "http://localhost:8080"`). A recording of a signed-in application holds the passwords
typed and the cookies: keep it as secret as the database.

Cookies of the application's own go through `std.web`: `read_cookie(header, name)` on a field
`@header("Cookie") @default("") cookies: str;`, and `with_cookie(answer, cookie(name, value))`, which
sets the same safe flags; change one with `set c.http_only = false;` where the cookie is made.

## Testing pages

Pages are values, so tests check them without a browser: call the app with `handle_request` and look
at the status and the body. To test signed-in pages, sign in through the form with `handle_request`,
take the cookie from the answer's `Set-Cookie` with `read_cookie`, and send it (with
`Sec-Fetch-Site: same-origin` for posts) in the next requests, as the example's tests do.

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
