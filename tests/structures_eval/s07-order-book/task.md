A limit order book. Each non-blank line of stdin is `buy PRICE QTY` or `sell PRICE QTY` (1 <= PRICE <= 1000000, 1 <= QTY <= 1000000). The book holds resting quantity per price level, on two sides: bids (from buy orders) and asks (from sell orders). It starts empty.

Process the orders in order:

- `buy PRICE QTY`: while QTY > 0 and the lowest ask price A is <= PRICE, trade min(QTY, quantity at A) at price A: that quantity leaves the ask level A (the level disappears when it reaches 0) and QTY. Then any QTY left rests as a bid at PRICE (added to that bid level).
- `sell PRICE QTY`: the same against the highest bid price B >= PRICE, trading at price B; any QTY left rests as an ask at PRICE.

After each order print one line `TRADED COST BID ASK`: TRADED is the total quantity this order traded, COST the sum of price * quantity over its trades, BID the highest bid price and ASK the lowest ask price after the order, each `-` when that side is empty.

Example: for

```
buy 100 5
buy 101 3
sell 103 4
sell 100 7
buy 104 10
```

the output is

```
0 0 100 -
0 0 101 -
0 0 101 103
7 703 100 103
4 412 104 -
```

(The `sell 100 7` trades 3 at 101 and 4 at 100, leaving 1 bid at 100. The `buy 104 10` trades 4 at 103, and 6 rest as a bid at 104.)

Effects: `console`, `input`.

The largest inputs have 100,000 orders and tens of thousands of price levels on each side: scanning all levels to find the best price is too slow.
