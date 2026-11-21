# Feature Specification: Editor Grammars

**Feature Branch**: `037-editor-grammars`

**Created**: 2026-11-21

**Status**: Implemented (gate not run)

**Input**: User description: "Editor grammars for vo: syntax highlighting outside the documentation site.
Today the only highlighter is the Pygments lexer the site uses; the repository has no TextMate grammar
and no tree-sitter grammar, so vo source is plain grey text on GitHub, in VS Code, Zed, Neovim and Helix.
Deliver, in an `editors/` directory: a TextMate grammar with a minimal VS Code extension folder; a
tree-sitter grammar with its generated parser and queries; a Vim syntax file if nearly free. The grammars
must agree with the real language, checked against every `.vo` file in the repository and against the
toolchain's own lexer, so a keyword added without updating them fails `make test`. Source:
docs/review-2026-11/ecosystem.md, Tooling, 'Editor grammar' (roadmap step 3). Must keep: one source of
truth for keyword and builtin lists, no new build dependency for the toolchain, the reference unchanged.
Out of scope: the language server, semantic highlighting, marketplace publishing, the Linguist pull
request itself."

Background: vo is written mostly by agents, but it is read by people: in a review on GitHub, in the
editor where the agent works, in a terminal. Everywhere except vo's own documentation site, a vo file is
uncoloured text, because every tool that colours code asks for one of two grammar formats and the
repository has neither. A reviewer comparing a vo diff with a Go or Python diff sees the difference
before reading a line. The two formats cover nearly everything: one is read by GitHub, VS Code, the
JetBrains editors and Sublime Text; the other by Zed, Neovim and Helix.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Coloured vo in VS Code and on GitHub (Priority: P1)

A developer opens a `.vo` file in VS Code with the vo extension folder installed and sees keywords,
types, strings, numbers, comments, annotations and effects coloured, with comment toggling, bracket
matching and sensible indentation. The same grammar file is the one GitHub's highlighter accepts.

**Why this priority**: VS Code and its derivatives are where most agents and most reviewers work, and
this grammar format reaches the most tools.

**Independent Test**: every `.vo` file in the repository is tokenized by the grammar with a reference
implementation of the format; each token the toolchain's lexer reports gets the expected class, and no
part of any file is left in an "invalid" class.

**Acceptance Scenarios**:

1. **Given** any vo file the toolchain can lex, **When** tokenized with the grammar, **Then** every
   keyword, string, number, comment and annotation the toolchain's lexer reports has the matching class,
   over the whole file.
2. **Given** a declaration (`fn`, `record`, `variant`, `test`, `prop`, `const`, `import`), **When**
   highlighted, **Then** the declared name has a class of its own (function, type, constant, module), so
   a reader finds definitions by colour.
3. **Given** a `uses` clause, **When** highlighted, **Then** the effect names are distinct from ordinary
   identifiers, including scoped ones like `file.read("data")`.
4. **Given** a type position (parameters, results, fields, `new`), **When** highlighted, **Then** builtin
   types and user types are coloured as types, including generic ones.
5. **Given** the extension folder, **When** installed in VS Code, **Then** `.vo` files are recognised,
   line comments toggle, brackets and quotes close and match, and pressing Enter after `{` indents.
6. **Given** a file that is incomplete or wrong (an unclosed string, a half-typed function), **When**
   highlighted, **Then** the rest of the file is still coloured: a mistake never greys out what follows
   the line it is on.

---

### User Story 2 - Coloured vo in Zed, Neovim and Helix (Priority: P1)

A developer adds the vo grammar to Zed, Neovim or Helix with the few lines the site gives and gets the
same colouring, plus what a syntax tree gives those editors: selection by syntax node and structural
navigation.

**Why this priority**: these editors cannot use the first format at all; without the second they stay
grey.

**Independent Test**: the grammar parses every syntactically valid `.vo` file in the repository with no
error node, and a set of highlight tests (source lines with the expected class under each token) passes.

**Acceptance Scenarios**:

1. **Given** every `.vo` file in the repository that the toolchain parses without a syntax error,
   **When** parsed with the grammar, **Then** the tree has no error or missing node.
