# Library lending

Run a small library from a log of dated commands on standard input: books with several copies, members with loan limits, waiting queues, late fines and payments. Print a response for each command and a final report.

## Program structure

Split the program into at least three source files: command parsing, the library state and its rules (books, members, loans, queues, fines), and the final report each in their own file, plus the entry file that reads standard input and drives them.

## Input

Lines are numbered from 1; every line counts. A line is split into fields at runs of spaces; leading and trailing spaces are ignored; lines without fields are ignored and print nothing.

Every other line is `<day> <command> <arguments...>`, where `day` is 1 to 6 ASCII digits (leading zeros allowed). Member ids and ISBNs are any fields.

| Command | Arguments |
|---|---|
| `book` | `<isbn> <copies>`, copies is 1 or 2 digits and at least 1 |
| `member` | `<id> <limit>`, limit is one digit from `1` to `9` |
| `borrow` | `<id> <isbn>` |
| `return` | `<id> <isbn>` |
| `pay` | `<id> <amount>`, amount is `D.CC`: 1 to 6 digits, `.`, exactly 2 digits; greater than zero |

A line that does not have exactly one of these forms, or whose day is smaller than the day of the last line that was not invalid, prints `line <N>: invalid` and changes nothing.

## Rules

Money is kept in cents and printed as dollars with exactly two decimals: whole dollars without leading zeros (`0` under one dollar), `.`, two digits (`0.25`, `12.00`). Day numbers are printed in decimal without leading zeros.

A member's *loans* are the ISBNs they currently hold, at most one copy per ISBN. A book's *shelf* count is its copies not on loan. Every book has a *queue* of waiting member ids.

- `book`: adds copies (a new ISBN starts with 0). Prints `book <isbn>: <total> copies` with the new total of copies, then *serves* the book.
- `member`: if the id exists, prints `error: member <id> exists`; otherwise adds the member with that loan limit, owing 0, and prints `member <id> joined`.
- `borrow`: checks in this order and prints the first that applies:
  1. unknown member: `error: no member <id>`; unknown ISBN: `error: no book <isbn>`;
  2. the member owes more than 5.00: `<id> blocked: owes <money>`;
  3. the member already has this ISBN: `<id> already has <isbn>`;
  4. the member's number of loans is at least their limit: `<id> at limit`;
  5. the shelf count is 0: if the member is already in the book's queue, `<id> already waiting for <isbn>`; otherwise the member joins the end of the queue and it prints `<id> waits for <isbn> (position <k>)`, `k` being the 1-based queue position;
  6. otherwise the member borrows a copy, due on day `day + 14`: `<id> borrows <isbn> (due <due>)`.
- `return`: unknown member or ISBN as in step 1 of `borrow`; if the member does not have the ISBN: `error: <id> does not have <isbn>`. Otherwise the copy goes back to the shelf. With `late = day - due`, if `late > 0` the member is charged a fine of `min(late * 25, 1000)` cents and it prints `<id> returns <isbn> late <late> days, fine <money>`; otherwise `<id> returns <isbn>`. Then the book is served.
- `pay`: unknown member as in step 1; if the amount is greater than what the member owes: `error: <id> owes only <money>`; otherwise it is subtracted and it prints `<id> pays <amount>, owes <money>` (both as money).
- *Serving* a book: while its shelf count is above 0 and its queue is not empty, remove the first id from the queue. If that member would pass checks 2 to 4 of `borrow`, they borrow a copy, due on the current line's day + 14, printing `<isbn> -> <id> (due <due>)`; otherwise it prints `<id> skipped for <isbn>`.

## Report

After the last line print:

- `members:` and then, for each member in byte order of id (ASCII order): `<id> loans <isbns> owes <money>`, where `<isbns>` is the member's loans in byte order joined by `,`, or `-` if there are none.
- `books:` and then, for each ISBN in byte order: `<isbn> <shelf>/<copies> queue <ids>`, where `<ids>` is the queue in order joined by `,`, or `-` if it is empty.

Every output line ends with a newline.
