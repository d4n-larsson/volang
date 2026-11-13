# Todo list

Keep a todo list driven by commands on standard input, one command per line, and print a response line (or a listing) for each command.

## Program structure

Split the program into at least three source files: command parsing, the task store (tasks, ids, state changes), and the listing/rendering of tasks each in their own file, plus the entry file that reads standard input and drives them.

## Input

Each line is split into fields at runs of spaces; leading and trailing spaces are ignored. Lines without fields are ignored and print nothing. Command words are lowercase and case-sensitive.

A *priority* is a single digit from `1` to `5` (1 is the most urgent). A *tag* is one or more characters, each a lowercase letter `a`-`z` or a digit `0`-`9`. An *id* field names a task: it matches task number `n` only if it is exactly the decimal spelling of `n` (no sign, no leading zeros).

## Commands

| Command | Effect and response |
|---|---|
| `add <priority> <word> ...` | Creates an open task. Its text is the fields after the priority joined by single spaces (at least one is required). Ids are 1, 2, 3, ... in order of creation and are never reused. Prints `added #<id>`. |
| `done <id>` | Marks an open task done: `done #<id>`. If it is already done: `#<id> is already done`. |
| `reopen <id>` | Marks a done task open again: `reopened #<id>`. If it is already open: `#<id> is already open`. |
| `delete <id>` | Removes the task for good: `deleted #<id>`. |
| `tag <id> <tag>` | Adds a tag: `tagged #<id> +<tag>`. If the task already has it: `#<id> already has +<tag>`. |
| `prio <id> <priority>` | Changes the priority: `#<id> is now P<priority>`. |
| `list` | Open tasks, sorted by priority (1 first), then by id ascending. |
| `list done` | Done tasks, in the order they were (last) marked done: the most recently completed is last. A task that was reopened and done again moves to the end. |
| `list +<tag>` | All tasks, open and done, that have the tag, by id ascending. |
| `stats` | `open <n> done <m>` (counts of existing tasks). |

In the response lines `<id>` is the task number in decimal.

## Listing format

Each listed task is one line: `#<id> P<priority> <text>`, then ` +<tag>` for each of its tags in byte order (ASCII order). In `list +<tag>` only, a done task's line ends with ` (done)` after its tags. A listing with no tasks prints the single line `no tasks`.

## Errors

- A line that does not have one of the forms above prints `error: bad command`. The form includes the command word, the exact number of fields (`add` needs at least 3, `list` 1 or 2), a valid priority where one is required, a valid tag where one is required, and for `list` with 2 fields a second field that is `done` or `+` followed by a valid tag.
- Otherwise, if the id field does not name an existing task (never created, already deleted, or not a valid spelling), print `error: no task <field>` with the field as written.

Every output line ends with a newline.