2. **Given** the files the toolchain rejects with a syntax error (the parse-error test cases), **When**
   parsed, **Then** parsing ends (no hang or crash) and the valid parts keep their structure.
3. **Given** the highlight queries, **When** applied, **Then** the classes of Story 1 (keywords, declared
   names, types, effects, strings, numbers, comments, annotations) are assigned, using the standard class
   names those editors' themes know.
4. **Given** the documented configuration for each of the three editors, **When** followed on a clean
   installation, **Then** a `.vo` file is highlighted.
5. **Given** a checkout of the repository, **When** the grammar is used, **Then** no generator has to be
   run first: the generated parser is in the repository and is the one the grammar source produces.

---

### User Story 3 - The grammars cannot drift from the language (Priority: P1)

A contributor adds a keyword, a builtin type, an effect or a literal form to vo. `make test` fails until
the grammars know it, and says which grammar and which word.

**Why this priority**: a grammar that is right today and wrong after the next spec is worse than none;
vo changes with every spec.

**Independent Test**: add a word to the toolchain's keyword list in a scratch build; the check fails
naming the word and each grammar that lacks it.

**Acceptance Scenarios**:

1. **Given** the keyword, builtin-type and effect lists of the toolchain, **When** the check runs,
   **Then** each grammar's lists equal them, and a difference is reported with the word and the grammar.
2. **Given** the checked-in generated parser, **When** the grammar source has changed without
   regenerating, **Then** the check fails and says how to regenerate.
