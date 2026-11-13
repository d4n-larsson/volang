from player import move, pick_up


def describe(world, room_id):
    room = world[room_id]
    exits = sorted(room.exits)
    items = "none"
    if room.items:
        items = ", ".join(room.items)
    return ["== " + room.title + " ==", "exits: " + " ".join(exits), "items: " + items]


def do_look(player, world):
    return describe(world, player.room)


def do_go(player, world, direction):
    if not move(player, world, direction):
        return ["you can't go " + direction]
    return describe(world, player.room)


def do_take(player, world, item):
    points = pick_up(player, world, item)
    if points < 0:
        return ["no " + item + " here"]
    if points > 0:
        return ["taken " + item + " (+" + str(points) + ")"]
    return ["taken " + item]


def do_inventory(player):
    if not player.inventory:
        return ["you carry nothing"]
    return ["you carry: " + ", ".join(player.inventory)]


def score_text(player):
    return "score " + str(player.score) + " in " + str(player.moves) + " moves"


def do_score(player):
    return [score_text(player)]


def do_quit(player):
    return ["final " + score_text(player)]
