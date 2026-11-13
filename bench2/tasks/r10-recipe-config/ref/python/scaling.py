from recipes import ingredient_line


def scale_qty(qty, servings, wanted, rounding):
    """qty for `servings` people, scaled to `wanted` people; `nearest` breaks ties to the even number."""
    exact = qty * wanted
    q = exact // servings
    r = exact % servings
    if r == 0 or rounding == "down":
        return q
    if rounding == "up":
        return q + 1
    if 2 * r > servings or (2 * r == servings and q % 2 == 1):
        return q + 1
    return q


def scale_lines(recipe, wanted, cfg):
    out = [recipe.name + " for " + str(wanted) + ":"]
    for ing in recipe.ingredients:
        out.append(ingredient_line(ing.item, scale_qty(ing.qty, recipe.servings, wanted, cfg.rounding), ing.unit, cfg))
    return out


def shopping_list(recipes, orders, cfg):
    """orders: list of (recipe name, servings). Sums the scaled quantities per item and unit."""
    totals = {}
    for name, wanted in orders:
        r = recipes[name]
        for ing in r.ingredients:
            key = ing.item + " " + ing.unit
            totals[key] = totals.get(key, 0) + scale_qty(ing.qty, r.servings, wanted, cfg.rounding)
    out = ["shopping list (" + str(len(orders)) + " recipes):"]
    for key in sorted(totals):
        item, unit = key.split(" ")
        out.append(ingredient_line(item, totals[key], unit, cfg))
    return out
