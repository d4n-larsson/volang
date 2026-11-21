# Research: editor grammars

## R1. Where the word lists come from

**Decision**: the check reads them from where they already are:

| List | Source |
|---|---|
| keywords | the "Keywords" line of `docs/reference.md`, which must equal the keyword entries of `tok_spell` in `src/parse.c` (the check compares the two first) |
| builtin types | the keywords that are types (`int float bool str bytes list map result void`) plus the collection types of the reference's collections section |
| contextual words | `const`, and the constraint names (`ordered`, `key`), read from the reference; they are highlighted only in their position |
| effects | the first column of the reference's effects table |
| literals | `true`, `false` |

Each grammar's lists are extracted from its file (the alternations in the TextMate patterns, the string
lists at the top of `grammar.js`, the `syn keyword` lines, the tuples of the Pygments lexer) and compared
as sets. A difference prints the word, the list and the file.

**Rationale**: the reference is the project's single source of truth and is itself checked against the
toolchain. No fourth copy and no new toolchain command.

**Alternatives**: generating the grammars from a list file (three generators for a dozen words each);
a `vo` command that prints its keywords (new surface for a test's sake).

**Found while planning**: the site lexer lists 8 effects; the language has 12. The check will fail on
that first.

## R2. The TextMate grammar

**Decision**: `editors/vscode/syntaxes/vo.tmLanguage.json`, scope `source.vo`, written by hand, line
oriented (no pattern spans lines, since vo has no multi-line token: strings and comments end with the
line). That is what makes FR-005 hold: an unclosed string ends at the end of its line. Scopes (the
conventional names themes know):

| Piece | Scope |
|---|---|
| `#` comment | `comment.line.number-sign.vo` |
| string, escape, bad escape | `string.quoted.double.vo`, `constant.character.escape.vo`, `invalid.illegal.escape.vo` |
| integer, float | `constant.numeric.integer.vo`, `constant.numeric.float.vo` |
| `true`, `false` | `constant.language.vo` |
| control keywords (`if else while for in match case return break continue try`) | `keyword.control.vo` |
| declaration keywords (`fn record variant test prop import export const let var set new`) | `keyword.declaration.vo` / `storage.type.vo` |
| `and or not` | `keyword.operator.word.vo` |
| `uses requires ensures` | `keyword.other.vo` |
| builtin types | `support.type.vo` |
| `PascalCase` names | `entity.name.type.vo` |
| name after `fn` | `entity.name.function.vo` |
| name before `(` | `entity.name.function.call.vo` (`support.function.vo` for builtins is not attempted: the list is long and changes) |
| annotation `@name` | `entity.name.tag.annotation.vo` → themes colour it via `meta.annotation`/`storage.type.annotation.vo` |
| effect names in a `uses` clause | `support.constant.effect.vo` |
| operators, punctuation | `keyword.operator.vo`, `punctuation.*.vo` |

The VS Code folder has `package.json` (language `vo`, extension `.vo`, the grammar, no activation code)
and `language-configuration.json` (line comment `#`, brackets, auto-closing and surrounding pairs,
indent after `{`/`[`/`(`). It installs by linking the folder into `~/.vscode/extensions`.

**Alternatives**: generating the TextMate grammar from the tree-sitter one (no such tool gives readable
scopes); YAML source converted to JSON (one more step for 400 lines).

## R3. The tree-sitter grammar

**Decision**: `editors/tree-sitter-vo/grammar.js` following `src/parse.c` rule by rule (the reference's
grammar section is the reading order), with the generated `src/parser.c`, `src/grammar.json`,
`src/node-types.json` and `src/tree_sitter/*.h` committed. Generated with the pinned `tree-sitter-cli`
at ABI 14. No external scanner: vo's lexical grammar has no context-dependent token. Comments are
`extras`. Type names and value names are separate tokens (`PascalCase` against `snake_case`), as in the
toolchain's lexer.

Queries: `highlights.scm` with captures every editor knows (`@keyword`, `@keyword.function`,
`@keyword.control`, `@type`, `@type.builtin`, `@function`, `@function.call`, `@variable.parameter`,
`@property`, `@string`, `@string.escape`, `@number`, `@boolean`, `@comment`, `@attribute`, `@operator`,
`@punctuation.bracket`, `@punctuation.delimiter`, `@constant.builtin` for effects, `@module`);
`locals.scm` (functions and blocks as scopes, `let`/`var`/parameters as definitions); `indents.scm` for
Neovim and Helix where their capture names agree, else per editor in the Zed folder and the site's
snippets; `injections.scm` only for comments (the `comment` grammar, for TODO marks), nothing else is
embedded in vo.

Highlight tests in `test/highlight/*.vo` (tree-sitter's own format: a comment under a line names the
capture) cover each class once.

**Rationale**: a hand-written grammar after the real parser is the only way to get a tree shaped for
queries; the corpus check (R4) keeps it honest.

**Alternatives**: an external scanner (not needed); leaving the parser ungenerated in the repository
(every editor would need node to use it).

## R4. Corpus check for the tree-sitter grammar

**Decision**: the corpus is every `*.vo` under `tests/cases`, `examples`, `std` and the `vo` blocks the
site check already extracts are left to the site's own check. A file belongs to the "parses" set when
`vo fmt --check FILE` exits 0 or 1 (it formats only what parses; a syntax error is 65). For those,
`tree-sitter parse --quiet` must report no `ERROR` or `MISSING` node. For the rest, the parse must only
end. `.expected.vo` and files under `expected/` are included like any other.

**Rationale**: `fmt --check` is an existing command whose exit status says exactly "this file has a
syntax tree", without parsing diagnostics.

## R5. Agreement of the TextMate grammar

**Decision**: `editors/check.js` tokenizes each file of the "parses" set with `vscode-textmate` (the
engine VS Code uses) and walks the tree-sitter tree of the same file (through the CLI's XML/JSON output
or the node binding the CLI ships with). For every leaf that is a keyword, string, escape, number,
boolean, comment, annotation, type name or effect name, the TextMate scope at that position must be the
class's scope from the table in `contracts/grammars.md`; and no TextMate token in such a file may carry
an `invalid.` scope. The spec's FR-008 was reworded to say this: the toolchain has no token dump, and
adding one for a test would be new surface; the tree-sitter grammar is already held to the toolchain by
R4.

**Alternatives**: a `vo` command that prints tokens (rejected: surface); a second lexer written in the
test (it would be a fourth grammar to keep right).

## R6. Vim

**Decision**: `syntax/vo.vim` (keyword lists, a one-line string region with escapes, numbers, `#`
comments, `PascalCase` as Type, the name after `fn` as Function), `ftdetect/vo.vim` (`*.vo`), and
`ftplugin/vo.vim` (`commentstring=#\ %s`, four-space indent). The lists join R1's check; when `vim` is
installed, the check also loads the files on a sample and fails on any error message. Neovim users
without tree-sitter can use the same files.

## R7. Tools, skipping and CI

**Decision**: `editors/package.json` (private, never published) pins the three development packages and
`package-lock.json` pins their dependencies; `npm ci` in `editors/` installs them into
`editors/node_modules` (ignored by git). `tests/grammars.py`:

- always: R1 (word lists), and that the committed parser's recorded hash of `grammar.js` matches the
  file (`editors/tree-sitter-vo/src/grammar.sha256`, written by `make grammars`), so a grammar edited
  without regenerating fails even without node;
- with `node_modules`: regenerates into a temporary directory and compares with the committed parser
  (FR-009), runs the highlight tests, R4 and R5;
- without: prints `skip: tree-sitter corpus (run npm ci in editors/)` and the like, and exits 0.

`VO_GRAMMARS=require` (set in CI) turns a skip into a failure. The gate workflow gains
`actions/setup-node` and `npm ci`. `make grammars` regenerates the parser and the hash.

**Rationale**: FR-010 and SC-006; contributors who do not touch grammars need nothing new.

## R8. Editor configuration

**Decision** (the site page `tools/editors.md` carries the exact snippets):

- **VS Code**: link `editors/vscode` into the extensions folder.
- **Neovim**: a `vim.treesitter.language.register`/nvim-treesitter parser entry pointing at this
  repository with `location = "editors/tree-sitter-vo"`, the `queries` copied or linked to
  `queries/vo/`, and the Vim `ftdetect`.
- **Helix**: a `[[language]]` and `[[grammar]]` entry in `languages.toml` with
  `source = { git = ..., rev = ..., subpath = "editors/tree-sitter-vo" }`, then `hx --grammar fetch` and
  `build`, and the queries in `runtime/queries/vo/`.
- **Zed**: `editors/zed/` is a Zed extension (`extension.toml` with the grammar's repository, revision
  and `path`; `languages/vo/config.toml`; the queries with Zed's capture names), installed as a dev
  extension.
- **Vim**: copy `editors/vim/*` into `~/.vim`.
- **GitHub**: needs the language in GitHub's Linguist, which asks for wide use first (hundreds of
  repositories) and a grammar repository it can pull in; the page says so. `.gitattributes` gets
  `*.vo linguist-language=Vo`, which has no effect until then and marks intent.

Editors present on this machine: Vim only. VS Code, Zed, Neovim and Helix are verified by the grammar
checks, not by eye, and recorded as not verified by hand (SC-005).

## R9. Colours

**Decision**: none are added. Grammars assign classes; the user's theme colours them, and Nord themes
exist for each of these editors. The one place this project chooses colours, the site's code blocks
(`site/docs/stylesheets/extra.css`), already uses the Nord palette, with hues darkened or lightened
where Nord's own value misses the 4.5:1 contrast the site check requires. The editors page shows vo
code through that same style. If a screenshot or sample theme is ever added, it uses Nord.

## R10. Documentation

**Decision**: one site page (`tools/editors.md`, listed in `tools/site.py`); `editors/README.md` (what
each folder is, how to regenerate, licence of the generated runtime header); roadmap, README (a line),
changelog (Unreleased), the review table. The reference is not touched.
