# Editors

vo code is highlighted in VS Code, Zed, Neovim, Helix and Vim with the grammars in the repository's [`editors/`](https://github.com/d4n-larsson/volang/tree/main/editors) directory. Nothing is published to an editor's marketplace yet: each editor is pointed at a checkout of the repository.

The grammars only say what each piece of code is (a keyword, a type, a string, an effect); the colours are your theme's. They are checked against the toolchain on every `.vo` file in the repository, so they follow the language as it changes.

```text
git clone https://github.com/d4n-larsson/volang
```

The steps below call that directory `volang`.

## VS Code

Link the extension folder into VS Code's extensions directory and restart the editor:

```text
ln -s "$PWD/volang/editors/vscode" ~/.vscode/extensions/vo
```

`.vo` files then have highlighting, `#` comment toggling, bracket matching and closing, and indentation after an opening bracket. The same folder works in editors built on VS Code (their extensions directory has another name). The grammar file, `syntaxes/vo.tmLanguage.json`, is a TextMate grammar: JetBrains editors and Sublime Text can load it as well.

## Neovim

With the `nvim-treesitter` plugin, register the parser, then install it with `:TSInstall vo`:

```text
local configs = require("nvim-treesitter.parsers").get_parser_configs()
configs.vo = {
  install_info = {
    url = "https://github.com/d4n-larsson/volang",
    location = "editors/tree-sitter-vo",
    files = { "src/parser.c" },
    branch = "main",
  },
  filetype = "vo",
}
vim.filetype.add({ extension = { vo = "vo" } })
```

Copy or link the queries to where Neovim looks for them:

```text
mkdir -p ~/.config/nvim/queries
ln -s "$PWD/volang/editors/tree-sitter-vo/queries" ~/.config/nvim/queries/vo
```

Without tree-sitter, use the Vim files below: they work in Neovim too.

## Helix

Add the language and its grammar to `~/.config/helix/languages.toml`:

```text
[[language]]
name = "vo"
scope = "source.vo"
file-types = ["vo"]
comment-token = "#"
indent = { tab-width = 4, unit = "    " }

[[grammar]]
name = "vo"
source = { git = "https://github.com/d4n-larsson/volang", rev = "main", subpath = "editors/tree-sitter-vo" }
```

Then fetch and build the grammar, and link the queries:

```text
hx --grammar fetch
hx --grammar build
mkdir -p ~/.config/helix/runtime/queries
ln -s "$PWD/volang/editors/tree-sitter-vo/queries" ~/.config/helix/runtime/queries/vo
```

Helix asks for a commit in `rev`; replace `main` with the commit you cloned (`git -C volang rev-parse HEAD`).

## Zed

`editors/zed` is a Zed extension. In Zed, run "zed: install dev extension" and choose that folder. Its `extension.toml` names the grammar's repository and revision; set `rev` to the commit you cloned, or point `repository` at your checkout with a `file://` address.

## Vim

Copy the three files into your Vim directory:

```text
cp -r volang/editors/vim/* ~/.vim/
```

This gives highlighting, `.vo` file detection, `#` comments and four-space indentation.

## GitHub

GitHub colours a language once it is in its language list (Linguist), and a language is added there only when it is already in wide use across many repositories. Until then `.vo` files on GitHub are plain text. The TextMate grammar is the format GitHub needs, so nothing else has to be written when that time comes.

## What is not there

Highlighting is by syntax only. Colouring by meaning (a name's kind after checking), completion, and diagnostics in the editor need a language server, which vo does not have yet. Agents get all of that today from [vo mcp](mcp.md).
