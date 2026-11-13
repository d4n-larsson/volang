# Hotel bookings: add cancellation

## The existing program

The program manages a small hotel. It reads commands from standard input, one per line, and prints the response to each command. Words on a line are separated by one or more spaces; blank lines are ignored. A *number* is a non-empty string of the digits `0`-`9`. Money is held in integer cents and printed as `<cents / 100>.<cents % 100 as two digits>` (for example `1250` prints `12.50`, `5` prints `0.05`). Days are numbers. A booking from day FROM to day TO checks in on day FROM and checks out on day TO: it uses the nights FROM, FROM+1, ..., TO-1 (TO - FROM nights), and the room is free again on the night of day TO. `NAME`, `TYPE`, `ID` and `GUEST` are single words.

Commands:

- `type NAME RATE COUNT` (RATE in cents and COUNT are numbers): adds a room type with COUNT rooms at a nightly rate of RATE. Prints `type NAME added`, or `error: type NAME exists` (nothing changes).
- `rate NAME RATE`: changes the type's nightly rate to RATE and prints `type NAME rate R` (R as money). Unknown type: `error: unknown type NAME`. Existing bookings keep the rate they were made with.
- `book ID TYPE GUEST FROM TO` (FROM and TO are numbers): checks, in this order: the ID is not already used by a booking — else `error: booking ID exists`; the type exists — else `error: unknown type TYPE`; FROM is less than TO — else `error: bad dates`; on every night of the stay fewer than COUNT rooms of the type are taken — else `error: no TYPE room free on day D` with D the first such night. On an error nothing changes. Otherwise, with N = TO - FROM nights and the type's current rate, the subtotal is N × rate, the long-stay discount is subtotal × 10 / 100 (integer division, rounded down) when N is at least 7 and 0 otherwise, and the total is subtotal − discount. Prints `booked ID: TYPE N nights, total T`.
- `avail TYPE FROM TO` (FROM and TO numbers): `error: unknown type TYPE` if the type does not exist, then `error: bad dates` unless FROM < TO. Prints `avail TYPE FROM-TO: K of C free`, where C is the type's room count and K the smallest number of free rooms (C minus rooms taken) over the nights FROM to TO-1.
- `occupancy DAY` (a number): prints `day DAY: X of Y rooms occupied`, where Y is the total room count of all types and X the number of rooms taken on the night of DAY, then one line per type in ascending byte order of the name, `  TYPE X/C` (rooms of that type taken on that night / its count).
- `invoice ID`: prints `invoice ID for GUEST`, then `  TYPE N nights x RATE = SUBTOTAL` (RATE the booking's rate), then `  long stay discount D` only when the discount is not zero, then `  total T` (all amounts as money). Unknown ID: `error: unknown booking ID`.
- `report`: one line per room type in ascending byte order of the name, `TYPE: B bookings, N nights, revenue R`, where B counts the type's bookings, N the sum of their nights and R the sum of their totals (B may be 0 or 1; the words stay `bookings` and `nights`). Then `total revenue R` with R the sum over all types.

A command with the wrong number of arguments, or with a non-number where a number is required, prints `error: bad command` (checked before anything else). A first word that is not a command prints `error: unknown command WORD`.

## The change

Add a command `cancel ID DAY` (DAY a number: the day the guest cancels). It prints `error: bad command` with the wrong number of arguments or a non-number DAY. Otherwise it checks, in this order: the booking exists — else `error: unknown booking ID`; it is not already cancelled — else `error: booking ID already cancelled`; DAY is less than the booking's TO — else `error: booking ID already ended`. On an error nothing changes.

Otherwise the booking becomes cancelled (its ID stays used: `book` with the same ID still says `error: booking ID exists`), and:

- **Refund** (the guest paid the booking's total): if FROM − DAY is at least 7, the refund is the whole total; else if DAY is less than FROM (1 to 6 days before check-in), the refund is total / 2 rounded down to whole cents; else (cancelled on or after the check-in day) the refund is 0. Prints `cancelled ID: refund R` (R as money, `0.00` when there is no refund).
- **Rooms**: the booking keeps only the nights before the cancellation that it already used: from now on it takes a room on the nights FROM to max(FROM, DAY) − 1 (no night at all when DAY ≤ FROM). All the later nights are free again for `book`, `avail` and `occupancy`. For example, a booking 10–20 cancelled on day 15 still takes the nights 10 to 14; cancelled on day 10 or earlier, it takes none.
- **Invoice**: the invoice of a cancelled booking prints the same lines as before (nights, subtotal, discount and total of the original booking), followed by `  cancelled on day DAY, refund R` and `  net N`, where N = total − refund.
- **Report**: for each type, B now counts only the bookings that are not cancelled; when the type has C ≥ 1 cancelled bookings, the line reads `TYPE: B bookings (C cancelled), N nights, revenue R`. N is the number of nights the type's bookings still take (all nights of an active booking, the kept nights of a cancelled one). R is the sum of the net amounts (total − refund) of all the type's bookings, cancelled or not. The last line is `total revenue R, refunds F` (F the sum of all refunds, possibly `0.00`) when at least one booking has been cancelled, and `total revenue R` as before otherwise.

Everything else stays exactly as it is.
