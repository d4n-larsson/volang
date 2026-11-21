# Contract: the grammars and their check

## Files

| Path | Read by |
|---|---|
| `editors/vscode/syntaxes/vo.tmLanguage.json` (scope `source.vo`) | VS Code, GitHub Linguist, JetBrains, Sublime Text |
| `editors/vscode/package.json`, `language-configuration.json` | VS Code |
| `editors/tree-sitter-vo/grammar.js`, `src/parser.c` (ABI 14), `queries/{highlights,locals,indents,injections}.scm` | Neovim, Helix, Zed |
| `editors/zed/extension.toml`, `languages/vo/` | Zed |
| `editors/vim/{syntax,ftdetect,ftplugin}/vo.vim` | Vim, Neovim |

Language id `vo`; file extension `.vo`; line comment `#`; no block comment.

## The check (`python3 tests/grammars.py`, part of `make test`)

| Check | Needs | Fails when |
|---|---|---|
| word lists | nothing | a word of the reference/toolchain lists is missing from a grammar, or a grammar has one the language lacks; prints `LIST: WORD missing in FILE` / `... extra in FILE` |
| grammar hash | nothing | `grammar.js` changed since `make grammars` |
| generated parser | node tools | regenerating gives other bytes than `src/` |
| highlight tests | node tools | a capture in `test/highlight/*.vo` differs |
| tree-sitter corpus | node tools, `build/vo` | a file `vo fmt --check` accepts has an `ERROR`/`MISSING` node, or any file makes the parser fail to end |
| TextMate agreement | node tools | a checked class (data-model.md) has another scope at a token, or an `invalid.` scope appears in a file that parses |
| Vim | `vim` | loading the syntax files prints an error |

Without the node tools each of those lines prints `skip: NAME (run npm ci in editors/)` and the script
exits 0; with `VO_GRAMMARS=require` a skip is a failure. The last line is
`grammars: N checks, S skipped, F failures`.

## Regenerating

```sh
cd editors && npm ci      # once
make grammars             # tree-sitter generate (ABI 14) and the grammar hash
```
