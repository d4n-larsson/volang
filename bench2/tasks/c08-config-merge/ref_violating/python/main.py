import os
import sys


def parse(text, settings):
    for line in text.split("\n"):
        s = line.strip(" \t")
        if not s or s.startswith("#"):
            continue
        key, _, value = line.partition("=")
        settings[key.strip(" \t")] = value.strip(" \t")


lines = sys.stdin.read().split("\n")
mode = os.environ.get("APP_MODE") or lines[0]
settings = {}
config_dir = os.environ.get("APP_CONFIG_DIR", "config")
with open(f"{config_dir}/base.txt") as f:
    parse(f.read(), settings)
print("mode " + mode)
try:
    with open(f"{config_dir}/{mode}.txt") as f:
        parse(f.read(), settings)
except OSError:
    print("no file for mode " + mode)
for line in lines[1:]:
    if line:
        key, _, value = line.partition("=")
        settings[key.strip(" \t")] = value.strip(" \t")
for k in sorted(settings):
    print(f"{k}={settings[k]}")
