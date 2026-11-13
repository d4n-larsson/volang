from garage import level_numbers, spot_name, unpark
from fees import money, fee


def find_parked(garage, plate, time):
    """(vehicle, error): the parked vehicle, or None and the error for `time`."""
    if plate not in garage.vehicles:
        return None, "error: " + plate + " not parked"
    vehicle = garage.vehicles[plate]
    if time < vehicle.since:
        return None, "error: time before entry"
    return vehicle, ""


def exit_vehicle(garage, plate, time):
    vehicle, error = find_parked(garage, plate, time)
    if vehicle is None:
        return error
    minutes = time - vehicle.since
    amount = fee(vehicle.kind, minutes)
    level = garage.levels[vehicle.level]
    level.exits += 1
    level.revenue += amount
    unpark(garage, vehicle)
    return plate + " left " + spot_name(vehicle) + " after " + str(minutes) + " min, fee " + money(amount)


def quote(garage, plate, time):
    vehicle, error = find_parked(garage, plate, time)
    if vehicle is None:
        return error
    minutes = time - vehicle.since
    return plate + " owes " + money(fee(vehicle.kind, minutes)) + " after " + str(minutes) + " min"


def pending(garage, number, time):
    """What the vehicles on level `number` would owe if they all left at `time`."""
    total = 0
    for vehicle in garage.vehicles.values():
        if vehicle.level == number and vehicle.since <= time:
            total += fee(vehicle.kind, time - vehicle.since)
    return total


def report(garage, time):
    out = []
    exits = 0
    revenue = 0
    owed = 0
    for number in level_numbers(garage):
        level = garage.levels[number]
        p = pending(garage, number, time)
        out.append("level " + str(number) + ": exits " + str(level.exits) + " revenue " + money(level.revenue)
                   + " pending " + money(p))
        exits += level.exits
        revenue += level.revenue
        owed += p
    out.append("total exits " + str(exits) + " revenue " + money(revenue) + " pending " + money(owed))
    return out
