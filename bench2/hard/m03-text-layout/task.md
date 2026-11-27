# Text layout

Write a document formatter. It reads a document with formatting directives from standard input and prints it laid out in lines of a fixed width, split into numbered pages with a header and a footer.

## Program structure

Split the program into at least three source files: the directives (reading them, the settings they change, and their errors), line filling (breaking a paragraph into lines, alignment, list markers and headings), and pagination (placing blocks on pages under the keep rules, headers and footers) each in their own file, plus the entry file that reads standard input and prints the result.

## Input

The input is ASCII text without tabs. Lines are numbered from 1, counting every line.

- A line whose first character is `.` is a *directive line*.
- A line that is empty or holds only spaces is a *blank line*.
- Every other line is a *text line* (a text line may start with spaces).

A *paragraph* is a maximal run of consecutive text lines; blank lines, directive lines and the end of the input end it. The *words* of a paragraph are the runs of non-space characters of all its lines, in order.

## Settings

| Setting | Default | Changed by |
|---|---|---|
| width `W` | 40 | `.width N` |
| page height `H` | 20 | `.page N` |
| indent `I` | 0 | `.indent N` |
| alignment | `left` | `.align A` |
| title | empty | `.title` |

A paragraph is laid out with the settings in effect when it ends (a directive line always ends the paragraph before it, so these are also the settings when it starts).

## Directives

A directive line is split into fields at runs of spaces; the first field is the directive name. A *number* argument is 1 to 3 ASCII digits (leading zeros allowed).

- `.width N`: one number, `20 <= N <= 100` and `N - I >= 20`.
- `.page N`: one number, `4 <= N <= 200`.
- `.indent N`: one number with `W - N >= 20`.
- `.align A`: `A` is one of `left`, `right`, `center`, `justify`.
- `.title`: any number of fields; the title becomes those fields joined by single spaces (no fields: the title becomes empty).
- `.heading`: one or more fields; adds a heading block (see Headings) whose text is the fields joined by single spaces, with the letters `a`-`z` changed to uppercase.
- `.list bullet` or `.list number`: opens a new list inside the lists already open.
- `.endlist`: no fields; closes the innermost open list.
- `.break`: no fields; the next block starts on a new page (see Pagination).
- `.space N`: one number, `1 <= N <= 9`; vertical space before the next block (see Blocks and spacing).
- `.contents`: no fields; a table of contents is printed after the pages (see Output).

Errors, checked in this order:

1. An unknown name is the error `unknown directive <name>` (`<name>` is the first field as written, including the dot).
2. A wrong number of fields or an invalid argument is `bad argument`.
3. `.width` or `.page` after a text line or a valid `.heading` line earlier in the input is `too late`.
4. `.list` when three lists are already open is `too deep`; `.endlist` when no list is open is `no list`.

A directive with an error changes nothing. Lists still open at the end of the input are closed without any message.

## Line filling

A paragraph is filled into lines of at most `A` characters, where `A` depends on where the text starts (see Lists). Words go in order, separated by one space:

