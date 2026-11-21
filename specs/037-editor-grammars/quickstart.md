# Quickstart: validating the editor grammars

```sh
make                                   # build/vo, for the corpus split
(cd editors && npm ci)                 # the grammar tools (optional for everything else)
python3 tests/grammars.py              # all checks; see contracts/grammars.md
VO_GRAMMARS=require python3 tests/grammars.py   # as CI runs it: no skips allowed
```

Expected last line: `grammars: 7 checks, 0 skipped, 0 failures` (with `vim` installed).

Without `editors/node_modules`: the word-list, hash and Vim checks run, four lines say `skip:`, exit 0.

## The drift experiment (SC-003)

Add a word to the reference's Keywords line (or to `tok_spell` in `src/parse.c`) and run the check: it
must fail naming the word and each grammar file. Undo.

## By eye

```sh
cd editors/tree-sitter-vo && npx tree-sitter highlight ../../examples/expenses/main.vo
ln -s "$PWD/editors/vscode" ~/.vscode/extensions/vo     # then open a .vo file in VS Code
vim -u NONE -c 'set rtp+=editors/vim' -c 'syntax on' examples/expenses/main.vo
```

The per-editor steps are on the site page `tools/editors.md`.
