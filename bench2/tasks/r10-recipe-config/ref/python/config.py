ROUNDINGS = ["up", "down", "nearest"]
UNIT_SYSTEMS = ["metric", "kitchen"]


class Config:
    def __init__(self, rounding, units):
        self.rounding = rounding
        self.units = units


def default_config():
    return Config("up", "metric")


def parse_config(settings):
    """The words after `config`: each KEY=VALUE, each key at most once. None when invalid."""
    cfg = default_config()
    seen = []
    for w in settings:
        parts = w.split("=")
        if len(parts) != 2 or parts[0] in seen:
            return None
        key, value = parts
        seen.append(key)
        if key == "rounding" and value in ROUNDINGS:
            cfg.rounding = value
        elif key == "units" and value in UNIT_SYSTEMS:
            cfg.units = value
        else:
            return None
    return cfg


def config_line(cfg):
    return "config rounding=" + cfg.rounding + " units=" + cfg.units
