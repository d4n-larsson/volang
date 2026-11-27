# Chat server: channel message stores, threads and read markers

## The existing program

The program simulates a chat server. It reads commands from standard input, one per line, and prints the response to each command. Words on a line are separated by one or more spaces; blank lines are ignored. Every other line is one command and moves the logical clock on by one: the first command runs at time 1, the second at time 2, and so on (commands that fail count too).

A NAME (user or channel) is 1 to 16 characters from `a`-`z` and `0`-`9`. A *number* is 1 to 6 digits. TEXT is all remaining words of the line (at least one), joined by single spaces. Channels are printed as `#NAME`.

Messages are kept in one global list with global IDs 1, 2, 3, ... in posting order (IDs are never reused). A message line is `[ID] t=TIME AUTHOR: TEXT`, followed by ` (edited)` if the message was edited. A message *mentions* user U when one of its space-separated words, after removing any trailing characters from `.,:;!?`, is exactly `@U`.

Commands:

- `user NAME`: `user NAME created`, or `error: user NAME exists`.
- `channel USER NAME [private]`: creates a channel whose creator is USER; the creator becomes its first member. Prints `channel #NAME created by USER`, with ` (private)` appended for a private channel.
- `join USER CHANNEL`: `USER joined #CHANNEL`. A private channel can only be joined by users invited to it (the creator counts as invited).
- `invite USER CHANNEL GUEST`: USER, a member of the private CHANNEL, invites GUEST: `USER invited GUEST to #CHANNEL`. Invitations stay valid.
- `leave USER CHANNEL`: `USER left #CHANNEL`.
- `rename USER CHANNEL NEW`: only the creator may rename a channel: `#CHANNEL renamed to #NEW`. Its messages stay with it.
- `post USER CHANNEL TEXT`: a member posts a message: `message ID posted to #CHANNEL`.
- `edit USER ID TEXT`: the author replaces the text and the message becomes edited: `message ID edited`.
- `delete USER ID`: the author, or the creator of the message's channel, deletes it (it disappears): `message ID deleted`.
- `history CHANNEL [N]`: the channel's messages, oldest first, under a header `#CHANNEL: K messages` (`1 message`). With N, when the channel has more than N messages, only the last N are shown and the header is `#CHANNEL: last N of K messages`. A channel without messages prints `#CHANNEL has no messages`.
- `search CHANNEL WORD`: `search #CHANNEL for WORD: K found`, then each message of the channel whose text contains WORD (as a substring), oldest first, as two spaces and its message line.
- `mentions USER`: `mentions of USER: K`, then every message that mentions USER in a channel USER is currently a member of, in posting order, as `  #CHANNEL ` followed by its message line.
- `members CHANNEL`: `#CHANNEL created by CREATOR` (plus ` (private)`), then `members: ` and the member names in ascending order joined by `, ` (`none` without members).
- `whois USER`: `USER: member of #A, #B; K messages` (channels in ascending order; `member of no channels`), K counting the user's messages.
- `top [N]` (N at least 1, default 3): the N authors with the most messages, most first, ties by ascending name: `1. NAME K messages`, `2. ...`; `no messages` when there are none.
- `channels`: one line per channel in ascending name order, `#NAME: M members, K messages` (` (private)` right after the name); `no channels` when there are none.
- `stats`: `users U, channels C, messages M, edits E, deletes D` (E and D count successful edits and deletes).

Names are compared and sorted by byte order. The checks of each command and their order are as in the code: first the number of arguments and the form of every argument (anything wrong prints `error: bad command`), then errors such as `error: unknown user U`, `error: unknown channel #C`, `error: U not in #C`, `error: unknown message ID`, `error: U cannot edit message ID`. A first word that is not a command prints `error: unknown command WORD`. A command that fails changes nothing.

## The change

### 1. Messages are kept per channel

Each channel keeps its own messages, numbered 1, 2, 3, ... within the channel, in posting order (top-level messages and replies share one sequence; numbers are never reused, also after a delete or a rename). Wherever the program prints a message's ID (`message N posted to #C`, message lines in `history`, `search`, `mentions` and `thread`), it now prints this number.

