# Editors

vo code is highlighted in VS Code, Zed, Neovim, Helix and Vim with the grammars in the repository's [`editors/`](https://github.com/d4n-larsson/volang/tree/main/editors) directory, and VS Code, Neovim and Helix can start the toolchain as a language server ([vo lsp](lsp.md)) for diagnostics, quick fixes, hover, go to definition, references, rename and formatting. Nothing is published to an editor's marketplace yet: each editor is pointed at a checkout of the repository.

The language server needs `vo` on your `PATH`.

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

`.vo` files then have highlighting, `#` comment toggling, bracket matching and closing, and indentation after an opening bracket.

For the language server, install the editor's client library in that folder once (it needs Node.js), and restart:

```text
cd volang/editors/vscode && npm install
```

The setting `vo.path` names the toolchain to start if `vo` is not on the `PATH`. Without `npm install` the highlighting still works and VS Code reports that the extension could not be activated. The same folder works in editors built on VS Code (their extensions directory has another name). The grammar file, `syntaxes/vo.tmLanguage.json`, is a TextMate grammar: JetBrains editors and Sublime Text can load it as well.

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

The language server needs no plugin:

```text
vim.api.nvim_create_autocmd("FileType", {
  pattern = "vo",
  callback = function(args)
    vim.lsp.start({
      name = "vo",
      cmd = { "vo", "lsp" },
      root_dir = vim.fs.root(args.buf, { "vo.effects", "vo.packages", ".git" }),
    })
  end,
})
```

## Helix

Add the language and its grammar to `~/.config/helix/languages.toml`:

```text
[[language]]
name = "vo"
scope = "source.vo"
file-types = ["vo"]
comment-token = "#"
indent = { tab-width = 4, unit = "    " }

language-servers = ["vo"]

[language-server.vo]
command = "vo"
args = ["lsp"]

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

`editors/zed` is a Zed extension for highlighting, brackets and the outline. In Zed, run "zed: install dev extension" and choose that folder. It does not start the language server: Zed starts one only from extension code compiled to WebAssembly, which this folder does not have yet. Its `extension.toml` names the grammar's repository and revision; set `rev` to the commit you cloned, or point `repository` at your checkout with a `file://` address.

## Vim

Copy the three files into your Vim directory:

```text
cp -r volang/editors/vim/* ~/.vim/
```

This gives highlighting, `.vo` file detection, `#` comments and four-space indentation.

## GitHub

GitHub colours a language once it is in its language list (Linguist), and a language is added there only when it is already in wide use across many repositories. Until then `.vo` files on GitHub are plain text. The TextMate grammar is the format GitHub needs, so nothing else has to be written when that time comes.

## What is not there

The language server does not offer completion, signature help or colouring by meaning yet. Agents use the toolchain through [vo mcp](mcp.md) instead.
