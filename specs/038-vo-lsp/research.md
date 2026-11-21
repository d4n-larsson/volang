# Research: `vo lsp`

## R1. Transport, lifecycle, capabilities

**Decision**: stdio with the protocol's `Content-Length` framing; JSON-RPC 2.0. A message over 64 MiB
is refused. `initialize` answers the capabilities of contracts/lsp.md and nothing else; before it, other
requests get `-32002` (server not initialized). `shutdown` answers `null`; `exit` ends with status 0
after `shutdown`, 1 without. End of input ends the server (status 1, the editor is gone). Unknown
requests get `-32601`; unknown notifications are ignored; `$/` requests are refused the same way.
`positionEncoding`: `utf-8` when the editor offers it, else `utf-16` (R6). Text sync: full documents
(`TextDocumentSyncKind.Full`), open/close, and save without text.

## R2. Every answer in a copy of the server

**Decision**: as spec 036: fork, the copy computes one answer and exits; the server collects the
copy's output and forwards it only if the copy ended with status 0 and the output is complete. The copy
inherits the open documents through its memory, so nothing is sent to it. Deadline 5 s (R8): the server
kills the copy's process group. The copy dies with the server.

A copy that fails (a non-zero status, a signal, the deadline) gives: for a request, the protocol's
`RequestFailed` (`-32803`) with one sentence; for a diagnostics run, nothing is published and the
editor gets one `window/showMessage` warning per session ("vo could not check FILE within 5 seconds").

**Rationale**: isolation, no state to reset, no leak, and the commands can be used unchanged although
they exit on their own in many places.

## R3. Unsaved text: the open documents as loaded files

