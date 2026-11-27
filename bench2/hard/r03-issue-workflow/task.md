# Issue tracker: a workflow for issue states

## The existing program

The program is an issue tracker. It reads commands from standard input, one per line, and prints the response to each command. Words on a line are separated by one or more spaces; blank lines are ignored. A *number* is a non-empty string of the digits `0`-`9`. The input is ASCII.

**Clock.** Every command (every non-blank line, also one that fails or is unknown) takes one tick of a logical clock: the first command's tick is 1, the next one's 2, and so on, except that `wait N` takes the tick N after the previous command's. Every successful change of an issue sets the issue's *updated* tick to the tick of the command; the issue also keeps its *created* tick. Every successful change (of a user or an issue) is also recorded as an *event*: the tick and the message the command printed, for `log`.

**Issues** are numbered 1, 2, 3, ... in creation order and written `#N`. An issue has a title (its words joined by single spaces), a priority `p1` (most urgent) to `p4`, an optional assignee (a user), a sorted set of labels, comments, and a status, `open` or `closed`. A new issue is `open`. An `ID` argument must be a number (else `error: bad command`) naming an existing issue (else `error: unknown issue #N`, N the number's value).

Commands:

- `user NAME`: adds a user. `user NAME added`, or `error: user NAME exists`.
- `users`: one line per user in byte order, `NAME (since T): K open, M closed` (T the tick the user was added; K and M count the open and the closed issues assigned to the user), or `no users`.
- `new P TITLE...`: creates an issue with priority P. `created #N: TITLE`, or `error: bad priority P` unless P is `p1` to `p4`.
- `assign ID USER`: errors in this order: `error: issue #N is closed`, `error: unknown user USER`, `error: #N is already assigned to USER`. Prints `#N assigned to USER`, or `#N reassigned from OLD to USER` when it had an assignee.
- `unassign ID`: `#N unassigned`, or `error: issue #N has no assignee`.
- `label ID OP...`: each OP is `+LABEL` or `-LABEL` with at least one character after the sign (else `error: bad command`), applied left to right; adding a present label or removing an absent one does nothing. Prints `#N labels: L1,L2,...` (sorted) or `#N labels: -`.
- `comment ID USER TEXT...`: `comment added to #N`, or `error: unknown user USER`.
- `priority ID P`: `error: bad priority P`; `#N priority unchanged` (not a change); else `#N priority pOLD -> pNEW`.
- `rename ID TITLE...`: `#N title unchanged` (not a change), else `#N renamed`.
- `close ID`: `#N closed`, or `error: issue #N is already closed`.
- `reopen ID`: `#N reopened`, or `error: issue #N is not closed`.
- `dup ID OTHER`: closes issue ID as a duplicate of OTHER (#M). Errors in this order: `error: issue #N cannot duplicate itself`, `error: issue #N is already closed` (ID closed), `error: issue #M is closed` (OTHER closed). It adds the label `duplicate` and a comment by `system` with the text `duplicate of #M`, and prints `#N closed as duplicate of #M`.
- `show ID`: `#N [pP] TITLE`, then `  status: STATUS`, `  assignee: USER` (`-` for none), `  labels: L1,L2` (`-` for none), `  created C, updated U`, `  K comments` (`1 comment`), then one line `  [TICK] AUTHOR: TEXT` per comment, oldest first.
- `list FILTER...`: each filter is `KEY=VALUE` (non-empty KEY and VALUE, else `error: bad filter WORD`); keys `status` (`open`, the default, `closed` or `all`; else `error: bad status VALUE`), `assignee` (a name, or `-` for unassigned issues), `label`, `priority` (`p1`-`p4`, else `error: bad priority VALUE`); another key is `error: unknown filter KEY`, a key given twice `error: duplicate filter KEY`. Filters are checked left to right. Prints the matching issues ordered by priority, then number, one line each, `#N [pP] STATUS ASSIGNEE TITLE` (ASSIGNEE `-` for none; a TITLE longer than 30 characters is shown as its first 27 characters followed by `...`), or `no issues`.
- `search WORD`: the issues whose title or any comment contains WORD, ignoring case, in number order, lines as in `list`; or `no matches`.
- `mine USER`: the open issues assigned to USER, ordered and written as in `list`; `no open issues for USER`; `error: unknown user USER`.
- `stale N`: the open issues whose updated tick is at least N ticks before the current one, ordered by updated tick, then number: `#N idle K ticks: TITLE` (`1 tick`; TITLE shortened as in `list`), or `no stale issues`.
- `stats`: `issues T: K open, M closed`, then for each assignee of some issue, in byte order, `  USER K open, M closed`, then `  unassigned K open, M closed` if some issue has no assignee.
- `labels`: for each label of some issue, in byte order, `LABEL: K open of T` (T issues have the label, K of them open), or `no labels`.
- `log` or `log N` (N at least 1): the last N events (default 5), oldest first, `[TICK] MESSAGE`; or `no events`.
- `wait N` (N at least 1): prints `clock T`, T this command's tick.

A command with the wrong number of arguments or a malformed value as described prints `error: bad command`. A first word that is not a command prints `error: unknown command WORD`. Checks run in this order: the argument count and number formats, then the issue (for `dup`, ID before OTHER), then the rest as listed.

## The change

Replace the `open`/`closed` status with a workflow. An issue's status is now a **state**, one of, in this order: `new`, `triaged`, `in_progress`, `blocked`, `review`, `done`, `wontfix`. `done` and `wontfix` are the *closed* states, the other five the *open* states. Everything the program says or decides about open and closed issues (`assign`, `users`, `mine`, `stats`, `labels`, `stale`, `dup`, the `list` filters, ...) now means an issue in an open or in a closed state. A new issue is in state `new`.

The allowed moves:

| From | To |
|---|---|
| `new` | `triaged`, `wontfix` |
| `triaged` | `in_progress`, `wontfix` |
| `in_progress` | `triaged`, `blocked`, `review` |
| `blocked` | `triaged`, `in_progress` |
| `review` | `in_progress`, `done` |
| `done` | `triaged` |
| `wontfix` | `triaged` |

Every change of an issue's state, by any command, is a **transition**: the issue remembers its tick, the old and the new state and a reason (possibly none), and it is a change of the issue (it sets the updated tick).

- **`move ID STATE [REASON...]`** (at least 2 arguments): REASON is the words after STATE joined by single spaces (none if there are none). Checks, in this order, after the issue: STATE is a state, else `error: unknown state STATE`; the issue is not in STATE, else `error: issue #N is already STATE`; the move is in the table, else `error: cannot move #N from OLD to STATE`; moving to `in_progress` or `review` needs an assignee, else `error: #N needs an assignee for STATE`; moving to `blocked` or `wontfix` needs a reason, else `error: #N needs a reason for STATE`. On success it prints `#N moved OLD -> STATE` (also the event's message).
- **`close ID`**: an issue in a closed state gives the same error as before. An issue in `review` goes to `done` (no reason); one in any other open state goes to `wontfix` with the reason `closed`. The table and the requirements of `move` do not apply. The output and event stay `#N closed`.
- **`reopen ID`**: an issue in an open state gives the same error as before; otherwise it goes to `triaged` (no reason). Output and event stay `#N reopened`.
- **`dup ID OTHER`**: the issue goes to `wontfix` with the reason `duplicate of #M`; its checks, output and event are unchanged.
- **`unassign ID`**: if the issue is in `in_progress` or `review`, it also goes to `triaged` with the reason `unassigned`: the command prints `#N unassigned`, then `#N moved OLD -> triaged`, and records both as events, in that order. In other states only the assignee goes, as before.
- **`show`**: the status line shows the state, `  status: STATE`; for an issue in `blocked` it is `  status: blocked (REASON)`, with the reason of the transition that blocked it. After the `created ..., updated ...` line comes one line per transition of the issue, oldest first, `  [TICK] OLD -> NEW`, followed by ` (REASON)` when the transition has a reason; then the comment lines as before.
- **`list`, `search`, `mine`**: the lines show the state where they showed the status. The `list` filter `status=` takes `open` (any open state; still the default), `closed` (any closed state), `all`, or a state name (only that state); anything else is `error: bad status VALUE`.
- **`stats`**: after its first line comes `  states: STATE COUNT, STATE COUNT, ...`, listing every state that at least one issue is in, in the state order above. It is left out when there are no issues.
- **`stale N`**: issues in `blocked` are never listed; issues in the other open states are listed as before.
- **`cycle`** (new, no arguments): for each issue now in `done`, its cycle time is the tick of its last transition into `done` minus the tick of its first transition ever into `in_progress`. One line per such issue, the longest cycle time first, ties by number: `#N K ticks: TITLE` (`1 tick`; TITLE shortened as in `list`). Then `average A ticks` (`1 tick`), A the sum of the cycle times divided by their count, rounded down. With no issue in `done` it prints only `no done issues`.

Everything else stays exactly as it is.
