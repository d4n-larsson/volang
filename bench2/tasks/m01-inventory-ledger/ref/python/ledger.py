class Ledger:
    def __init__(self):
        self.lots = {}  # sku -> list of [qty, cost]
        self.revenue = self.cogs = self.loss = 0

    def stock(self, sku):
        return sum(q for q, _ in self.lots.get(sku, []))

    def receive(self, sku, qty, cost):
        self.lots.setdefault(sku, []).append([qty, cost])

    def take(self, sku, qty):
        """Removes qty units FIFO and returns their cost; caller checks stock first."""
        lots, cost = self.lots[sku], 0
        while qty > 0:
            n = min(qty, lots[0][0])
            cost += n * lots[0][1]
            lots[0][0] -= n
            qty -= n
            if lots[0][0] == 0:
                lots.pop(0)
        return cost

    def ship(self, sku, qty, price):
        self.cogs += self.take(sku, qty)
        self.revenue += qty * price

    def scrap(self, sku, qty):
        self.loss += self.take(sku, qty)
