from world import START_ROOM, VISIT_POINTS, item_value


class Player:
    def __init__(self):
        self.room = START_ROOM
        self.inventory = []
        self.score = 0
        self.moves = 0
        self.visited = {START_ROOM}


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
    """Moves an item from the current room to the inventory; returns the points earned, or -1."""
    room = world[player.room]
    if item not in room.items:
        return -1
    room.items.remove(item)
    player.inventory.append(item)
    points = item_value(item)
    player.score += points
    return points
