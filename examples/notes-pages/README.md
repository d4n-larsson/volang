# Notes pages

The notes as web pages: a list with a form to add a note and a button to mark one done, built from
`std.html` templates and served with `std.web`, the notes kept in the SQLite database `data/notes.db`.
It works in a browser without any script; with the page's small script, marking a note done replaces
only that note.

## Run it

```sh
cd examples/notes-pages
../../bin/vo run main.vo        # serve on http://localhost:8080/notes (Ctrl-C stops it)
../../bin/vo test main.vo       # five tests: every page and form through the app, a database in memory
```

Open `http://localhost:8080/notes`, add a note whose text is `<script>alert(1)</script>`: it shows as
text.

## How it works

### Templates

A page is HTML with `{}` holes. `vo check` reads every template: its tags must nest and close, and a
hole may be text, a quoted attribute value or a URL. Each value is escaped for the place it goes, so a
note's text can never become markup. The layout is a constant with the whole document (its `<style>`
keeps its own braces), and `page` fills it.

<!-- code: pages.vo fn page -->

One note is a template of its own. A page is built by passing `Html` values into holes: the list
passes a `list[Html]` of items, the layout takes the list. There are no loops or conditions inside a
template: `item` chooses between two templates with an `if`.

<!-- code: pages.vo fn item -->

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

### Effects

`main` declares `serve("8080")` and `Storage` (`file.read("data/")`, `file.write("data/")`), which only
`open_pages` needs; the templates and operations need none of their own.

## Record and replay

`recordings/demo.jsonl` is a session: two notes added through the form, the list, one note marked done
through the script, an empty note refused, and the script. `../../bin/vo replay recordings/demo.jsonl`
answers the same requests again without a port or `data/`.
