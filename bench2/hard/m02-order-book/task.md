# Order book

Write a matching engine for a small stock exchange. Read commands from standard input, one per line, match buy and sell orders for several symbols, print every event, and at the end print each symbol's order book and each trader's cash and positions.

## Program structure

Split the program into at least three source files: command parsing and the formatting of prices and amounts, the order book (resting orders, matching, pending stop orders), and the accounts (each trader's cash and positions) each in their own file, plus the entry file that reads standard input, drives them and prints the final report.

## Input

Lines are numbered from 1; every line counts. A line is split into fields at runs of spaces; leading and trailing spaces are ignored; a line without fields is ignored. Field formats:

- *id*: 1 to 8 ASCII letters and digits. Ids are case-sensitive.
- *trader*: 1 to 10 lowercase letters `a`-`z`.
- *side*: `buy` or `sell`.
- *symbol*: 1 to 5 uppercase letters `A`-`Z`.
- *qty*: 1 to 6 digits, the first not `0` (so 1 to 999999).
- *price*: `D.CC`: `D` is 1 to 6 digits without a leading zero unless `D` is exactly `0`, a `.`, then exactly 2 digits `CC`. Its value is `D * 100 + CC` cents and must be greater than zero (`0.00` is not a price; `0.05`, `12.30` and `999999.99` are).
- *tif* (time in force): `gtc`, `ioc` or `fok`.

Commands (the first field is the command word):

| Form | Meaning |
|---|---|
| `limit <id> <trader> <side> <symbol> <qty> <price>` optionally followed by `<tif>` | a limit order; without a tif it is `gtc` |
| `market <id> <trader> <side> <symbol> <qty>` | a market order |
| `stop <id> <trader> <side> <symbol> <qty> <price>` | a stop order; the price is its *stop price* |
| `cancel <id>` | cancel a resting limit order or a pending stop order |
| `modify <id> <qty> <price>` | change a resting limit order |
| `book <symbol>` | print a symbol's book |

A line with an unknown command word, the wrong number of fields, or a field that does not have its format is invalid: print `invalid line <N>` and ignore it. Invalid lines never affect anything else (their ids are not used).

## Orders and the book

Each symbol has its own book with two sides: *bids* (resting buy orders) and *asks* (resting sell orders). Only `gtc` limit orders ever rest. A resting order has an id, trader, side, symbol, price, a *remaining* quantity, and a *time priority*: a counter that increases by one each time any order is placed on any book (the first order placed gets 1).

The *last price* of a symbol is the price of its most recent trade; a symbol without trades has none.

**Ids.** The ids of all earlier `limit`, `market` and `stop` lines that were not invalid are *used*, whether those orders were accepted or rejected. A `limit`, `market` or `stop` whose id is used is rejected with reason `duplicate id`.

**Rejection.** A rejected order prints `reject <id> <reason>` and changes nothing.

## Matching

An incoming order with quantity `q` (its *open* quantity) on side buy matches against asks, on side sell against bids. Repeat while the open quantity is greater than zero:

1. Find the *best* resting order on the opposite side that the incoming order *accepts*. For a buy the best is the lowest price, for a sell the highest price; among equal prices, the smallest time priority. A limit buy accepts asks priced at or below its limit price; a limit sell accepts bids priced at or above its limit price; a market order (and a triggered stop, below) accepts any price.
2. If there is none, matching ends.
3. If the best order belongs to the same trader as the incoming order (self-trade prevention), remove it from the book, print `stp <id> <remaining>` with the resting order's id and remaining quantity, and continue with step 1.
4. Otherwise trade `t = min(open quantity, resting remaining)` shares at the *resting* order's price. Print `trade <symbol> <t> <price> <buy id> <sell id>` (the id of the buy order first, whichever of the two was resting). Reduce both quantities by `t`; a resting order whose remaining quantity becomes zero is removed. The symbol's last price becomes the trade price. The buyer's position in the symbol increases by `t` and their cash decreases by `t * price` cents; the seller's position decreases by `t` and their cash increases by `t * price`.

After matching, the incoming order ends with exactly one of these lines:

- open quantity zero: `filled <id>`;
- a `gtc` limit order: it is placed on its book with its open quantity as remaining quantity, gets the next time priority, and prints `rest <id> <remaining> <price>`;
- otherwise (`ioc` limit, market order, triggered stop): the rest is dropped: `expired <id> <open quantity>`.

## Commands

**limit.** Checks, in this order: `duplicate id`; then, if the symbol has a last price `L`, the limit price `P` must satisfy `10 * P >= 9 * L` and `10 * P <= 11 * L`, else reject `price band`; then, for `fok` only, the sum of the remaining quantities of the opposite-side resting orders that the order accepts and that belong to *other* traders must be at least the order's quantity, else reject `not filled`. An accepted order is matched as above.

**market.** Checks: `duplicate id`; then the opposite side must hold at least one resting order of another trader, else reject `no liquidity`. An accepted order is matched as above.

**stop.** Check: `duplicate id`. An accepted stop order is not matched: it becomes *pending* and prints `pending <id>`. A pending buy stop *triggers* when its symbol's last price is at or above its stop price; a pending sell stop when the last price is at or below its stop price.

**cancel.** If the id is a resting limit order or a pending stop order, remove it and print `cancelled <id> <remaining>` (for a stop, its quantity). Otherwise print `reject <id> unknown order`.

**modify.** The id must be a resting limit order, else `reject <id> unknown order` (also for a pending stop). Then, if the new price differs from the order's price and the symbol has a last price, the new price must pass the price band check above, else `reject <id> price band` and the order stays as it was. Otherwise print `modified <id> <qty> <price>` with the new values, and:

- if the price is unchanged and the new quantity is at most the remaining quantity, the remaining quantity becomes the new quantity and the order keeps its place and time priority;
- otherwise the order is removed from the book and enters again as an incoming `gtc` limit order with the same id, trader, side and symbol and the new quantity and price: it is matched and ends with `filled` or `rest` as above (and gets a new time priority if it rests).

**book.** Print the symbol's book (format under Output). Any well-formed symbol is allowed, also one that never had an order.

## Stop triggers

After each accepted `limit`, `market`, `stop` or `modify` command (and not after rejected ones, `cancel` or `book`), check the pending stops of that command's symbol. Repeat: among the pending stops of that symbol whose trigger condition holds with the current last price, take the one accepted earliest; if there is none, stop. Otherwise remove it from the pending stops, print `trigger <id>`, and match it like a market order of its trader, side and quantity, without the `no liquidity` check (it ends with `filled` or `expired`). Its trades can change the last price, which can trigger further stops. A symbol without a last price triggers nothing.

## Output

Every line is printed as the event happens, and every output line ends with a newline. Prices and amounts in cents are printed as `D.CC`: the absolute value divided by 100, a `.`, and the remainder as exactly two digits, with a leading `-` for a negative amount (`-1015.00`, `0.05`, `0.00`).

A book prints `book <symbol> last <price>` (or `book <symbol> last -` without a last price), then one line per price level: the asks from the highest price to the lowest, then the bids from the highest price to the lowest, each as `  ask <price> <qty> <count>` or `  bid <price> <qty> <count>` (two leading spaces), where `qty` is the sum of the remaining quantities of the resting orders at that price on that side and `count` their number. A book without resting orders prints `  empty` instead of the levels. Pending stops are not shown.

After the last input line, print:

1. The book of every symbol that appeared in an accepted `limit`, `market` or `stop` order, in byte order of the symbols.
2. For every trader who placed at least one accepted `limit`, `market` or `stop` order, in byte order: `trader <name> cash <amount>` followed by ` <symbol>:<position>` for each symbol in which the trader's position is not zero, in byte order of the symbols (positions are plain integers such as `10` or `-5`).
