# Data Model: `vo lsp`

## Open document (the server's only lasting state)

| Field | Meaning |
|---|---|
| `uri`, `path` | the address as sent, and its file path |
| `version` | the editor's version number (echoed in published diagnostics) |
| `text`, `len` | a mapped copy of the editor's text |
| `entry` | the entry file of its program as last found (R7), or none yet |
| `dirty` | changed since it was last checked |

## Published set

Per entry: the files that had diagnostics in the last run, so the next run can clear them.

## Session

`initialized`, `shutdown_seen`, `utf8_positions`, the workspace folder, the time of the last change.

## A copy's result

Its exit status and its output: complete framed messages to forward. For a diagnostics run also, on a
second pipe: the entry that turned out to hold each document, and the files published.

## Position

Toolchain: (line 1-based, column 1-based in the lexer's unit). Protocol: (line 0-based, character
0-based in UTF-16 units or bytes). Converted with the line's text only (R6).
