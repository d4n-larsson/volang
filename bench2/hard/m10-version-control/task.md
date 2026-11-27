# Version control

Simulate a small version control system driven by commands read from standard input, one command per line: a working tree of text files, numbered commits, branches, history, and three-way merges. Print what each command prints, in order.

## Program structure

Split the program into at least three source files: the repository (commits, branches, revisions and ancestry), the three-way merge, and the commands (one function per command, with their checks and messages) each in their own file, plus the entry file that reads standard input, splits lines and dispatches to the commands.

## Model

- The *working tree* maps file paths to texts. A path is any field (it contains no spaces); a text is any string, possibly empty. At the start the working tree is empty.
- A *commit* has a *number*, a list of *parents* (zero, one or two commit numbers, in order), a *message*, and a *snapshot*: a copy of a working tree. Commits are numbered 1, 2, 3, … in the order they are made, across all branches. Commits never change and are never removed.
- A *branch* has a name and points at a commit, except that at the start there is a single branch `main` that points at no commit. Exactly one branch is the *current branch*; at the start it is `main`.
- The *head* is the commit the current branch points at, if any. The *head snapshot* is the head's snapshot, or an empty tree when there is no head.
- A commit A is *contained in* a commit B when A is B, or A is contained in one of B's parents.
- The *changes* from tree X to tree Y are, for every path in either tree: `A <path>` if it is only in Y (added), `D <path>` if it is only in X (deleted), `M <path>` if it is in both with different texts (modified). They are listed in byte order of the paths (ASCII order; a path that is a prefix of another comes first). There are *uncommitted changes* when the changes from the head snapshot to the working tree are not empty.
- A *revision* (REV) names a commit:
  - A REV that contains `~` is split at its last `~` into X and N. N must be one or more ASCII digits; the REV names the commit reached from the commit X names (X is itself a REV, and may contain `~`) by following the first parent N times (N = 0 names X's commit itself).
  - Otherwise, a REV of one or more ASCII digits names the commit with that number (leading zeros allowed).
  - Otherwise it names the commit the branch of that name points at.

  A REV that names no existing commit (an unused number, an unknown branch, a branch that points at no commit, a bad N, or a commit without parents reached before N steps are done) is the error `error: unknown revision <REV>`, with the whole REV as written.

## Input

A line is split into fields at runs of spaces; leading and trailing spaces are ignored; lines without fields are ignored and print nothing. The first field is the command word and the rest are its arguments. Where a command takes a text or a message, it is its argument fields from that point on joined by single spaces.

An unknown command word, or a known one with a number of arguments it does not accept, prints `error: bad command` and changes nothing.

Every error a command reports is one line starting with `error: `, and a command that reports an error changes nothing. When a command lists several checks, they are made in the order listed and only the first failing one is reported.

## Commands

**`write PATH [TEXT...]`** (1 or more arguments) sets PATH in the working tree to the text (empty if there are no text fields), adding the path if it is not there. Prints nothing.

**`delete PATH`** (1 argument) removes PATH from the working tree. If it is not there: `error: no such file <PATH>`. Prints nothing otherwise.

**`commit MESSAGE...`** (1 or more arguments). If there are no uncommitted changes: `error: nothing to commit`. Otherwise it makes a new commit whose parents are the head alone (none if there is no head), with the message and a snapshot of the working tree, points the current branch at it, and prints `[<branch> <number>] <message> (<a> added, <m> modified, <d> deleted)`, counting the changes from the old head snapshot to the new snapshot.

**`status`** (no arguments) prints `on <branch>`, then the changes from the head snapshot to the working tree, one per line, or `clean` if there are none.

**`branch NAME [REV]`** (1 or 2 arguments) creates a branch pointing at the commit REV names, or at the head if REV is omitted. Checks: a NAME made only of ASCII digits, or containing `~`, is `error: invalid branch name <NAME>`; an existing name is `error: branch <NAME> exists`; an unknown REV is reported as above; without REV and with no head, `error: no commits yet`. Prints nothing. The current branch does not change.

**`branches`** (no arguments) prints one line per branch in byte order of the names: `*` for the current branch or a space for the others, a space, the name, a space, and the commit number it points at or `-` if none (`* main 4`, `  fix 2`).

**`unbranch NAME`** (1 argument) deletes a branch. Checks: no branch of that name is `error: no such branch <NAME>`; the current branch is `error: cannot delete the current branch`; a branch whose commit is not contained in the head is `error: branch <NAME> is not merged`. Then it prints `deleted branch <NAME> (was <number>)`.

**`checkout NAME`** (1 argument) makes NAME the current branch. Checks: no branch of that name is `error: no such branch <NAME>`; the current branch is `error: already on <NAME>`; uncommitted changes are `error: uncommitted changes`. Then the working tree becomes a copy of the new head snapshot, and it prints `switched to <NAME>`.

**`reset REV`** (1 argument) points the current branch at the commit REV names and makes the working tree a copy of its snapshot; uncommitted changes are discarded. Commits that no branch points at any more still exist and can be named by number. Prints `head is now <number>`.

**`log [REV]`** (0 or 1 arguments) prints the history starting at the commit REV names, or at the head if REV is omitted (with no head: `error: no commits yet`): that commit, then its first parent, then that commit's first parent, and so on until a commit without parents. Each commit is a line `<number> <message>`, and a commit with two parents adds ` (merge of <second parent number>)` at the end.

**`show REV`** (1 argument) prints `commit <number>`, then `parents <p1> <p2>` with the parent numbers in order separated by single spaces (`parents none` if there are none), then `message <message>`, then one line per file of its snapshot in byte order of the paths: `<path> "<text>"` (the text exactly, between double quotes; no escaping).

**`diff REV1 REV2`** (2 arguments) prints the changes from the snapshot of REV1 to the snapshot of REV2, one per line, or `no differences`. REV1 is checked before REV2.

**`merge REV`** (1 argument) merges the commit REV names (*theirs*) into the head (*ours*). Checks: an unknown REV; no head is `error: no commits yet`; uncommitted changes are `error: uncommitted changes`. Then:

1. If theirs is contained in ours, it prints `already up to date` and changes nothing.
2. Otherwise, if ours is contained in theirs, the current branch is pointed at theirs and the working tree becomes a copy of its snapshot. No commit is made. It prints `fast-forward to <number>`.
3. Otherwise the *base* is the commit with the highest number among the commits contained in both ours and theirs. If there is none, the base snapshot is an empty tree. Each path that is in at least one of the three snapshots (base, ours, theirs) is merged on its own. Write B, O and T for the path's text in the base, ours and theirs snapshots, or *absent* if the path is not in that snapshot:
   - If O equals T (both absent counts as equal), the result is O.
   - Otherwise, if O equals B, the result is T; otherwise, if T equals B, the result is O.
   - Otherwise the path is a *conflict*: `both added` if B is absent, `deleted by us` if O is absent, `deleted by them` if T is absent, and `both modified` otherwise.

   An absent result means the path is not in the merged tree.
4. If there are conflicts, it prints `conflict: <path> (<kind>)` for each, in byte order of the paths, then `merge aborted`, and changes nothing (this is not an error line).
5. Otherwise it makes a new commit whose parents are ours and theirs (in that order), with the message `merge <REV> into <branch>` (REV as written) and the merged tree as its snapshot, even if that tree equals ours. The current branch is pointed at it, the working tree becomes a copy of the merged tree, and it prints the same line as `commit` does, counting the changes from the ours snapshot to the merged tree.

**`pick REV`** (1 argument) applies the changes one commit made on top of the head. Checks: an unknown REV; no head is `error: no commits yet`; uncommitted changes are `error: uncommitted changes`; a commit with two parents is `error: cannot pick a merge`. The paths are then merged exactly as in `merge` step 3, with the base snapshot being the snapshot of the picked commit's parent (an empty tree if it has none), ours the head snapshot and theirs the picked commit's snapshot. Conflicts are printed as in `merge` step 4, ending with `pick aborted`. If the merged tree equals the head snapshot, the error is `error: nothing to pick`. Otherwise it makes a new commit whose only parent is the head, with the message `<picked message> (picked from <number>)` and the merged tree as its snapshot, points the current branch at it, makes the working tree a copy of the merged tree, and prints the `commit` line (changes counted from the head snapshot).

**`revert REV`** (1 argument) undoes the changes one commit made, on top of the head. It works exactly like `pick`, except: a commit with two parents is `error: cannot revert a merge`; the base snapshot is the reverted commit's snapshot and theirs is the snapshot of its parent (an empty tree if it has none); conflicts end with `revert aborted`; an unchanged tree is `error: nothing to revert`; and the message is `revert <number>`.

## Output

Every output line ends with a newline.
