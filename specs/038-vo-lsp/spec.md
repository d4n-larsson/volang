# Feature Specification: The Toolchain in the Editor (`vo lsp`)

**Feature Branch**: `038-vo-lsp`

**Created**: 2026-11-21

**Status**: Implemented (gate not run)

**Input**: User description: "`vo lsp`: a language server inside the vo binary, speaking the Language
Server Protocol over stdin/stdout, so that editors show what the toolchain already knows while a person
reads or edits vo: diagnostics as you type with their stable codes, the toolchain's fixes as quick fixes,
hover with type, signature and doc comment, go to definition, find references, rename across the program,
format document, and document symbols. It must work on the text in the editor, not only on saved files.
Source: docs/review-2026-11/ecosystem.md, Tooling, 'Language server' (roadmap step 3). Like `vo mcp`, each
request runs the ordinary toolchain code isolated from the server, and the server holds no state except
the open documents. Must keep: the same diagnostics, codes, messages and fixes as `vo check --json` on
the same text, determinism, no libc, three architectures, a server that never runs the user's program,
the reference under 15,000 tokens (about 14,803 now), no large static buffers. Out of scope: completion
and signature help, semantic tokens, inlay hints, code lens, workspace symbol search, debugging,
multi-root workspaces, publishing an extension."

Background: an agent writes vo with `vo check --json` in a loop, or through `vo mcp`. The person who
reads, reviews or corrects that code works in an editor, and there vo is, since spec 037, coloured but
mute: a mistake shows only when a command is run in a terminal, a name's type has to be looked up, and a
rename is a search and replace. Everything an editor needs is already computed by the toolchain
(diagnostics with fixes, types and documentation at a position, references, a safe rename, canonical
formatting). Editors ask for exactly these things through one standard protocol. A server that answers
it puts the whole toolchain under the cursor in every editor at once.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Mistakes shown as you type, with their fixes (Priority: P1)

A developer opens a vo file. Problems are underlined where they are, with the toolchain's message and
stable code, and they update as the text changes, before saving. On a problem that has a fix, the editor
offers it as a quick fix, and applying it makes the same change the toolchain's fix describes.

**Why this priority**: it is what a language server is for; everything else is navigation.

**Independent Test**: a scripted editor opens a file with known mistakes, and receives diagnostics equal
(position, code, message, severity) to `vo check --json` on that text; it changes the text without saving
and receives the diagnostics of the new text; it asks for the quick fixes at a diagnostic and applying
the edit gives the text the toolchain's fix gives.

**Acceptance Scenarios**:

1. **Given** a file opened in the editor, **When** the server has checked it, **Then** the editor has one
   diagnostic for each error and warning `vo check --json` reports for that text, with the same range,
   severity, code and message, and a link to the code's catalog entry.
2. **Given** an edit that is not saved, **When** the editor sends the new text, **Then** the diagnostics
   are those of the new text; the file on disk is not read for it and never written.
3. **Given** a file that imports other files of its program, **When** checked, **Then** the imports
   resolve as on the command line, using the editor's text for files that are open and the disk for the
   rest; an edit in one open file updates the diagnostics of the other open files that depend on it.
4. **Given** a diagnostic that has a fix, **When** the editor asks for actions there, **Then** it gets a
   quick fix titled with the fix's description whose edits are the toolchain's; a diagnostic with several
   fixes offers each; one without offers none.
5. **Given** a file with no problems, or a file that is closed, **When** it is checked or closed,
   **Then** its diagnostics are cleared.
6. **Given** a file that cannot be lexed or parsed at all, **When** checked, **Then** the syntax
   diagnostics are shown and the server keeps working; when the text is fixed the other diagnostics come
   back.
7. **Given** fast typing, **When** several changes arrive before a check ends, **Then** only the newest
   text's diagnostics are published, and nothing older arrives after them.

---

### User Story 2 - Know what a name is, and go to it (Priority: P1)

Hovering over a name shows its type, the declared signature and its doc comment. "Go to definition"
jumps to the declaration, in this file or another; "find references" lists every use in the program.

