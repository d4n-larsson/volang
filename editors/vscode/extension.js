// Starts `vo lsp` for vo files. Everything the editor shows (diagnostics, quick fixes, hover, go to
// definition, references, rename, formatting, the outline) comes from that server, which is the vo
// toolchain itself; this file only connects the two. Needs `npm install` in this folder once (the
// editor's language-client library) and `vo` on the PATH (or the setting `vo.path`).
'use strict';
const vscode = require('vscode');
const { LanguageClient } = require('vscode-languageclient/node');

let client;

exports.activate = function () {
  const command = vscode.workspace.getConfiguration('vo').get('path') || 'vo';
  client = new LanguageClient(
    'vo',
    'vo',
    { command, args: ['lsp'] },
    { documentSelector: [{ scheme: 'file', language: 'vo' }] },
  );
  return client.start();
};

exports.deactivate = function () {
  return client ? client.stop() : undefined;
};
