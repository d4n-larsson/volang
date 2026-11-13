START_ROOM = "hall"
VISIT_POINTS = 5
CARRY_LIMIT = 10

ITEM_WEIGHTS = {
    "lamp": 2,
    "book": 3,
    "key": 1,
    "knife": 2,
    "bread": 1,
    "coin": 1,
    "anvil": 9,
    "shovel": 4,
    "statue": 8,
    "crown": 3,
}

# Points for picking up an item; items not listed are worth nothing.
ITEM_VALUES = {
    "book": 5,
    "bread": 2,
    "coin": 10,
    "statue": 20,
    "crown": 50,
}


class Room:
    def __init__(self, title, exits, items):
        self.title = title
        self.exits = exits
        self.items = items


def make_world():
    return {
        "hall": Room("Hall", {"north": "library", "east": "kitchen", "south": "garden"}, ["lamp"]),
        "library": Room("Library", {"south": "hall", "up": "attic"}, ["book", "key"]),
        "kitchen": Room("Kitchen", {"west": "hall", "down": "cellar"}, ["knife", "bread"]),
        "cellar": Room("Cellar", {"up": "kitchen"}, ["coin", "anvil"]),
        "garden": Room("Garden", {"north": "hall"}, ["shovel", "statue"]),
        "attic": Room("Attic", {"down": "library"}, ["crown"]),
    }


def item_value(item):
    return ITEM_VALUES.get(item, 0)


def item_weight(item):
    return ITEM_WEIGHTS[item]
