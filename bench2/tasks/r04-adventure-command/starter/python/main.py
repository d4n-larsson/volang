import sys

from world import make_world
from player import Player
from commands import do_look, do_go, do_take, do_inventory, do_score, do_quit

# Number of words after the verb that each command takes.
ARITY = {"look": 0, "go": 1, "take": 1, "inventory": 0, "score": 0, "quit": 0}


def run(words, player, world):
    verb = words[0]
    args = words[1:]
    if verb not in ARITY:
        return ["unknown command " + verb]
    if len(args) != ARITY[verb]:
        return ["what?"]
    if verb == "look":
        return do_look(player, world)
    if verb == "go":
        return do_go(player, world, args[0])
    if verb == "take":
        return do_take(player, world, args[0])
    if verb == "inventory":
        return do_inventory(player)
    if verb == "score":
        return do_score(player)
    return do_quit(player)


def main():
    world = make_world()
    player = Player()
    for line in sys.stdin.read().split("\n"):
        words = [w for w in line.split(" ") if w != ""]
        if not words:
            continue
        for out in run(words, player, world):
            print(out)
        if words[0] == "quit" and len(words) == 1:
            break


main()
