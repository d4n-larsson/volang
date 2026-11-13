# Recipe scaler: configurable rounding and units

## The existing program

The program scales recipes and builds shopping lists. It reads commands from standard input, one per line, and prints the response to each command. Words on a line are separated by one or more spaces; blank lines are ignored. A *count* is a non-empty string of the digits `0`-`9` whose value is at least 1. `NAME` and `ITEM` are single words. A *unit* is one of `g`, `ml`, `pcs`; every quantity is an integer number of its unit.

**Printing a quantity** Q of a unit: for `g` with Q ≥ 1000, print Q/1000 in kilograms as `W kg` when Q is a multiple of 1000, else `W.D kg` where W = Q / 1000 (integer division) and D is Q % 1000 written with three digits (leading zeros added) and then its trailing zeros removed (`1005` → `1.005 kg`, `1250` → `1.25 kg`, `1200` → `1.2 kg`, `2000` → `2 kg`). `ml` with Q ≥ 1000 is printed the same way with `l` (`2050` → `2.05 l`). Anything else prints as `Q UNIT` (`999 g`, `0 ml`, `1 pcs`, `12 pcs`).

**Scaling** a quantity Q of a recipe for S servings to N servings gives Q × N / S rounded **up** to an integer (exact results are kept).

Commands:

- `recipe NAME SERVINGS ITEM QTY UNIT [ITEM QTY UNIT ...]`: SERVINGS and every QTY are counts. Adds a recipe with its ingredients in the given order. Checks, in this order: every UNIT is a unit — else `error: bad unit U` with the first bad one; no recipe NAME exists — else `error: recipe NAME exists`; no ITEM appears twice in the recipe — else `error: duplicate ITEM` with the first item that repeats. Prints `recipe NAME added (K ingredients)` (K may be 1; the word stays `ingredients`).
- `show NAME`: prints `NAME (S servings)`, then one line per ingredient in recipe order, `  ITEM QUANTITY` (two leading spaces, the quantity printed as above).
- `scale NAME N` (N a count): prints `NAME for N:`, then per ingredient in recipe order `  ITEM QUANTITY` with the scaled quantity.
- `shop NAME N [NAME N ...]` (each N a count; a recipe may appear several times): every NAME must be a recipe, else `error: unknown recipe NAME` for the first unknown one in the line. Each recipe is scaled to its N (each ingredient rounded as in `scale`), then the scaled quantities are added up per (ITEM, unit) pair. Prints `shopping list (K recipes):` where K is the number of NAME N pairs, then one line `  ITEM QUANTITY` per (ITEM, unit) pair, sorted by ITEM then by unit (byte order); the same item in two units gives two lines.
- `list`: one line per recipe in ascending byte order of the name, `NAME: S servings, K ingredients`; `no recipes` if there are none.

`show` and `scale` print `error: unknown recipe NAME` for an unknown recipe. A command with the wrong number of arguments (for `recipe`: fewer than 5, or not 2 + a multiple of 3; for `shop`: zero, or an odd number), or with a non-count where a count is required, prints `error: bad command` (checked before anything else). A first word that is not a command prints `error: unknown command WORD`.

## The change

Make rounding and the display of volumes configurable by a new, **optional** first input line:

```
config [KEY=VALUE ...]
```

It is recognised only when the very first line of the input (before any other line, blank or not) has `config` as its first word. Each following word must be `KEY=VALUE` with exactly one `=`, where KEY is `rounding` (VALUE `up`, `down` or `nearest`) or `units` (VALUE `metric` or `kitchen`), each KEY at most once, in any order; a missing key keeps its default (`rounding=up`, `units=metric`), so `config` alone is valid. On a valid line the program prints `config rounding=R units=U` with the values in effect. On any other config line (unknown key, bad value, repeated key, a word without exactly one `=`) it prints `error: bad config` and uses the defaults for the whole run. Either way the config line is not a command. A `config` line anywhere else (including after a blank first line) is still an unknown command (`error: unknown command config`). Without a config line, the program behaves exactly as before.

The settings apply to every command:

- **rounding** is used everywhere a quantity is scaled (`scale`, and each recipe's scaling inside `shop`, before the sums): `up` rounds Q × N / S up (as before), `down` rounds it down, `nearest` rounds it to the nearest integer, and when it lies exactly halfway between two integers (the remainder of Q × N divided by S is exactly S / 2), to the **even** one of the two (1.5 → 2, 2.5 → 2, 0.5 → 0). A result may be 0, and is printed as `0 g`, `0 ml`, `0 pcs`.
- **units** changes how `ml` quantities are printed, everywhere quantities are printed (`show`, `scale`, `shop`): `metric` is the existing format. With `kitchen`, Q ml is split greedily into cups (240 ml), tablespoons (15 ml), teaspoons (5 ml) and the remaining ml: take as many cups as fit, then from the rest as many tablespoons, then teaspoons, then ml; print the non-zero parts in that order, each as `COUNT NAME` with NAME `cup`, `tbsp`, `tsp`, `ml` (never pluralised), joined by single spaces (`500` → `2 cup 1 tbsp 1 tsp`, `239` → `15 tbsp 2 tsp 4 ml`, `1000` → `4 cup 2 tbsp 2 tsp`, `0` → `0 ml`). Quantities are never shown in `l` with `kitchen`. `g` and `pcs` print as before in both settings. In `shop`, the sums are still made in ml and only the printing changes.

All other commands, messages and checks are unchanged.
