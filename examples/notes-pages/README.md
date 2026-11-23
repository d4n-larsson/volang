# Notes pages

The notes as web pages: a list with a form to add a note and a button to mark one done, built from
`std.html` templates and served with `std.web`, the notes kept in the SQLite database `data/notes.db`.
Each person signs in with a form (`std.security`'s browser way) and sees only their own notes. It works
in a browser without any script; with the page's small script, marking a note done replaces only that
note.

## Run it

```sh
cd examples/notes-pages
../../bin/vo run main.vo -- add-user ada   # type a password (8 characters or more) and Enter
../../bin/vo run main.vo                   # serve on http://localhost:8080/notes (Ctrl-C stops it)
../../bin/vo test main.vo                  # seven tests: every page and form through the app, in memory
```

Open `http://localhost:8080/notes` in Chrome or Firefox: you are sent to the sign-in page, and back to
the notes after signing in. Add a note whose text is `<script>alert(1)</script>`: it shows as text.
Safari does not keep `Secure` cookies from `http://localhost`; to try it there, set `origin` to
`"http://localhost:8080"` in `notes_browser` (`review` then reports the cookie over plain HTTP).
A `data/notes.db` from before users were added has no `owner` column: delete it.

## How it works

### Templates

A page is HTML with `{}` holes. `vo check` reads every template: its tags must nest and close, and a
hole may be text, a quoted attribute value or a URL. Each value is escaped for the place it goes, so a
note's text can never become markup. The layout is a constant with the whole document (its `<style>`
keeps its own braces), and `page` fills it.

<!-- code: views.vo fn page -->

One note is a template of its own. A page is built by passing `Html` values into holes: the list
passes a `list[Html]` of items, the layout takes the list. There are no loops or conditions inside a
template: `item` chooses between two templates with an `if`.

<!-- code: views.vo fn item -->

### Forms

The add form posts `text=...` as `application/x-www-form-urlencoded`. `std.web` binds the form's fields
into the operation's input record as it binds query parameters, with the record's limits
(`@size(1, 200)`): an empty text is 422 before the operation runs. A successful post answers 303 back to
the list.

<!-- code: pages.vo fn add -->

### Swapping one note

The done button sits in a form, so it works without a script. The page loads `/app.js`, served by the
app itself: the page's content security policy (set by `html_reply`) allows scripts only from the site.
The script posts with the header `X-Swap: 1`, and `done` then answers only the note's new markup, the
same `item` the list uses, which the script puts in place of the old one.

<!-- code: pages.vo fn done -->

### Signing in

`notes_browser` tells `std.security` how this app keeps accounts and sessions (the functions of
`users.vo`, over the same database), which page asks for the password, and how long a session lasts.
`browser(...)` is the way of signing in listed in the policy, and `browser_routes(...)` adds the sign-in
page, the sign-in form and sign-out to the routes. The policy opens those and the script to anyone and
everything else to signed-in users.

<!-- code: pages.vo fn notes_policy -->

The library does the rest: the session cookie is `HttpOnly`, `Secure` and `SameSite=Lax`; a new session
value is made at every sign-in and only its hash is stored; a page that needs a user sends the browser
to the sign-in page and back; sign-out ends the session in the database; and a form another site makes
the browser post is refused with 403, with nothing written into the forms. Each operation gets the
user from `require_user(input.identity)` and passes the name to every query, so one person never sees
another's notes.

### Effects

`main` declares `serve("8080")`, `Storage` (`file.read("data/")`, `file.write("data/")`, which only
`open_pages` needs), `clock` (session expiry), `entropy` (session values and password hashes) and
`input` (the password for `add-user`); the templates and operations need none of their own.

## Record and replay

`recordings/demo.jsonl` is a session: the redirect to sign in, signing in, two notes added through the
form, the list, one note marked done through the script, an empty note refused, a post from another
site refused, an API request answered 401, and signing out. `../../bin/vo replay recordings/demo.jsonl`
answers the same requests again without a port or `data/`.

A recording keeps every request as it arrived, so this one holds the password typed at sign-in (a
throwaway one) and the session cookie. Keep recordings of a real application as secret as its
database.
