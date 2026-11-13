from units import is_unit, format_qty


class Ingredient:
    def __init__(self, item, qty, unit):
        self.item = item
        self.qty = qty
        self.unit = unit


class Recipe:
    def __init__(self, name, servings, ingredients):
        self.name = name
        self.servings = servings
        self.ingredients = ingredients


def add_recipe(recipes, name, servings, ingredients):
    for ing in ingredients:
        if not is_unit(ing.unit):
            return "error: bad unit " + ing.unit
    if name in recipes:
        return "error: recipe " + name + " exists"
    seen = []
    for ing in ingredients:
        if ing.item in seen:
            return "error: duplicate " + ing.item
        seen.append(ing.item)
    recipes[name] = Recipe(name, servings, ingredients)
    return "recipe " + name + " added (" + str(len(ingredients)) + " ingredients)"


def ingredient_line(item, qty, unit, cfg):
    return "  " + item + " " + format_qty(qty, unit, cfg)


def show_lines(recipe, cfg):
    out = [recipe.name + " (" + str(recipe.servings) + " servings)"]
    for ing in recipe.ingredients:
        out.append(ingredient_line(ing.item, ing.qty, ing.unit, cfg))
    return out


def list_lines(recipes):
    if len(recipes) == 0:
        return ["no recipes"]
    out = []
    for name in sorted(recipes):
        r = recipes[name]
        out.append(name + ": " + str(r.servings) + " servings, " + str(len(r.ingredients)) + " ingredients")
    return out