**Why this priority**: reading code that someone else (an agent) wrote is the main activity; these are
the questions a reader asks.

**Independent Test**: for a set of positions in a multi-file program, hover, definition and references
answers equal what `vo query type-at` and `vo query refs` give for the same position.

**Acceptance Scenarios**:

1. **Given** a position on a name or expression, **When** hover is asked, **Then** the answer shows the
   type, the declaration's signature where there is one, and the doc comment, as `vo query type-at`
   gives them; on white space or a comment the answer is empty.
2. **Given** a name that has a declaration in the program or a package, **When** definition is asked,
   **Then** the answer is the declaration's file and range; for a builtin or a standard-library item the
   answer is empty (there is no file to open) and hover still documents it.
3. **Given** a name, **When** references are asked, **Then** the answer lists every reference in the
   program that `vo query refs` lists, in open and unopened files, with the declaration included or not
   as the editor asks.
4. **Given** unsaved changes, **When** any of these is asked, **Then** positions are those of the
   editor's text.
5. **Given** a file whose program has errors, **When** asked, **Then** the answers are given wherever
   the toolchain can give them, and are empty elsewhere; never an error dialog for an ordinary mistake.

---

### User Story 3 - Change code safely: rename and format (Priority: P2)

"Rename symbol" on a function, record, field, variant case, parameter or local renames it everywhere in
the program, or is refused with the toolchain's reason. "Format document" rewrites the buffer in
canonical form.

**Why this priority**: the two changes people make by hand that the toolchain makes correctly.

**Independent Test**: a rename request returns edits that, applied, give the files `vo edit rename`
writes; a rename the toolchain refuses returns its message as the error; formatting returns the text
`vo fmt` produces.

**Acceptance Scenarios**:

1. **Given** a name that can be renamed, **When** rename is asked with a valid new name, **Then** the
   answer is one set of edits across every file of the program that `vo edit rename` would change, and
   the server itself writes nothing: the editor applies them.
