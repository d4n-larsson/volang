# vo lsp

`vo lsp` makes the toolchain a language server: an editor that speaks the Language Server Protocol shows vo's diagnostics while you type and answers "what is this", "where is it used" and "rename it" with the toolchain's own results. How to start it from each editor is on the [Editors](editors.md) page.

## Synopsis

```text
vo lsp
```

It takes no options. It speaks the protocol on standard input and output and ends when the editor closes the session.

## What it offers

| In the editor | Comes from |
|---|---|
| Problems underlined as you type, with their code and a link to the catalog | [vo check](check.md), on the text in the editor |
| Quick fixes | the fixes `vo check` attaches to a diagnostic |
| Hover: type, signature, documentation | [vo query](query.md) `type-at` |
| Go to definition | the same |
| Find references | `vo query refs` |
| Rename | [vo edit](edit.md) `rename`: across the whole program, or refused with the reason |
| Format document | [vo fmt](fmt.md) |
| Outline | the file's functions, records with fields, variants with cases, constants, effect sets, tests |

Every answer is computed by the command in the right column, so the editor never disagrees with the command line.

It does not offer completion, signature help or semantic colouring in this version.

## Unsaved text

The server works on what the editor holds, not on what is saved: a file you are editing is checked as it is, together with the saved files it imports, and a rename fits the unsaved text. Files that are not open are read from disk at each request, so a change made by another tool (an agent, `git checkout`) is seen at once.

## Which program a file belongs to

Diagnostics, references and rename need the whole program. For a file, the server takes the nearest `main.vo` in its directory or a directory above it, inside the folder the editor opened, whose program imports the file. A file that no such program imports is checked alone, as `vo check FILE` does.

A mistake in an imported file that is not open is shown for that file, and cleared when it is fixed.

## What it never does

The server never runs your program and never writes a file. Rename and format return edits that the editor applies; `vo.effects` is changed only by an edit you accept (a rename updates the function's line there).

## Limits

| What | Limit |
|---|---|
| One check or request | 5 seconds; after that the request fails, and for a check the editor shows one notice |
| A message | 64 MiB |
| Open documents followed | 512 |

Checks start 150 ms after the last keystroke, and only the newest text's diagnostics are published. Each check or request runs as its own process: a text that makes the checker fail cannot end the session.