3. **Given** a machine without the grammar tools, **When** `make test` runs, **Then** the checks that
   need them are skipped with one clear line each, and everything that needs no tool (the word lists,
   the first grammar's corpus check if its engine is absent: also skipped, clearly) still runs; the
   toolchain builds and tests without them.
4. **Given** the continuous-integration run, **When** it executes, **Then** the tools are present and no
   grammar check is skipped.

---

### User Story 4 - Vim, and where to find all of it (Priority: P3)

A Vim user gets a syntax file; every user finds one page on the site that says, per editor, what to
copy or add. The page also says how GitHub highlighting will come (the grammar must be submitted to
GitHub's language list, which has its own conditions).

**Why this priority**: small, and it completes the set.

**Independent Test**: the Vim file loads without error on a `.vo` file and colours the same word lists;
the site page lists each editor with its steps and the site's checks pass.

**Acceptance Scenarios**:

1. **Given** the Vim syntax and file-type files, **When** a `.vo` file is opened, **Then** keywords,
   types, strings, numbers and comments are coloured and the word lists pass the check of Story 3.
2. **Given** the site page, **When** read, **Then** it has steps for VS Code, Zed, Neovim, Helix and Vim,
   and a paragraph on GitHub.
3. **Given** the repository, **When** viewed by tools that read language attributes, **Then** `.vo` files
   are marked as vo.

---

### Edge Cases

- Strings: every escape the language accepts is coloured as an escape; an invalid escape is not; a
  string never runs past the end of its line unless the language lets it.
- Text and byte literals, if the language distinguishes them by prefix, are both strings.
- Numbers: every form the lexer accepts (integers, floats, exponents, separators or prefixes if any),
  and a `-` before a number stays an operator.
- `..` in ranges next to numbers (`0..n`) is not read as a decimal point.
- Comments: line comments to the end of the line; documentation comments directly above an item have
  no special class unless free.
- Annotations (`@spec("ID")` and the others) with their arguments.
- Keywords used as record field names or after a dot, where the language allows it: follow the
  toolchain's lexer.
- `vo-fmt`-style non-canonical code (odd spacing, several statements on a line) highlights the same.
- Generic brackets `list[map[str, int]]` nested several deep; function types.
- `match` cases with nested patterns; `try`; `new` records and lists; holes and `todo`.
- Very long lines and files of several thousand lines: no visible delay.
- Markdown code blocks tagged `vo` in editors that support embedded languages: highlighted where the
  format gives it for free.
- The step-case and expected-output files next to tests are not vo and are not claimed.

## Requirements *(mandatory)*

### Functional Requirements

**The grammars**

- **FR-001**: The repository MUST contain a grammar in the format read by GitHub's highlighter, VS Code,
  JetBrains editors and Sublime Text, assigning the conventional class names to keywords, declared
  names, types, effects, strings and escapes, numbers, comments, annotations, operators and punctuation.
- **FR-002**: A VS Code extension folder MUST accompany it, declaring the language (`.vo`), comment
  syntax, brackets, auto-closing and surrounding pairs and indentation rules, installable by copying or
  linking the folder.
- **FR-003**: The repository MUST contain a syntax-tree grammar in the format read by Zed, Neovim and
  Helix, with its generated parser checked in, and queries for highlighting; indentation, local-scope
  and injection queries where they follow directly from the grammar.
- **FR-004**: The repository MUST contain Vim syntax and file-type detection files.
- **FR-005**: A mistake in a file MUST NOT remove highlighting from the lines after it (both grammars
  recover).

**Agreement with the language**

- **FR-006**: The keyword, builtin-type and effect lists of every grammar MUST be checked against the
  toolchain's own lists as part of `make test`; a difference fails with the word and the grammar named.
- **FR-007**: The syntax-tree grammar MUST parse every `.vo` file in the repository that the toolchain
  parses, with no error node; this is checked in `make test` when the tool is present.
- **FR-008**: The first grammar's tokens MUST agree, on keywords, strings, numbers, comments and
  annotations, with the syntax-tree grammar's tokens for every `.vo` file in the repository that the
  toolchain parses (the syntax-tree grammar being held to the toolchain by FR-007); checked in
  `make test` when the tools are present.
- **FR-009**: The checked-in generated parser MUST equal what the grammar source generates; checked when
  the generator is present.
- **FR-010**: Building and testing the toolchain MUST NOT require any grammar tool; a missing tool skips
  its checks with a clear message. Continuous integration MUST have the tools, so nothing is skipped
  there.

**Finding and installing**

- **FR-011**: The site MUST have a page with installation steps for VS Code, Zed, Neovim, Helix and Vim,
  and what is needed for GitHub.
- **FR-012**: The repository MUST mark `.vo` files with their language for tools that read such
  attributes.
- **FR-013**: The language reference, the toolchain binary and every existing test case MUST be
  unchanged, apart from what exposing the word lists to the check requires.

### Key Entities

- **Token class**: what a piece of source is for colouring (keyword, type, string...), named by each
  format's conventions.
- **Word lists**: the language's keywords, builtin types and effect names; owned by the toolchain.
- **Corpus**: every `.vo` file in the repository, split into those the toolchain parses and those it
  rejects for syntax.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of the repository's syntactically valid vo files parse with the syntax-tree grammar
  with no error node.
- **SC-002**: On 100% of the repository's lexable vo files, the first grammar agrees with the
  toolchain's lexer on every keyword, string, number, comment and annotation.
- **SC-003**: Adding a keyword to the toolchain without touching the grammars makes `make test` fail
  with a message naming the word (shown once by experiment).
- **SC-004**: A vo file of 5,000 lines is highlighted by each grammar in under one second.
- **SC-005**: Each of VS Code, Zed, Neovim, Helix and Vim shows a highlighted `.vo` file after following
  the site's steps; verified by hand where the editor is available, and recorded as not verified where
  it is not.
- **SC-006**: `make test` on a machine without the grammar tools passes, with the skipped checks named.

## Assumptions

- Class names follow each format's common conventions, so existing colour themes work; vo defines no
  editor theme. Wherever this project does choose colours (today only the documentation site's code
  blocks, which already use the Nord palette adjusted for contrast), they are Nord colours.
- The word lists are taken from the toolchain at test time (from its sources or from a command), not
  kept in a fourth file by hand; the plan chooses how.
- The site's own highlighter is not replaced; if cheap, its word lists join the same check.
- The generated parser is committed (it is what editors fetch); regenerating needs the generator, which
  is a development tool only.
- Nothing is published: no marketplace extension, no pull request to GitHub's language list, no
  submission to the editors' grammar registries. The site page says what each would take.
- Semantic colouring (by what a name resolves to) belongs to the language server, a later item.
- Linux is the only place the editors are tried here; the grammars themselves are platform-independent.
