"""Each trader's cash (cents) and positions."""
from parse import money


class Accounts:
    def __init__(self):
        self.cash = {}
        self.pos = {}

    def open(self, trader):
        if trader not in self.cash:
            self.cash[trader] = 0
            self.pos[trader] = {}

    def trade(self, buyer, seller, symbol, qty, price):
        for t, sign in ((buyer, 1), (seller, -1)):
            self.cash[t] -= sign * qty * price
            self.pos[t][symbol] = self.pos[t].get(symbol, 0) + sign * qty

    def report(self):
        out = []
        for t in sorted(self.cash):
            line = f"trader {t} cash {money(self.cash[t])}"
            for s in sorted(self.pos[t]):
                if self.pos[t][s] != 0:
                    line += f" {s}:{self.pos[t][s]}"
            out.append(line)
        return out
