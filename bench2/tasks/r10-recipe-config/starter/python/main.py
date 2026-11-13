import sys

from recipes import Ingredient, add_recipe, show_lines, list_lines
from scaling import scale_lines, shopping_list


def is_count(s):
    """A number of at least 1."""
    return len(s) > 0 and all("0" <= c <= "9" for c in s) and int(s) > 0


def run(words, recipes):
    cmd = words[0]
    args = words[1:]
    if cmd == "recipe":
        if len(args) < 5 or (len(args) - 2) % 3 != 0 or not is_count(args[1]):
            return ["error: bad command"]
        ingredients = []
        for i in range(2, len(args), 3):
            if not is_count(args[i + 1]):
                return ["error: bad command"]
            ingredients.append(Ingredient(args[i], int(args[i + 1]), args[i + 2]))
        return [add_recipe(recipes, args[0], int(args[1]), ingredients)]
    if cmd == "show":
        if len(args) != 1:
            return ["error: bad command"]
        if args[0] not in recipes:
            return ["error: unknown recipe " + args[0]]
        return show_lines(recipes[args[0]])
    if cmd == "scale":
        if len(args) != 2 or not is_count(args[1]):
            return ["error: bad command"]
        if args[0] not in recipes:
            return ["error: unknown recipe " + args[0]]
        return scale_lines(recipes[args[0]], int(args[1]))
    if cmd == "shop":
        if len(args) < 2 or len(args) % 2 != 0:
            return ["error: bad command"]
        orders = []
        for i in range(0, len(args), 2):
            if not is_count(args[i + 1]):
                return ["error: bad command"]
            orders.append((args[i], int(args[i + 1])))
        for name, _ in orders:
            if name not in recipes:
                return ["error: unknown recipe " + name]
        return shopping_list(recipes, orders)
    if cmd == "list":
        if len(args) != 0:
            return ["error: bad command"]
        return list_lines(recipes)
    return ["error: unknown command " + cmd]


def main():
    recipes = {}
    for line in sys.stdin.read().split("\n"):
        words = [w for w in line.split(" ") if w != ""]
        if not words:
            continue
        for out in run(words, recipes):
            print(out)


main()
