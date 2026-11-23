# Feature Specification: HTML Pages (`std.html`)

**Feature Branch**: `051-html-templates`

**Created**: 2026-11-23

**Status**: Implemented ([results](results.md))

**Input**: User description: "Specify the HTML templates feature." From the review
(docs/review-2026-11/ecosystem.md, Standard library, "Templates": "`std.web` returns JSON; an HTML
template with escaping by default would make it a full web stack"), and the discussion of 2026-11-23:
rather than running vo in the browser, server-rendered pages from `std.web`, with small HTML fragments
that a page swaps in, cover most of what agents build (internal tools, admin pages, CRUD apps) with no
browser runtime and no event model.

Background: a vo service can answer JSON and nothing else. An agent asked for "a small web app to keep
notes" builds HTML by joining strings with `+`, which is the most common way to write a cross-site
scripting hole: a note whose text is `<script>...</script>` runs in every visitor's browser. It also
cannot read what an HTML form sends: browsers post forms as `application/x-www-form-urlencoded`, and
`std.web` reads JSON bodies only.

From an agent's point of view:
- **Escaping must not be a step that can be forgotten.** If inserting a value into HTML always escapes
  it for the place it goes, the hole cannot be written by accident.
- **Agents know HTML.** A template that is HTML with holes is what they would write anyway; a new
  template language (loops, conditions, filters inside the template) is a second language to learn and
  a second way to do what vo code already does.
- **Mistakes should be found before the program runs**, as `format`, `regex` and `sql` literals are
  checked today: an unclosed tag, a hole where no value can be safe, a wrong number of values.

What this spec must keep:
- **Principle I**: one way to build a page; control flow stays in vo code.
- **Principle III**: rendering is pure and deterministic; byte-identical on the three architectures.
- **Principle V**: the reference is at about 14,999 of 15,000 tokens. The new module's row and rules
  are paid for by shortening other parts of the reference; the total must not exceed 15,000.
- **Security**: no value a program inserts can add an element, an attribute, a script or a URL scheme
  that the template did not contain.

## User Scenarios & Testing *(mandatory)*

The users are AI coding agents that write vo services from the reference and `vo doc`, the people who
review them, and the people who use the pages in a browser.

### User Story 1 - A page from a template, escaped (Priority: P1)

An agent writes the notes list page: a template with the page's HTML and holes for the title and the
list, a smaller template for one note, and an operation that answers the page. A note's text with
`<`, `&`, quotes or a `<script>` tag shows as text in the browser.

**Why this priority**: it is the feature, and the security property is the reason for it.

**Independent Test**: golden cases that render templates with every kind of value in every allowed
place, byte for byte; a corpus of hostile strings rendered into every allowed place and parsed by an
HTML5 parser, which must find exactly the elements and attributes of the template.

**Acceptance Scenarios**:

1. **Given** `html("<li>{}</li>", text)` with `text` = `<script>alert(1)</script> & "x"`, **When**
   rendered, **Then** the result is `<li>&lt;script&gt;alert(1)&lt;/script&gt; &amp; &quot;x&quot;</li>`.
2. **Given** a hole in a quoted attribute value (`<a title="{}">`), **When** a value with `"`, `'`,
   `<`, `>` or `&` is inserted, **Then** it is escaped so the attribute cannot end early.
3. **Given** a hole that is a whole URL attribute value (`href`, `src`, `action`, `formaction`), **When**
   the value starts with `javascript:`, `data:`, `vbscript:` or any scheme other than `http`, `https`
   and `mailto`, **Then** the attribute's value is `#` instead; relative URLs and the three schemes
   are kept, escaped. **Given** a hole after a start of the URL written in the template that fixes its
   scheme (`/notes/{}`, `?q={}`, `mailto:{}`), **Then** the value is escaped as attribute text
   (decided while implementing: refusing it made the most common link an error).
4. **Given** a value that is itself the result of `html(...)`, **When** inserted into a text hole,
   **Then** it is inserted as it is (it is already safe), so pages are built from smaller templates.
5. **Given** a list of such values, **When** inserted into a text hole, **Then** they are inserted one
   after another; an empty list inserts nothing.
6. **Given** `int`, `float` and `bool` values, **When** inserted, **Then** they appear as `str(x)`
   gives them.

---

### User Story 2 - Template mistakes are caught before the program runs (Priority: P1)

An agent writes a template with an unclosed tag, a hole inside a `<script>`, a hole in an `onclick`
attribute, an unquoted attribute hole, or passes three values to a template with two holes. `vo check`
reports each at its place.

**Why this priority**: an unsafe place for a value is a design mistake no escaping can fix; the agent
must learn it while writing, with the reason.

**Independent Test**: golden check cases, one per kind of mistake, with the expected JSON diagnostics.

**Acceptance Scenarios**:

1. **Given** `html(t, ...)` where `t` is not a string literal or a constant, **When** checked, **Then**
   a diagnostic says templates are literals and that pages are composed by passing `html(...)` values
   into holes.
2. **Given** a template whose tags do not nest (`<p><b></p>`), or that leaves an element open that
   must be closed, **When** checked, **Then** a diagnostic points at the tag. Void elements (`<br>`,
   `<img>`, `<input>`, ...) need no end tag, and an end tag for one is an error.
3. **Given** a hole inside `<script>` or `<style>`, in an event-handler attribute (`on...`), in a
   `style` attribute, in an unquoted attribute value, inside a tag or attribute name, or inside a
   comment, **When** checked, **Then** a diagnostic names the place and says why no value can be safe
   there.
4. **Given** a template with two holes called with one or three values, **When** checked, **Then** a
   diagnostic gives both counts. **Given** a value of a type that cannot be inserted (a record, a map),
   **Then** a type error names the allowed types.
5. **Given** an `html(...)` value or a list of them passed to an attribute hole, **When** checked,
   **Then** it is an error: markup cannot go into an attribute.
6. **Given** `new Html { ... }` written outside `std.html`, **When** checked, **Then** it is an error:
   an `Html` value comes only from templates, so it is always safe to insert.

---

### User Story 3 - Pages and forms in std.web (Priority: P1)

An agent answers an operation with a page (status, `Content-Type: text/html; charset=utf-8`), and
reads what an HTML form posts into the operation's input record, as it reads JSON today. A form that
adds a note posts `text=...`, the record `NewNote { text: str }` receives it, and its field limits
(`@size(1, 200)`) apply.

**Why this priority**: a page without forms is read-only; HTML forms are how a server-rendered app
takes input without any script.

**Independent Test**: golden and `handle_request` cases: a page answer with its headers; form bodies
with every field type, missing fields, repeated fields, percent-encoding and `+`, oversized bodies, and
the same limits as JSON.

**Acceptance Scenarios**:

1. **Given** an operation that returns `html_reply(200, page)`, **When** it answers, **Then** the body
   is the page's text and the headers say `text/html; charset=utf-8`, `nosniff`, and a content
   security policy that allows scripts only from the same site and forbids framing.
2. **Given** a `POST` with `Content-Type: application/x-www-form-urlencoded` and body
   `text=buy+milk%21`, **When** the route's input record has `text: str`, **Then** the operation
   receives `"buy milk!"`.
3. **Given** a form field for an `int`, `float` or `bool` field, **When** its text is not a value of
   that type, **Then** the request is answered 400 with the field named, as for JSON. A `bool` field is
   `true` when the field is present with value `on` or `true`, `false` when absent (an unchecked
   checkbox sends nothing).
4. **Given** a field the record does not have, or a field sent twice, **When** bound, **Then** it is a
   400 naming the field, as JSON's unknown and duplicate keys are.
5. **Given** a form body over the size limit that applies to JSON bodies, **When** received, **Then** it
   is refused the same way.
6. **Given** a `GET` form, **When** submitted, **Then** its fields arrive as the query string and bind
   the input record the way `std.web` binds query parameters today.

---

### User Story 4 - Fragments that a page swaps in (Priority: P2)

An agent adds "mark done" to each note: a button posts to `/notes/3/done`, and the route answers only
the updated `<li>`, which the page puts in place of the old one with a small client-side library chosen
by the app. The same template renders the item inside the full page and alone.

**Why this priority**: partial updates make server-rendered apps feel immediate without a frontend
framework; in vo it needs nothing more than answering a fragment, which this story makes explicit and
tested.

**Independent Test**: a route answering a fragment; the full page and the fragment contain
byte-identical markup for the same note.

**Acceptance Scenarios**:

1. **Given** a template for one note used by the list page and by the `done` route, **When** both are
   rendered for the same note, **Then** the item's markup is byte-identical in both.
2. **Given** an `html(...)` value that is not a whole document, **When** answered with `html_reply`,
   **Then** the body is exactly that markup, with the same headers.

---

### User Story 5 - Tested without a browser (Priority: P2)

An agent tests its pages with `vo test`: it calls the app with `handle_request`, checks the status,
and checks that the page contains the note's text, escaped, using the rendered text.

**Why this priority**: agents fix code by running tests; page tests must not need a browser.

**Acceptance Scenarios**:

1. **Given** an `Html` value, **When** turned into text, **Then** the text is the exact markup, so a
   test can compare it or search it.
2. **Given** the same templates and values, **When** rendered on x86-64, ARM64 and RISC-V and 20 times,
   **Then** the bytes are identical.

### Edge Cases

- **Braces**: `{}` is a hole and `{{`/`}}` are literal braces, as in `format`; a template has no other
  syntax (no options after a colon: formatting a number is done with `format` before inserting it).
- **Whole documents**: `<!DOCTYPE html>` is allowed at the start; `<html>`, `<head>` and `<body>` are
  ordinary elements that must be balanced.
- **Text content**: characters that need no escaping are left as they are; non-ASCII text is inserted as
  UTF-8 (vo's `str` is always valid UTF-8).
- **Attributes without values** (`<input checked>`) are allowed in the template; a value cannot decide
  whether an attribute is present (the program chooses between two templates).
- **`<textarea>` and `<title>`**: holes there are text holes, escaped as text.
- **Entities written in the template** (`&nbsp;`, `&amp;`) are kept as written.
- **Comments in the template** are kept; a hole inside one is an error.
- **Size**: rendering a page of 1 MiB from 10,000 inserted values is linear in its size.
- **`Html` and JSON**: an `Html` value cannot be turned into JSON or read from it (it is not a `json`
  type), so a page cannot be smuggled through an API's input.
- **Form encoding**: field names and values are percent-decoded and must be UTF-8 (a 400 otherwise);
  `+` is a space; an empty value is the empty string; field order does not matter.
- **Multipart forms** (file uploads) are refused with 415, naming the encodings that are read.

## Requirements *(mandatory)*

### Functional Requirements

**Templates (`std.html`)**

- **FR-001**: The standard library MUST provide `std.html` with a type `Html` (markup known to be safe)
  and `html(template, values...) -> Html`, where `template` is a string literal or a constant whose
  `{}` holes take the values in order and `{{`/`}}` are braces.
- **FR-002**: A value inserted into a text hole MUST be escaped so it is text (`&`, `<`, `>`, `"`, `'`);
  into a quoted attribute value, so it cannot end the attribute; into a URL attribute (`href`, `src`,
  `action`, `formaction`, `poster`, `cite`), when the value is the whole URL, additionally replaced by
  `#` unless it is relative or its scheme is `http`, `https` or `mailto`; after a start of the URL in the
  template containing `/`, `?`, `#` or `:`, escaped as attribute text; anywhere else in a URL attribute
  refused (a value there could choose the scheme).
- **FR-003**: Values MAY be `str`, `int`, `float`, `bool`, `Html` and `list[Html]`; `Html` and
  `list[Html]` only in text holes, inserted as they are.
- **FR-004**: An `Html` value MUST be obtainable only from `html` (and the functions of FR-006), never
  built with `new` outside `std.html`, so every `Html` value is markup that came from a checked
  template.
- **FR-005**: `vo check` MUST report, each with a stable code, its place in the template and its
  reason: a template that is not a literal or constant; tags that do not nest or are left open; an end
  tag for a void element; a hole inside `<script>`, `<style>`, an `on...` or `style` attribute, an
  unquoted attribute value, a tag or attribute name, or a comment; a wrong number of values; markup
  passed to an attribute hole. A value of another type is the existing type error.
- **FR-006**: `std.html` MUST provide turning an `Html` value into its text, and the empty `Html` value;
  nothing that turns arbitrary text into `Html` unescaped.

**Pages and forms (`std.web`)**

- **FR-007**: `std.web` MUST provide `html_reply(status, page: Html) -> Response` with the headers
  `Content-Type: text/html; charset=utf-8`, `X-Content-Type-Options: nosniff`, and a
  `Content-Security-Policy` that allows scripts only from the same origin and forbids framing.
- **FR-008**: Routes MUST bind their input record from `application/x-www-form-urlencoded` bodies with
  the same rules, field annotations, limits and error answers as JSON bodies: text to the field's type,
  unknown and repeated fields refused, `bool` fields from presence (`on`/`true`) or absence.
- **FR-009**: Other body encodings that are not JSON or form encoding MUST be answered 415 as today's
  wrong content types are.

**Fit**

- **FR-010**: Rendering MUST be pure (no effect) and give identical bytes on the three architectures.
- **FR-011**: Each new diagnostic MUST have a stable code with a catalog entry, an example and, where
  one correction is clear, a fix; `vo doc std.html` MUST document every item with a checked example.
- **FR-012**: The reference MUST describe `std.html`, the template rules and form binding, and MUST NOT
  exceed 15,000 tokens; what is shortened to make room is stated in the change.
- **FR-013**: An example service MUST render the notes as pages: a list page, a form that adds a note, a
  route that answers one updated item as a fragment, and tests through `handle_request`.

### Key Entities

- **Html**: markup that came from a checked template, safe to insert anywhere text may go.
- **Template**: a string literal or constant of HTML with `{}` holes; each hole has a place (text,
  quoted attribute, URL attribute) decided by `vo check`.
- **Form input**: the fields of a URL-encoded body or query string, bound into the operation's input
  record.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every acceptance scenario is a golden or `handle_request` case, byte-identical on x86-64,
  ARM64 and RISC-V and over 20 runs.
- **SC-002**: A corpus of at least 1,000 hostile strings (tag and attribute injections, quote breaks,
  entity tricks, `javascript:` and other schemes in mixed case and with whitespace and control bytes,
  invalid-looking UTF-8 sequences in text) inserted into every allowed place of a set of templates,
  rendered and parsed by an HTML5 parser, yields exactly the elements and attributes of the templates,
  and no URL attribute with a scheme other than `http`, `https` and `mailto`.
- **SC-003**: Every mistake of User Story 2 is reported by `vo check` with a stable code at its place;
  every fix offered, applied, gives a template that checks.
- **SC-004**: The example's pages work in a browser: listing, adding through the form, and marking done
  through a fragment; its `vo test` passes with no network.
- **SC-005**: Rendering a 1 MiB page from 10,000 values takes under 50 ms (median of 3, native x86-64).
- **SC-006**: The reference stays at or under 15,000 tokens.
- **SC-007**: Form binding passes the same field-validation cases as JSON binding (each JSON binding
  case has a form twin where the encoding allows it).

## Assumptions

- **A module of the standard library, not a package**: templates are checked by `vo check` (FR-005)
  and `Html` values must not be built by hand (FR-004); both need the checker, as `format`, `regex` and
  `sql` do.
- **Holes are `{}` as in `format`**, so an agent knows one template syntax; values are positional.
- **No template language**: no loops, conditions, filters or includes inside templates. A list is built
  in vo and passed as `list[Html]`; a choice is an `if` between two `html(...)` values; a layout is a
  function that takes the content as `Html`.
- **Templates live in vo source** (literals and constants, raw strings for several lines); template
  files are out of scope: they would need reading at check time and at run time.
- **No escape hatch** for raw HTML from text: markup from elsewhere (rendered Markdown, a CMS) is out of
  scope until a program needs it, and then needs a sanitizer, not a cast.
- **The client-side library for fragments is the app's choice and not shipped by vo**; serving static
  files (scripts, styles, images) is out of scope: a page's style goes in a `<style>` element of a
  template, and a script would be served by a route that answers its text.
- **The content security policy** of `html_reply` is a safe default for server-rendered pages; a page
  that needs another policy is out of scope for this spec.
- **Cookies, sessions and sign-in forms** (with protection against cross-site form posts) are a
  separate, later spec; `std.security`'s ways (tokens, basic) are unchanged.
- **Form errors are answered as JSON errors are** (400/422 problem documents); an app that wants to show
  them on the page binds `str` fields and validates in its operation.
- Out of scope: running vo in the browser, client-side rendering, template files, internationalized
  formatting, multipart forms and file uploads, static file serving, cookies and sessions, streaming
  responses of HTML.
