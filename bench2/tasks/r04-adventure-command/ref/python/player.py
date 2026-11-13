from world import START_ROOM, VISIT_POINTS, CARRY_LIMIT, item_value, item_weight

NOT_HERE = -1
TOO_HEAVY = -2


class Player:
    def __init__(self):
        self.room = START_ROOM
        self.inventory = []
        self.score = 0
        self.moves = 0
        self.visited = {START_ROOM}
        # every item ever picked up (points are only earned the first time)
        self.taken = set()


def move(player, world, direction):
    """Moves through an exit of the current room; False if there is none."""
    room = world[player.room]
    if direction not in room.exits:
        return False
    player.room = room.exits[direction]
    player.moves += 1
    if player.room not in player.visited:
        player.visited.add(player.room)
        player.score += VISIT_POINTS
    return True


def pick_up(player, world, item):
    """Moves an item from the current room to the inventory; returns the points earned, NOT_HERE or TOO_HEAVY."""
    room = world[player.room]
    if item not in room.items:
        return NOT_HERE
    if carried_weight(player) + item_weight(item) > CARRY_LIMIT:
        return TOO_HEAVY
    room.items.remove(item)
    player.inventory.append(item)
    if item in player.taken:
        return 0
    player.taken.add(item)
    points = item_value(item)
    player.score += points
    return points


def carried_weight(player):
    return sum(item_weight(i) for i in player.inventory)


def put_down(player, world, item):
    """Moves an item from the inventory to the current room; False if it is not carried."""
    if item not in player.inventory:
        return False
    player.inventory.remove(item)
    world[player.room].items.append(item)
    return True
