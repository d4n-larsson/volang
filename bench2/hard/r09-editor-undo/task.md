# Line editor: buffers, undo and a clipboard

## The existing program

The program is a small line editor in the style of `ed`. It holds one buffer of text lines and a *current line*, reads commands from standard input, one per line, and prints the response to each command. Empty input lines are ignored. Input is printable ASCII and spaces. A *number* is a non-empty string of the digits `0`-`9`; inputs keep every number below 1000000. Line N is the N-th line of the buffer (from 1). The current line is 0 when the buffer is empty.

**Splitting a command line.** The command word is the text up to the first space (or the whole line). For `append TEXT`, TEXT is everything after that first space, kept exactly (it may be empty and may hold any spaces); `append` without a space is `error: bad command`. For `insert ADDR TEXT` and `replace ADDR TEXT`, ADDR runs up to the next space and TEXT is everything after that single space, kept exactly; without that space: `error: bad command`. For `find TEXT`, TEXT is everything after the first space and must not be empty. Every other command takes *words*: the text after the command word split on spaces, empty words dropped.

**Addresses.** An address is a number, `.` (the current line) or `$` (the last line, 0 in an empty buffer). A RANGE is one address A (meaning A to A) or two, `A,B`. Resolving a RANGE: anything else is `error: bad command`; then an empty buffer is `error: empty buffer`; then an address outside 1 to the number of lines L is `error: line V out of range` (V its value; A is checked before B); then A > B is `error: bad range A,B`. A single LINE resolves the same way (no comma allowed). The address of `insert` may be 1 to L + 1 (no empty-buffer error), and the address AFTER of `move` may be 0 to L, both with the same `bad command` / `out of range` errors.

**Commands** (no output unless one is given; "cur" is the current line afterwards):

- `append TEXT`: adds a line at the end; cur = the new line. `insert ADDR TEXT`: inserts before line ADDR; cur = ADDR. `replace LINE TEXT`: replaces the line; cur = LINE.
- `delete RANGE`: removes the lines; cur = A, or the last line if fewer than A lines remain (0 if empty). `clear`: removes every line; cur = 0.
- `move RANGE AFTER`: moves the lines so that they follow line AFTER (numbered before the move; 0 puts them first); `error: cannot move lines into themselves` if A ≤ AFTER < B. cur = the last moved line.
- `dup RANGE`: inserts a copy of the lines right after line B; cur = the last copy.
- `join RANGE`: joins the lines into one, separated by single spaces; one address A joins A and A + 1. `error: nothing to join` for `A,A`, or for one address that is the last line. cur = A.
- `split LINE COL`: splits the line after its first COL bytes; `error: bad column COL` unless 1 ≤ COL < the line's length. cur = LINE + 1.
- `subst RANGE OLD NEW`: replaces every occurrence of the word OLD by the word NEW in each line (non-overlapping, left to right); prints `N substitutions` (`1 substitution`). If N > 0, cur = B.
- `upper RANGE`, `lower RANGE`, `strip RANGE` (removes leading and trailing spaces), `indent RANGE N` (prepends N spaces, N from 1 to 8, else `error: bad command`), `sort RANGE` (byte order), `reverse RANGE`: change the lines; cur = B.
- `wrap RANGE W` (W ≥ 1): refills each line separately: its words are put greedily, single-spaced, on lines of at most W bytes (a longer word gets a line of its own; a line without words becomes one empty line). cur = the last resulting line.
- `dedupe`: removes every line equal to the line before it; prints `removed N lines` (`1 line`); cur stays, but at most the number of lines.
- `print [RANGE]`: prints each line as its number right-aligned to the width of the buffer's last line number, two spaces, then the text; without a RANGE the whole buffer (`error: empty buffer` if empty). cur = B. `goto LINE`: cur = LINE and prints it the same way.
- `find TEXT`: `found at N1, N2, ...` (every line containing TEXT) and cur = the first of them, or `not found`.
- `count`: `L lines, W words, B bytes` (each line counts its length + 1 bytes; singular `1 line` etc.). `where`: `line C of L`. `longest`: `longest line N (B bytes)` for the first longest line (`error: empty buffer` if empty).

A command with the wrong number of words, or a word that is not as described, prints `error: bad command`. A command word that is not a command prints `error: unknown command WORD`. Every check happens before anything changes.

## The change

Add named buffers, per-buffer undo and redo, a clipboard, undo groups and a saved state. Below, *L* is a buffer's line count and `plural` means the singular word when the count is 1 (`1 line`, `2 lines`, `0 lines`, `1 step`, `1 change`). Each new command takes exactly the words shown (`[K]`: zero or one); otherwise, or when a word is not as described, it prints `error: bad command` and nothing happens; that check comes before all others.

