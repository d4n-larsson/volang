# Editor support for vo

Grammars and editor files. How to install each: [the site's Editors page](../site/docs/tools/editors.md).

| Folder | What | Read by |
|---|---|---|
| `vscode/` | a TextMate grammar (`syntaxes/vo.tmLanguage.json`, scope `source.vo`) in a VS Code extension folder | VS Code, GitHub's Linguist, JetBrains, Sublime Text |
| `tree-sitter-vo/` | a tree-sitter grammar (`grammar.js`), its generated parser (`src/`, ABI 14) and queries | Neovim, Helix, Zed |
| `zed/` | a Zed extension around the tree-sitter grammar | Zed |
| `vim/` | syntax, file detection and settings | Vim, Neovim |

The grammars are written by hand and held to the language by `tests/grammars.py` (part of `make test`):
their word lists must equal the reference's and the parser's, the tree-sitter grammar must parse every
`.vo` file in the repository that the toolchain parses, and the TextMate grammar must give every token
the class the tree-sitter grammar gives it.

## Changing a grammar

```sh
npm ci                        # here, once: the tree-sitter generator and a TextMate tokenizer
make -C .. grammars           # after editing tree-sitter-vo/grammar.js: regenerates src/ and its hash
python3 ../tests/grammars.py  # all checks
```

The tools are needed only for this. Without them `make test` skips the checks that need them and says
so; CI has them (`VO_GRAMMARS=require`).

`zed/languages/vo/highlights.scm` is a copy of `tree-sitter-vo/queries/highlights.scm` (the check
compares them). `package.json` and `package-lock.json` here are development tools only and are never
published.

## Licences

Everything here is under the repository's MIT licence. `tree-sitter-vo/src/parser.c` and
`src/tree_sitter/*.h` are produced by the tree-sitter generator (MIT, © Max Brunsfeld and contributors).