2. **Given** a rename the toolchain refuses (a name in use, a wrong spelling for the kind of name, a
   builtin, a package's item), **When** asked, **Then** the editor shows the toolchain's message.
3. **Given** a position, **When** the editor asks whether a rename can start there, **Then** the answer is
   the name's range, or a refusal where nothing can be renamed.
4. **Given** a buffer, **When** formatting is asked, **Then** the answer changes it into what `vo fmt`
   gives for that text; an already canonical buffer gets no edits; a buffer with syntax errors gets none
   and no error dialog.
5. **Given** a rename across files with unsaved changes in some, **When** asked, **Then** the edits fit
   the editor's texts.

---

### User Story 4 - The outline of a file (Priority: P3)

The editor's outline and breadcrumb show the file's functions, records with their fields, variants with
their cases, constants, effect sets, tests and properties.

**Why this priority**: cheap, and it makes large files navigable.

**Independent Test**: the symbols of a file equal its declarations, in order, with kinds, names and
ranges.

**Acceptance Scenarios**:

1. **Given** a file, **When** its symbols are asked, **Then** each top-level item is listed with its
   kind, its name (a test's or property's text) and its range, records and variants with their members
   as children.
2. **Given** a file with syntax errors, **When** asked, **Then** the items that could be read are listed.

---

### User Story 5 - A session that survives everything (Priority: P1)

The server runs for hours while code is half-written. Whatever a buffer holds, the server answers or
answers empty, and keeps going: a check that would take too long is cut off, a request for a file that
does not exist or a position outside the text gets an empty answer, a malformed message gets the
protocol's error.

**Why this priority**: an editor restarts a server that dies a few times and then gives up; a server
that hangs is worse.

**Independent Test**: in one session: text that makes the checker work hard, a deeply nested expression,
binary garbage as a document, positions beyond the end, requests for unopened and missing files,
malformed messages, a cancelled request; each gets its documented answer and a following ordinary
request succeeds; memory after a long session of edits is no higher than at its start.

**Acceptance Scenarios**:

1. **Given** any document text, **When** processed, **Then** the server stays up; an internal failure of
   one request yields an empty answer or a protocol error for that request only.
2. **Given** a request that takes longer than a documented limit, **When** the limit passes, **Then** it
   is ended and answered as failed; later requests are unaffected.
3. **Given** the editor cancels a request, **When** the cancellation arrives, **Then** the work stops and
   the request is answered as cancelled.
4. **Given** the protocol's opening and closing exchanges, **When** performed, **Then** the server
   announces exactly the features it implements, and on shutdown and exit ends with the status the
   protocol prescribes; if the editor disappears, the server ends by itself.
5. **Given** a session of thousands of edits and requests, **When** it ends, **Then** the server's memory
   has not grown with the number of requests.
6. **Given** any request, **When** served, **Then** the user's program is never run and no file is
   written.

---

### User Story 6 - It starts by itself, and is documented (Priority: P2)

With the editor folders of spec 037 installed, opening a vo file starts `vo lsp`; the site's editor page
gives the lines for Neovim and Helix. The reference lists the command.

**Why this priority**: a server nobody starts does nothing.

**Independent Test**: the editor configurations name the command and the file type; the reference has
the command and stays under 15,000 tokens; the site's checks pass.

**Acceptance Scenarios**:

1. **Given** the VS Code and Zed folders and the site's snippets, **When** read, **Then** each starts
   `vo lsp` for vo files and says that `vo` must be on the PATH.
2. **Given** the reference, **When** read, **Then** `vo lsp` is in the command list with one sentence.

---

### Edge Cases

- Which program a file belongs to: documented and predictable (see Assumptions); a file outside any
  program is its own program.
- A file open in the editor that does not exist on disk yet (a new, unsaved file): checked from its text.
- A file changed on disk by another tool (an agent, `git checkout`) while not open: the next request
  sees the new content; while open, the editor's text wins.
- A file that is open and deleted on disk: still checked from the editor's text.
- Positions: the protocol counts characters its own way (not bytes); non-ASCII text in strings and
  comments must not shift any range.
- Line endings: Windows-style line endings in a buffer do not shift ranges.
- Very large files (tens of thousands of lines) and programs of hundreds of files: diagnostics still
  arrive in interactive time or the check is cut off with a message, never a frozen editor.
- Diagnostics that belong to another file than the one being edited (a mistake in an imported file):
  published for that file.
- Warnings (unused local, hole): shown as warnings; holes carry their expected type in the message as
  the toolchain prints it.
- A quick fix asked for a diagnostic that is no longer there (the text moved on): no actions.
- The effects record (`vo.effects`) disagreeing with the program: reported as on the command line; the
  server never rewrites the record.
- Two editors on the same directory: each has its own server; they share nothing.
- `vo lsp` with any argument: a usage error (T0001). A built executable is never a language server.
- Requests for features not offered (completion): the protocol's "method not found", which editors
  treat as "not supported".

## Requirements *(mandatory)*

### Functional Requirements

**The server**

- **FR-001**: `vo lsp` MUST serve the Language Server Protocol on standard input and output until the
  protocol's exit, announcing in the opening exchange exactly the features below.
- **FR-002**: The server MUST keep the text of every open document as the editor sends it, and use that
  text, not the disk, for those files in every answer; unopened files are read from disk at each request.
- **FR-003**: The server MUST never run the user's program, write a file, or change `vo.effects`.

**Diagnostics and fixes**

- **FR-004**: After a document is opened or changed, the server MUST publish its diagnostics: one per
  error and warning that `vo check --json` gives for the same texts, with the same range, severity, code
  and message, and the code's catalog address. Diagnostics of other open files affected by the change
  MUST be updated too.
- **FR-005**: Only the newest text's diagnostics MUST be published; closing a document clears its
  diagnostics.
- **FR-006**: For a range, the server MUST offer one quick fix per toolchain fix of the diagnostics
  there, with the fix's description as title and the fix's edits.

**Reading**

- **FR-007**: Hover MUST show the type, declared signature and documentation that `vo query type-at`
  gives for the position.
- **FR-008**: Go to definition MUST answer the declaration's location for names declared in the program
  or its packages, and nothing for builtins and the standard library.
- **FR-009**: Find references MUST answer the locations `vo query refs` gives.
- **FR-010**: Document symbols MUST list a file's items with kind, name, range and members.

**Changing**

- **FR-011**: Rename MUST answer the edits of `vo edit rename` for the whole program, or the toolchain's
  refusal as the request's error; a "can this be renamed" request MUST answer the name's range or refuse.
- **FR-012**: Format document MUST answer edits that turn the buffer into `vo fmt`'s output, and none for
  canonical or unparsable text.

**Robustness**

- **FR-013**: No document text, position, file name or message may end the server; a failure inside one
  request affects only that request.
- **FR-014**: Each request MUST have a documented time limit and memory limit; cancellation from the
  editor MUST stop the work.
- **FR-015**: The server's memory MUST not grow with the number of requests; it MUST hold no state but
  the open documents, each one's program, and which files it has published diagnostics for (so that it
  can clear them).
