# Mail delivery

Split mail spool files from the directory `spool/` into one mailbox file per local user under `out/`, and print a delivery log.

## Input

Standard input, line 1: `domain <domain>` (the local mail domain, lowercase). Line 2: `quota <n>`, the most bytes any one user's mailbox may receive in this run (a non-negative integer). Every further non-empty line is a spool name; spool `<s>` is the file `spool/<s>.mbox`.

Handle the spools in input order. A spool name that is not 1 to 20 characters, each a lowercase ASCII letter, a digit or `-`, gives `skip <s>: bad spool name` (such a name must never be used to open a file, nor may anything outside `spool/` be read); a spool whose file does not exist gives `skip <s>: no such spool`. The same spool may be listed more than once; it is then read again.

## Spool files

A spool file's **lines** are its text split at newline characters (a newline at the very end does not start another line). A line starting with `From ` (with the space) starts a **message**; the message is that line and all lines after it up to the next such line or the end of the file. Lines before the first message are ignored. Messages are numbered 1, 2, ... within their spool file.

In a message, the lines after the `From ` line up to the first empty line are its **header lines** (all of them if there is no empty line). A header line starting with a space or a tab continues the header above it; any other header line is `<name>:<text>` (header lines without `:` are ignored, and so are continuation lines following them). Header names compare without regard to ASCII letter case (`TO`, `To` and `to` are the same). A header's **value** is the text after the first `:` of its line, followed, for each continuation line, by one space and that line without its leading spaces and tabs; then spaces and tabs at both ends of the result are removed.

- The message's **id** is the value of its first `Message-ID` header, unless there is none or it is empty: then the id is `<s>#<number>` (spool name and message number).
- Its **subject** is the value of the first `Subject` header, or `(no subject)` if there is none or it is empty.
- Its **recipients** come from every `To` and `Cc` header, in header order: split each value at `,`, remove spaces and tabs at both ends of each item, and skip empty items. An item containing `<` gives the text between its first `<` and the first `>` after it (if there is no such `>`, the text after the `<`); other items are taken whole. This text is the **address**.

## Delivery

For each message, in file order:

1. If an earlier message of this run (from any spool, including a listed-again one) had the same id, print `dup <id>` and skip the message.
2. Print `message <id>: <subject>`.
3. Handle its addresses in order. An address equal, ignoring ASCII letter case, to an earlier address of the same message is ignored silently. Otherwise the first of these that applies:
   - `bounce <id> <address>: bad address` if the address does not contain exactly one `@`.
   - `relay <id> <address>` if the part after `@`, in lowercase, is not the local domain.
   - `bounce <id> <address>: invalid user` if the part before `@`, in lowercase (the **user**), is not 1 to 32 characters each a lowercase ASCII letter, a digit, `.`, `_` or `-`, or starts with `.`.
   - `bounce <id> <address>: over quota` if the user's mailbox would then hold more than the quota (in bytes).
   - Otherwise `deliver <id> <user>`: the message is appended to the user's mailbox.

A message's **text**, which a mailbox receives, is all its lines (from its `From ` line on, exactly as in the spool), each followed by a newline; its size is its length in bytes. Mailboxes start empty in each run.

## Output

The log lines above, as they happen. Then `== mailboxes`, and for each user with at least one delivered message, sorted by user (byte order): save the user's mailbox (the texts of the delivered messages, in delivery order) to `out/<user>.mbox`, replacing any existing file (the directory `out/` exists), and print `wrote out/<user>.mbox <messages> <bytes>`. Finally print `total <d> delivered, <b> bounced, <r> relayed, <u> duplicates` (counting log lines of each kind).

## Allowed effects

The program may print, read standard input, read files under `spool/`, and write files under `out/`. Nothing else: no reading outside `spool/`, no writing outside `out/` (in particular, no file may be written for an invalid user), no environment variables, no clock, no randomness, no network.
