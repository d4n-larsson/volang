# Text adventure: add `drop` and a carry weight limit

## The existing program

The program is a small text adventure. It reads commands from standard input, one per line, and prints the response to each. Words on a line are separated by one or more spaces; blank lines (including lines of only spaces) are ignored. Commands and names are case-sensitive.

### The world

The world is built into the program. The player starts in the hall, which counts as visited. Each room's items are listed in their initial order.

| Room id | Title | Exits (direction → room) | Items |
|---|---|---|---|
| hall | Hall | north → library, east → kitchen, south → garden | lamp |
| library | Library | south → hall, up → attic | book, key |
| kitchen | Kitchen | west → hall, down → cellar | knife, bread |
| cellar | Cellar | up → kitchen | coin, anvil |
| garden | Garden | north → hall | shovel, statue |
| attic | Attic | down → library | crown |

Item values (points for picking the item up): book 5, bread 2, coin 10, statue 20, crown 50; every other item is worth 0.

The player has a current room, an inventory (a list, in the order items were picked up), a score (starts at 0) and a move count (starts at 0).

### Commands

- `look`: describes the current room in three lines:
  ```
  == TITLE ==
  exits: DIR DIR ...
  items: ITEM, ITEM, ...
  ```
  The exit directions are sorted alphabetically (byte order) and separated by single spaces. The items are the room's current items in order, separated by `, `; with no items the line is `items: none`.
- `go DIR`: if the current room has an exit DIR, the player moves there and the move count rises by 1; if the new room was not visited before, it becomes visited and the score rises by 5. Then the new room is described exactly as `look` does. If there is no such exit: `you can't go DIR` (nothing changes).
- `take ITEM`: if ITEM is in the current room, it is removed from the room and appended to the inventory, and the score rises by the item's value. Prints `taken ITEM (+V)` when the value V is not 0, else `taken ITEM`. If the item is not in the current room: `no ITEM here`.
- `inventory`: `you carry: ITEM, ITEM, ...` (inventory order), or `you carry nothing`.
- `score`: `score S in M moves`.
- `quit`: prints `final score S in M moves` and stops: every later line is ignored.

A known command with the wrong number of words after it (`look`, `inventory`, `score` and `quit` take none; `go` and `take` take exactly one) prints `what?` and changes nothing (a `quit` with extra words does not stop the game). An unknown first word prints `unknown command WORD`.

## The change

Add a `drop` command and a limit on what the player can carry.

- Every item has a weight: lamp 2, book 3, key 1, knife 2, bread 1, coin 1, anvil 9, shovel 4, statue 8, crown 3. The player can carry at most a total weight of 10.
- `take ITEM`: if the item is not in the current room, still `no ITEM here`. Otherwise, if the total weight of the inventory plus the item's weight is more than 10, print `ITEM is too heavy` and change nothing. Otherwise the item is taken as before.
- An item's value is added to the score only the first time that item is ever taken. Taking it again later (after dropping it) prints `taken ITEM` without points and does not change the score.
- New command `drop ITEM` (exactly one word after it, else `what?`): if ITEM is in the inventory, it is removed from the inventory (the other items keep their order), appended to the end of the current room's items, and `dropped ITEM` is printed. Otherwise `you don't have ITEM` (nothing changes). Dropping never changes the score or the move count.

Everything else (every other command, message, the world, scores for visiting rooms) stays exactly as it is.