- **FR-016**: The server MUST end when the editor that started it is gone.
- **FR-017**: Positions MUST be converted exactly between the protocol's character counting and the
  toolchain's, for any text.

**Agreement, documentation, tests**

- **FR-018**: Every answer MUST come from the same checker, query and edit code as the commands, so that
  an answer never disagrees with the command line on the same texts.
- **FR-019**: The editor folders of spec 037 and the site's editor page MUST start the server; the
  reference MUST list `vo lsp` and stay under 15,000 tokens.
- **FR-020**: Mistakes on the command line reuse an existing code; protocol mistakes use the protocol's
  own errors. The server MUST behave identically on the three architectures.

### Key Entities

- **Open document**: an address, a version and the text the editor holds; the only state.
- **Program of a file**: the entry file from which the file is reached, by the documented rule.
- **Diagnostic**: range, severity, stable code, message, and the fixes the toolchain attaches.
- **Edit set**: text changes in one or more files, applied by the editor.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On a set of files covering every diagnostic the test suite can produce, the server's
  diagnostics equal `vo check --json`'s (range, severity, code, message) with no differences, and every
  quick fix applied gives the text the toolchain's fix gives.
- **SC-002**: Hover, definition, references, rename and formatting answers equal the corresponding
  command's on a multi-file program, including with unsaved changes, with no differences.
- **SC-003**: After a change in a 2,000-line file of a 20-file program, diagnostics arrive within
  200 ms; hover within 100 ms.
- **SC-004**: A session of 5,000 edits and requests, including every robustness case, ends with every
  request answered and the server's memory no higher than after the first 50.
- **SC-005**: An editor configured as documented shows diagnostics, hover and rename on a vo file;
  verified by hand in one editor where one is available, and recorded as not verified otherwise.
- **SC-006**: The reference stays under 15,000 tokens; every existing test case is unchanged.

## Assumptions

- **The program of a file**: the nearest `main.vo` in the file's directory or a directory above it, up to
  the workspace folder the editor opened, if that program imports the file; otherwise the file itself is
  the entry (as `vo check FILE` treats it). Diagnostics always come from checking the file's program, so a
  library file shows the mistakes its program's check reports for it.
- Whole documents are sent on each change (the protocol's simplest mode); vo files are small and the
  check is fast, so partial updates are not needed.
- A check is started after a short pause in typing, not on every keystroke; the newest text wins.
- Limits per request: 5 seconds and the command line's default memory limit; a request over a limit is
  answered as failed, and for diagnostics the editor is told once in a message.
- One workspace folder; files outside it are served as single-file programs.
- The standard library has no files on disk to jump to; "go to definition" on its items is empty and
  hover carries the documentation.
- Completion, signature help, semantic colouring, inlay hints, test lenses and workspace-wide symbol
  search are not offered in this version; completion needs the checker to answer "what can stand here",
  which is a spec of its own.
- The VS Code folder of spec 037 has no code today; starting a server there needs a few lines of
  extension code and its client library, added in this spec and still not published to a marketplace.
