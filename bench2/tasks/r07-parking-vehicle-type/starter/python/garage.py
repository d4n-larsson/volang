class Level:
    def __init__(self, number, spots):
        self.number = number
        self.spots = [""] * spots
        self.exits = 0
        self.revenue = 0


class Vehicle:
    def __init__(self, plate, level, spot, since):
        self.plate = plate
        self.level = level
        self.spot = spot
        self.since = since


class Garage:
    def __init__(self):
        self.levels = {}
        self.vehicles = {}


def level_numbers(garage):
    return sorted(garage.levels)


def spot_name(vehicle):
    return str(vehicle.level) + "-" + str(vehicle.spot)


def open_level(garage, number, spots):
    if number in garage.levels:
        return "error: level " + str(number) + " exists"
    garage.levels[number] = Level(number, spots)
    return "level " + str(number) + " opened with " + str(spots) + " spots"


def park(garage, plate, time):
    if plate in garage.vehicles:
        return "error: " + plate + " already parked"
    for number in level_numbers(garage):
        level = garage.levels[number]
        for i in range(len(level.spots)):
            if level.spots[i] == "":
                level.spots[i] = plate
                vehicle = Vehicle(plate, number, i + 1, time)
                garage.vehicles[plate] = vehicle
                return plate + " parked at " + spot_name(vehicle)
    return "error: garage full"


def unpark(garage, vehicle):
    garage.levels[vehicle.level].spots[vehicle.spot - 1] = ""
    del garage.vehicles[vehicle.plate]


def occupancy(garage):
    out = []
    used_all = 0
    spots_all = 0
    for number in level_numbers(garage):
        level = garage.levels[number]
        used = sum(1 for p in level.spots if p != "")
        out.append("level " + str(number) + ": " + str(used) + "/" + str(len(level.spots)) + " used")
        used_all += used
        spots_all += len(level.spots)
    out.append("total " + str(used_all) + "/" + str(spots_all) + " used")
    return out


def parked(garage):
    out = []
    for number in level_numbers(garage):
        for plate in garage.levels[number].spots:
            if plate != "":
                vehicle = garage.vehicles[plate]
                out.append(spot_name(vehicle) + " " + plate + " since " + str(vehicle.since))
    if not out:
        return ["no vehicles"]
    return out