**Decision**: before running a command, the copy changes directory to the entry file's directory and
calls `file_add(path, text, len)` for every open document that lies under it, with the path in the
form the loader builds (the entry's name; `dir/file.vo` for imports). `load_source` and `load_file`
look a path up with `file_find` before reading the disk, and `load_file` lexes and parses a registered
file that has no syntax tree yet, so the editor's text is used wherever that file is loaded, and the
disk for everything else. Open documents the program does not import stay registered and unused.

**To verify in step 2**: the exact path forms (`inside_tree` opens the path only to resolve links, and
treats a missing file as inside); that a registered, unused file does not appear in a command's output;
package files (`pkg_cached` is skipped for a registered path, which is right: the editor's text wins,
and the package-hash diagnostic still comes from the manifest check).

**Alternatives**: writing buffers to temporary files (paths in every answer would be wrong, and files
would be written); a virtual file layer in `read_whole_file` (wider than needed: `file_find` is already
the layer).

## R4. Capturing a command's output, and its writes

**Decision**: two small hooks:

- `out_capture` in `src/sys.c`: when set, `out_bytes`/`out_cstr` for descriptors 1 and 2 append to a
  buffer in the copy's memory instead of writing. The copy sets it, calls `vo_main` with the command's
  words and `--json`, and translates the buffer when the command ends. Commands often end through
  `vo_exit`; `vo_exit` calls `lsp_finish` (a function pointer, null outside a copy) before flushing, so
  the translation happens on every path.
- `write_capture` in `write_file_safely` (`src/main.c`): when set, the new text is kept instead of
  written. `vo fmt FILE` then yields the formatted text, and no command can write a file in a copy
  whatever it is asked (FR-003). `vo.effects` is written through its own function: the copy never runs
  `effects --update`, and that function gets the same guard.

**Rationale**: the commands' printed JSON is the stable interface (schema 1); translating it keeps one
source of every answer (FR-018). Capturing in memory avoids a second process and lets the translation
use the copy's loaded file texts.

**Alternatives**: calling the checker's and query's internal functions directly (their results are
printed as they are found, not returned); translating in the server process (it would need the texts of
unopened files and a heap).

## R5. Requests and the commands behind them

| Request | In the copy | Answer |
|---|---|---|
| diagnostics (after open/change/save) | `check --json ENTRY` | one `textDocument/publishDiagnostics` per file of the program that is open or has diagnostics; `diagnostic` records → `Diagnostic` (`range`, `severity` 1 or 2, `code`, `codeDescription.href` = the site's catalog page, `source: "vo"`, `message`); empty lists for open files without any |
| `textDocument/codeAction` | `check --json ENTRY` again | for each diagnostic whose range touches the asked range, one `CodeAction` (`kind: quickfix`, `title` = the fix's description, `edit.changes` from the fix's edits, `diagnostics: [it]`); checking again keeps the server stateless and costs a few milliseconds |
| `textDocument/hover` | `query --json type-at ENTRY FILE:L:C` | Markdown: a `vo` code block with `text: type` or the signature, then the description; `range` from the record; `null` when there is no record |
| `textDocument/definition` | the same | `Location` from `declaration`; `null` without one or when it lies in `std/` |
| `textDocument/references` | `query --json refs ENTRY FILE:L:C` | `Location[]` of the records, without the `declaration` role unless `context.includeDeclaration` |
| `textDocument/prepareRename` | `query --json type-at` | the record's range when it has a `declaration` outside `std/`; else `null` |
| `textDocument/rename` | `edit --json --dry-run rename ENTRY FILE:L:C NEW` | `WorkspaceEdit.changes` from the `edit` records (the `vo.effects` edit included: it is a text file the editor can change); a `toolchain_error` (T0006 and the like) → `RequestFailed` with its message |
| `textDocument/formatting` | `fmt --json FILE` with the write captured | one `TextEdit` replacing the lines between the first and last line that differ; `[]` when nothing is captured (canonical, or a syntax error) |
| `textDocument/documentSymbol` | `load_file` only, then a walk over the file's top-level items | `DocumentSymbol[]`: kinds Function, Struct (record, children Field), Enum (variant, children EnumMember), Constant, Interface (effect set), Method (test, prop: their text as the name) |

A `toolchain_error` record for an unreadable entry (T0002) or an unknown target gives an empty answer,
not an error: half-written code is the normal case.

## R6. Positions

**Decision**: the toolchain's `line` is 1-based and its `col` 1-based in the unit the lexer counts
(established in step 2 with a non-ASCII case; bytes or code points). The copy converts with the line's
text, which it has for every file a record names (`file_find`): to UTF-16 units by default, to bytes
when `utf-8` was negotiated, both 0-based; and back for request positions. A `\r` before the newline is
not a character of the line. Positions past the end of a line or of the text are clamped. One pair of
functions, tested on a table of mixed-width lines in both encodings.

## R7. The program of a file, and what is checked when

**Decision**: for an open document the server tries, in a copy, the nearest `main.vo` at or above the
file's directory, not above the workspace folder: if that program loads the file, it is the entry;
else the next `main.vo` further up; else the file itself. The result is kept with the document and
tried first next time. One check per distinct entry serves all its open files.

After a change the server waits for 150 ms without another change, then checks the entries of all open
documents, the changed document's first. A change that arrives while a check runs kills that copy
(newest wins, FR-005). Requests are answered in order between checks; a request for a document with a
pending check does not wait for it (it runs the command on the current text itself).

The server remembers, per entry, which files it last published non-empty diagnostics for, and
publishes an empty list for those that have none in the next run (a mistake in an unopened, imported
file must be cleared when it is fixed).

## R8. Limits and memory

**Decision**: 5 s per copy (a deadline in the server's wait); memory: the checker's own arena limit and
the runtime's default limit apply in the copy as on the command line. In the server: one mapped buffer
per open document (unmapped on close or replacement), one mapped buffer per incoming message (unmapped
after it), one for a copy's output (grown by remapping, unmapped after forwarding, limit 64 MiB), a
table of open documents that grows by remapping. No static array over 64 KiB. SC-004 measures the
resident size over 5,000 requests.

## R9. Shared with `vo mcp`

**Decision**: `src/jsonrpc.c` gets, from `src/mcp.c`: the in-place reader (`js_skip`, `js_get`,
`js_member`, `js_is`, string decoding into a caller's buffer), the buffered writer with JSON string
escaping and UTF-8 replacement, `utf8_seq`, and one function that runs a callback in a forked copy with
optional input, two capped output buffers, a deadline, and a hook called when the server's input has
something (for cancellation). `src/mcp.c` keeps its line framing, tool table and results. `tests/mcp.py`
is the regression check for the move.

## R10. Editors

- **VS Code**: `editors/vscode/extension.js` (about 25 lines: start `vo lsp` with
  `vscode-languageclient` for the `vo` language; setting `vo.path`, default `vo`) and `package.json`
  (`main`, `activationEvents`, the dependency). The folder needs `npm install` once; the site says so.
  Without it the grammar still works and VS Code reports that the extension could not start.
- **Zed**: `editors/zed/extension.toml` cannot start a server without extension code compiled to
  WebAssembly (Rust); instead the site gives the user-settings lines that attach `vo lsp` to the language
  (`"lsp": {...}` with a custom binary) if Zed accepts them for an extension language, else it is
  recorded as not available in Zed in this version.
- **Neovim**: `vim.lsp.start({ name = "vo", cmd = { "vo", "lsp" }, root_dir = ... })` in a `FileType`
  autocommand. **Helix**: `[language-server.vo] command = "vo", args = ["lsp"]` and
  `language-servers = ["vo"]` in the language entry.
- None of these editors is installed here; the server is verified by the scripted client, the editor
  side by reading each editor's documentation, and SC-005 is recorded accordingly.

## R11. Documentation

The reference: `vo lsp` in the command list and one bullet (≤ 50 tokens). A site page `tools/lsp.md`:
what it offers, the program rule, limits, what it does not do (completion). The editors page gains the
start lines. Roadmap, README, changelog, review table. Usage messages that list commands gain `lsp`
(three golden outputs change by that word).

## R12. Tests

`tests/lsp.py`, part of `make test`, through `bin/vo`:

- lifecycle; capabilities exactly as the contract;
- diagnostics equal `vo check --json` (range converted, severity, code, message) on a set of files with
  each kind of diagnostic the fixture can produce, single- and multi-file, saved and unsaved, with
  non-ASCII text, in both position encodings; dependents updated; cleared on fix and on close; newest
  wins under a burst of changes;
- every quick fix applied equals the toolchain's fix applied (`tests/apply_fixes.py`'s logic);
- hover, definition, references, prepareRename, rename (applied edits equal `vo edit rename` on a copy),
  formatting (applied edit equals `vo fmt` on a copy), document symbols;
- robustness: garbage text, deep nesting, positions out of range, unopened and missing files, malformed
  frames and messages, cancellation, a copy over its deadline (a generated pathological file if one can
  be found, else a test hook that sleeps), the editor going away;
- `--session N`: SC-004 memory; `--time`: SC-003.
