# Contract: `vo lsp`

```sh
vo lsp        # no options, no arguments; anything else is T0001 (exit 64)
```

Language Server Protocol 3.17 on stdin/stdout (`Content-Length` framing).

## Capabilities announced

```json
{"positionEncoding": "utf-16",
 "textDocumentSync": {"openClose": true, "change": 1, "save": {"includeText": false}},
 "hoverProvider": true,
 "definitionProvider": true,
 "referencesProvider": true,
 "renameProvider": {"prepareProvider": true},
 "documentFormattingProvider": true,
 "documentSymbolProvider": true,
 "codeActionProvider": {"codeActionKinds": ["quickfix"]}}
```

`positionEncoding` is `utf-8` when the client lists it in `general.positionEncodings`.
`serverInfo`: `{"name": "vo", "version": "<vo version>"}`.

## Methods

| Method | Answer |
|---|---|
| `initialize`, `initialized`, `shutdown`, `exit` | the protocol's; exit status 0 after `shutdown`, else 1 |
| `textDocument/didOpen`, `didChange` (full text), `didSave`, `didClose` | none; diagnostics follow |
| `textDocument/publishDiagnostics` (from the server) | per file: `Diagnostic[]` with `range`, `severity` (1 error, 2 warning), `code`, `codeDescription.href`, `source: "vo"`, `message` |
| `textDocument/codeAction` | `CodeAction[]`, kind `quickfix`, one per toolchain fix |
| `textDocument/hover` | `{contents: {kind: "markdown", value}, range}` or `null` |
| `textDocument/definition` | `Location` or `null` |
| `textDocument/references` | `Location[]` |
| `textDocument/prepareRename` | `Range` or `null` |
| `textDocument/rename` | `WorkspaceEdit` (`changes`) or error `-32803` with the toolchain's message |
| `textDocument/formatting` | `TextEdit[]` (empty when canonical or unparsable) |
| `textDocument/documentSymbol` | `DocumentSymbol[]` |
| `$/cancelRequest` | the request, if running, is stopped and answered `-32800` |
| any other request | `-32601` |

## Errors

| Code | When |
|---|---|
| `-32700`, `-32600` | a body that is not JSON; not a request object |
| `-32002` | a request before `initialize` |
| `-32601` | a method not offered |
| `-32602` | parameters of the wrong shape |
| `-32800` | cancelled by the client |
| `-32803` | the request failed: the toolchain refused (its message), the copy ended badly, or the time limit passed |

## Rules

- Files: `file://` addresses; anything else is ignored (no diagnostics, empty answers).
- The program of a file: the nearest `main.vo` at or above it, inside the workspace folder, whose
  program loads the file; else the file itself.
- Open documents are used as the editor holds them; other files are read from disk at each request.
- Checks start 150 ms after the last change; only the newest text's diagnostics are published.
- Limits: 5 s per request or check; 64 MiB per message (a larger one, or a frame without
  `Content-Length`, ends the session with status 1); 512 open documents are followed.
- Positions: the toolchain counts columns in code points, from 1; the protocol's characters are UTF-16
  units (or bytes when `utf-8` is negotiated), from 0.
- A quick fix's title is made from its edit: "Insert `;`", "Replace with `str(1)`", "Remove it" (the
  toolchain's fixes carry no description).
- The server never runs a program and never writes a file.