**Buffers.** The editor starts with one empty buffer `main`, which is current. Every buffer has its own lines, current line, undo history and saved state; all the existing commands work on the current buffer.

- `open NAME` (NAME: one or more of the characters `a`-`z` and `0`-`9`): if a group is open, first ends it exactly as `endgroup` does (printing its line). Then, if no buffer NAME exists, creates an empty one, makes it current and prints `new buffer NAME`; otherwise makes it current (it may already be) and prints `buffer NAME, L lines`.
- `close NAME` and `close! NAME`: checked in this order: `error: no buffer NAME`; `error: buffer NAME is current`; for `close` only, `error: buffer NAME has unsaved changes` if it is *modified* (see Saving). Otherwise the buffer and everything it holds are gone (a later `open NAME` makes a new, empty one) and the program prints `closed NAME`. Any buffer may be closed, `main` too, as long as it is not current.
- `buffers`: one line per buffer in byte order of the names: `*` for the current buffer or a space for the others, a space, NAME, a space, `L lines`, then `, modified` if it is modified. Example: `* main 3 lines, modified` and `  notes 1 line`.
- `where` now prints `buffer NAME, line C of L`.

**Undo steps.** A command other than those listed in this section (`open`, `close`, `close!`, `buffers`, `where`, `undo`, `redo`, `group`, `endgroup`, `save`) that succeeds and leaves the current buffer's lines different from what they were before it (another number of lines, or another text in some line) records one *undo step* in that buffer. A command that fails, or leaves every line as it was, records nothing, even if it printed something or moved the current line: for example `subst` with no substitution or with OLD equal to NEW, `sort` of lines already in order, `replace` with the same text, a `move` that puts the lines where they are, `dedupe` that removes nothing, `clear` of an empty buffer, `print`, `goto` and `find`. A step remembers the buffer's lines and current line just before and just after its command.

Recording a step empties that buffer's redo list.

**Undo and redo.** `undo [K]` and `redo [K]` (K a number of at least 1, default 1) work on the current buffer. After their words, they are checked in this order: `error: group open` while a group is open; `error: nothing to undo` (`error: nothing to redo`) when there is no step to undo (redo); `error: only N steps to undo` (`to redo`, with `plural`) when K is larger than the N steps there are. On an error nothing changes. Otherwise:

- `undo K` takes back the last K steps, newest first, moving them to the redo list. The buffer's lines and current line become exactly what they were just before the oldest of those K steps. Prints `undone K steps` (`plural`).
- `redo K` reapplies the last K undone steps, the most recently undone first. The lines and current line become exactly what they were just after the newest of those K steps. Prints `redone K steps` (`plural`).

**Clipboard.** One clipboard holds a list of lines for all buffers; it starts empty and only these commands change it (undo and redo never touch it):

- `yank RANGE`: copies the lines into the clipboard (replacing what it held), prints `yanked N lines` (`plural`). The buffer and its current line do not change.
- `cut RANGE`: copies the lines into the clipboard, then removes them exactly like `delete`; prints `cut N lines` (`plural`). It records an undo step.
- `paste AFTER` (AFTER as for `move`: 0 to L, `.` is 0 in an empty buffer): checked in this order: the address, then `error: clipboard empty`. Inserts a copy of the clipboard after line AFTER; cur = the last pasted line; prints `pasted N lines` (`plural`). It records an undo step.

Their RANGE and AFTER errors are those of the existing commands.

**Groups.** `group` starts an undo group in the current buffer and prints `group started` (`error: group already open` if one is open). `endgroup` ends it (`error: no group open` without one): every undo step recorded in the buffer since its `group` becomes one single step, whose *before* is that of the first of them and whose *after* that of the last of them; it prints `grouped N changes` (`plural`; N is the number of steps merged, possibly 0 or 1, and 0 adds no step). While a group is open, `undo`, `redo` and `save` fail with `error: group open`. `open` ends an open group first (see above). A group still open at the end of the input has no further effect.

**Saving.** `save` marks the current buffer's present state as saved and prints `saved NAME, L lines` (`error: group open` while a group is open). A buffer is *modified* when its number of undo steps is not the number it had at its last `save` (0 for a buffer that was never saved), with one exception: once the saved state can no longer be reached — a step was recorded while the saved state was among the undone steps (that is, while the buffer had fewer undo steps than at its last save) — the buffer stays modified until it is saved again. So undoing or redoing back to the saved state makes a buffer unmodified, but editing it back to the same text does not. Merging steps at `endgroup` does not change whether a buffer is modified.

Everything else stays exactly as it is.