Commands that name one message take an *address* `CHANNEL/N` instead of an ID: CHANNEL a NAME, then `/`, then a number N of at least 1 (for example `general/3`). Anything else in its place is `error: bad command`. An address is resolved by first looking up the channel (`error: unknown channel #CHANNEL`), then the message (`error: unknown message CHANNEL/N`). In every message that mentions an address, it is printed as the channel's name, `/`, and N as a plain integer without leading zeros: `edit` prints `message CHANNEL/N edited`, `delete` prints `message CHANNEL/N deleted`, and the errors read `error: USER cannot edit message CHANNEL/N` and `error: USER cannot delete message CHANNEL/N`. The checks keep their order: `edit USER CHANNEL/N TEXT` checks the user, then the address, then that USER is the author; `delete USER CHANNEL/N` the user, then the address, then the permission. After a rename, a channel's messages are addressed by its new name only.

### 2. Threads

- `reply USER CHANNEL/N TEXT`: posts a reply in the thread of message N. Checks, in this order: the arguments (`error: bad command`), the user, the address, that USER is a member of the channel (`error: USER not in #CHANNEL`). The reply gets the next number of the channel. If message N is itself a reply, the new reply goes into the same thread as N (threads are one level deep): its *thread parent* is N's thread parent. Prints `message M posted to #CHANNEL in thread P`, M the new number and P the thread parent's number.
- `history` shows only top-level messages (not replies). K in the header and N in `last N of K` count these top-level entries. A top-level entry with R ≥ 1 replies ends with ` (R replies)`, or ` (1 reply)`, after ` (edited)` if present.
- `thread CHANNEL/N`: `thread #CHANNEL/P: R replies` (`1 reply`, `0 replies`), P the thread: N itself if it is top-level, else N's thread parent. Then P's message line (without a reply count), then each reply of P in number order as two spaces and its message line. Errors: `error: bad command`, then those of the address.
- `delete` of a reply, or of a top-level message without replies, removes it as before. Deleting a top-level message that has replies leaves a *deleted parent* in its place: in `history` and `thread` it shows as `[N] t=TIME [deleted]` (TIME its original posting time), followed in `history` by its reply count. When the last reply of a deleted parent is deleted, the deleted parent disappears as well. A deleted parent counts as deleted everywhere: `edit`, `delete` and `reply` on its address print `error: unknown message CHANNEL/N` (but a reply to one of its replies is allowed and joins its thread), it never matches `search` or `mentions`, and no count includes it, except that `thread` accepts its address and `history` lists it (and counts it in K).
- A reply counts as a message of its channel and its author everywhere: `search` (in number order) and `mentions` (in posting order) include replies, and their line then ends with ` (in thread P)` (after ` (edited)` if present); `top`, `whois`, `channels` and `stats` count replies.

### 3. Read markers

Each member of a channel has a *read marker*, a clock value:

- Creating a channel sets the creator's marker, and `join` sets the joining user's marker, to the time of that command (so nothing posted before is unread). `leave` discards the marker; joining again starts a new one.
- `post` and `reply` set the poster's marker in that channel to the time of the command.
- `read USER CHANNEL`: checks the arguments, the user, the channel, and that USER is a member (`error: USER not in #CHANNEL`); sets USER's marker to the time of this command and prints `USER read #CHANNEL`.
- No other command changes a marker (`edit` does not change a message's posting time; `rename` keeps the markers).
- `unread USER`: checks the arguments and the user. A message is *unread* for USER in a channel when it is not deleted (nor a deleted parent), its author is not USER, and its posting time is greater than USER's marker there; replies count like any message. For each channel USER is a member of, in ascending name order, that has K ≥ 1 unread messages, prints `#CHANNEL: K unread`, followed by ` (M mentioning you)` when M ≥ 1 of those K messages mention USER. If no channel has unread messages, prints `no unread messages for USER`.

Everything else stays exactly as it is.
