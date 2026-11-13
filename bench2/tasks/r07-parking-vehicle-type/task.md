# Parking garage: vehicle types

## The existing program

The program runs a parking garage. It reads commands from standard input, one per line, and prints the response to each command. Words on a line are separated by one or more spaces; blank lines are ignored. A *number* is a non-empty string of the digits `0`-`9`; inputs keep every number below 1000000. A TIME is a number of minutes since the garage started (commands need not come in time order).

Money is held in integer cents and printed as `<cents / 100>.<cents % 100 as two digits>` (`250` prints `2.50`, `5` prints `0.05`, `0` prints `0.00`).

**Fee.** A stay of D minutes (D = leaving TIME − entry TIME) costs, with the rates below: 0 if D ≤ FREE; otherwise `days * CAP + min(CAP, hours * HOURLY)` where `days = D / 1440` and `hours = (D % 1440 + 59) / 60` (integer division, so each started hour of the last partial day counts, and a partial day never costs more than CAP). Once a stay is longer than FREE, its whole duration is billed. The rates are FREE = 30 minutes, HOURLY = 250 cents, CAP = 2000 cents.

Commands (`PLATE` is a single word):

- `level L SPOTS` (L a number of at least 1, SPOTS a number from 1 to 99): opens level L with spots numbered 1 to SPOTS. Prints `level L opened with SPOTS spots`, or `error: level L exists` (nothing changes). Levels can be opened in any order.
- `enter PLATE TIME`: parks the vehicle in the free spot with the lowest level number, and on that level the lowest spot number. Prints `PLATE parked at L-S` (level L, spot S). If the plate is already parked: `error: PLATE already parked`; else if no spot is free: `error: garage full`.
- `exit PLATE TIME`: if the plate is not parked, `error: PLATE not parked`; else if TIME is before its entry TIME, `error: time before entry`. Otherwise the vehicle leaves, its spot becomes free, its level's exit count rises by 1 and its level's revenue by the fee, and the program prints `PLATE left L-S after D min, fee F`.
- `quote PLATE TIME`: the same two errors as `exit`; otherwise prints `PLATE owes F after D min` (the fee if it left at TIME) and changes nothing.
- `occupancy`: one line per level in ascending level order, `level L: U/N used` (U occupied spots of N), then `total U/N used` over all levels (`total 0/0 used` without levels).
- `parked`: one line per parked vehicle, by level number then spot number, `L-S PLATE since TIME` (its entry TIME); `no vehicles` when none is parked.
- `report TIME`: one line per level in ascending level order, `level L: exits N revenue R pending P`, where N and R are the level's exit count and revenue so far and P is the sum of the fees the vehicles parked on the level would owe if they left at TIME (a vehicle whose entry TIME is after TIME adds 0). Then `total exits N revenue R pending P` with the sums over all levels.
- `rates`: prints `car: first 30 min free, 2.50 per hour, 20.00 per day`.

A command with the wrong number of arguments, or with a value that is not as described, prints `error: bad command` and changes nothing. A first word that is not a command prints `error: unknown command WORD`.

## The change

The garage now takes motorcycles and trucks. The fee function gets a vehicle-type parameter, and so does everything that computes a fee (`exit`, `quote`, `report`).

- `enter PLATE TIME [TYPE]`: TYPE is optional and is one of `car` (the default when it is left out), `motorcycle` or `truck`. With fewer than 3 or more than 4 words on the line it is still `error: bad command` (and so is a TIME that is not a number). Then the checks, in this order: a TYPE that is not one of the three prints `error: unknown vehicle type TYPE`; then `error: PLATE already parked`; then finding a spot.
- Spots: cars and motorcycles use any level, exactly as before (`error: garage full` when nothing is free). Trucks may only park on level 1 (the level numbered 1): the lowest free spot there; if level 1 does not exist or has no free spot the program prints `error: no truck spot`.
- Each vehicle remembers its type. Rates per type (same fee formula):

  | type | FREE | HOURLY | CAP |
  |---|---|---|---|
  | car | 30 | 250 | 2000 |
  | motorcycle | 60 | 100 | 800 |
  | truck | 0 | 600 | 4500 |

  (A truck pays from its first minute: only a stay of 0 minutes is free.) `exit`, `quote` and `report`'s pending sums use each vehicle's own type.
- `parked`: lines for cars are unchanged; for a motorcycle or truck the line gets ` (TYPE)` appended: `2-1 BIKE since 10 (motorcycle)`.
- `rates` prints three lines:

  ```
  car: first 30 min free, 2.50 per hour, 20.00 per day
  motorcycle: first 60 min free, 1.00 per hour, 8.00 per day
  truck: no free period, 6.00 per hour, 45.00 per day, level 1 only
  ```

Everything else is unchanged. In particular, for input in which no `enter` gives a TYPE (all vehicles are cars) and that does not use `rates`, the output must be byte-identical to the existing program's.