- A word goes on the current line if it fits there (the line's length plus one space plus the word is at most `A`, or the line is empty and the word is at most `A` long).
- Otherwise, if the word is at most `A` long, it starts a new line.
- Otherwise the word is longer than `A`: the current line (if not empty) is finished, and the word is cut into pieces of `A` characters, the last piece holding the rest. Each piece but the last is a line of its own; the last piece starts a new line that the following words may join.

Each line is then aligned within `A` columns, where `L` is its length:

- `left`: as it is.
- `right`: `A - L` spaces in front.
- `center`: `(A - L) / 2` spaces in front, rounded down.
- `justify`: every line that holds at least two words (a piece of a cut word counts as a word), except the paragraph's last line, is widened to exactly `A` characters by adding spaces to the gaps between its words. With `E = A - L` extra spaces and `G` gaps, every gap gets `E / G` (rounded down) more spaces, and the first `E mod G` gaps from the left get one more each. The paragraph's last line and lines with one word stay as they are.

## Lists

While no list is open, a paragraph's lines start at column `I` (the line is prefixed with `I` spaces) and `A = W - I`.

While lists are open, each paragraph is an *item* of the innermost open list. The lists are numbered by depth: the outermost open list has depth 1, a list opened inside it depth 2, and so on. For an item of a list at depth `d`:

- The marker is `*` at depth 1, `-` at depth 2, `+` at depth 3 for a bullet list, and `<n>.` for a number list, where `<n>` counts the items of that list from 1 (a list's count continues after a list nested in it closes).
- The marker starts at column `M = I + 2 * (d - 1)` and the text at column `T = M + (length of the marker) + 1`. `A = W - T`.
- The first line is `M` spaces, the marker, one space, then the aligned line. Every other line is `T` spaces, then the aligned line.

## Headings

A heading block has two lines: `I` spaces followed by the heading's *shown text*, which is the heading text cut to its first `W - I` characters if it is longer, without trailing spaces; then `I` spaces followed by as many `=` as the shown text has characters. Headings ignore lists and the alignment.

## Blocks and spacing

The document is a sequence of *blocks*: each paragraph and each valid `.heading`, in input order. The *separator* between two consecutive blocks that are on the same page is one empty line, except that nothing separates two consecutive blocks that are items of the same list. If valid `.space` directives came between the two blocks, the separator is instead as many empty lines as the total of their `N` (also between items of the same list). A separator is only placed between two blocks on the same page: a block that starts a page never has empty lines above it. Every output line has its trailing spaces removed.

## Pagination

Each page has `H` lines of body. Blocks are placed in order. A page is *empty* until a line is placed on it. Let `used` be the number of body lines already on the current page, and `s` the number of separator lines a block would get after the previous block on this page (see above).

The *need* of a block is the number of its lines that must fit on a page that is not empty for the block to start there:

- a paragraph of `n` lines: `n` if `n <= 3`, otherwise 2;
- a heading: 2 if it is the last block or a valid `.break` comes between it and the next block; otherwise `2 + t + need(next block)`, where `t` is the separator the next block would get after the heading, because a heading is kept on the same page as the start of the block after it.

To place a block:

1. If a valid `.break` came after the previous block (or anywhere before the first block) and the current page is not empty, start a new page.
2. If the current page is not empty and `used + s + need > H`, start a new page.
3. A heading is placed whole (after the separator if the page is not empty).
4. A paragraph of `n` lines: on a page that is not empty, if `used + s + n <= H` it is placed whole after the separator; otherwise the separator and its first `k = min(H - used - s, n - 2)` lines are placed, and the rest continues on a new page. On an empty page, if the remaining `m` lines fit (`m <= H`) they are placed; otherwise the first `min(H, m - 2)` lines are placed and the rest continues on a new page. So a paragraph never leaves fewer than two lines on either side of a page break, and paragraphs of 2 or 3 lines are never split.

Each block takes the title in effect when the block's last input line was read.

## Output

Pages are numbered from 1; `P` is the number of pages. Each page is printed as:

1. the header: the title of the block whose line is first on the page, cut to its first `W` characters, preceded by `(W - length) / 2` spaces (rounded down); an empty title gives an empty line;
2. an empty line;
3. the body: for every page but the last, exactly `H` lines (unused lines are empty); for the last page, only up to its last placed line;
4. an empty line;
5. the footer: `page <p> of <P>` preceded by `(W - length) / 2` spaces (rounded down).

A document without blocks prints no pages.

If the input has at least one valid `.contents` directive and at least one heading, the pages are followed by a line `CONTENTS` and then one line for each heading, in order: the heading's shown text, one space, a run of dots (`.`), one space, and the number of the page that holds the heading. The run of dots is as long as needed to make the line exactly `W` characters long; if that would leave fewer than 3 dots, the text is cut to its first characters so that there are exactly 3 dots.

After that, print one line `error line <N>: <message>` for each directive with an error, in input order. Every output line ends with a newline.
