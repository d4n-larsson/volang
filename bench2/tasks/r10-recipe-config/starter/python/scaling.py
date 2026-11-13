from recipes import ingredient_line


def scale_qty(qty, servings, wanted):
    """qty for `servings` people, scaled to `wanted` people, rounded up."""
    return (qty * wanted + servings - 1) // servings


def scale_lines(recipe, wanted):
    out = [recipe.name + " for " + str(wanted) + ":"]
    for ing in recipe.ingredients:
        out.append(ingredient_line(ing.item, scale_qty(ing.qty, recipe.servings, wanted), ing.unit))
    return out


def shopping_list(recipes, orders):
    """orders: list of (recipe name, servings). Sums the scaled quantities per item and unit."""
    totals = {}
    for name, wanted in orders:
        r = recipes[name]
        for ing in r.ingredients:
            key = ing.item + " " + ing.unit
            totals[key] = totals.get(key, 0) + scale_qty(ing.qty, r.servings, wanted)
    out = ["shopping list (" + str(len(orders)) + " recipes):"]
    for key in sorted(totals):
        item, unit = key.split(" ")
        out.append(ingredient_line(item, totals[key], unit))
    return out
