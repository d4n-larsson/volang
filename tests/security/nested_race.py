#!/usr/bin/env python3
"""Bug sweep 2026-11-27 (finding 9): a scope nested in another (`file.read("data/sub/")` under
`file.read("data/")`) is a directory inside the outer grant, so a process that writes there can swap
it for a link. The open must not follow it out of the outer grant. Runs the program RUNS times while
a helper exchanges `data/sub` with `data/lnk -> ../outside`; exit 1 on any read of the secret.

  nested_race.py [VO] [RUNS]   (RUNS default 300)
"""
import ctypes, os, subprocess, sys, tempfile, threading

VO = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else
                     os.path.join(os.path.dirname(__file__), "..", "..", "build", "vo"))
RUNS = int(sys.argv[2]) if len(sys.argv) > 2 else 300
PROGRAM = """fn get(p: str) -> str uses file.read("data/sub/") {
    match read_file(p) {
        case ok(s) {
            return s;
        }
        case err(e) {
            return "";
        }
    }
}

fn main() uses console, file.read("data/") {
    var i = 0;
    while i < 2000 {
        if get("data/sub/x") == "TOPSECRET" {
            print("SECRET");
        }
        set i = i + 1;
    }
}
"""
d = tempfile.mkdtemp()
os.makedirs(os.path.join(d, "data", "sub"))
os.mkdir(os.path.join(d, "outside"))
open(os.path.join(d, "data", "sub", "x"), "w").write("benign")
open(os.path.join(d, "outside", "x"), "w").write("TOPSECRET")
open(os.path.join(d, "main.vo"), "w").write(PROGRAM)
os.symlink("../outside", os.path.join(d, "data", "lnk"))
libc = ctypes.CDLL(None, use_errno=True)
a, b = os.path.join(d, "data", "sub").encode(), os.path.join(d, "data", "lnk").encode()
stop = False

def flip():
    while not stop:
        libc.renameat2(-100, a, -100, b, 2)  # AT_FDCWD, RENAME_EXCHANGE

t = threading.Thread(target=flip)
t.start()
won = 0
try:
    for _ in range(RUNS):
        r = subprocess.run([VO, "run", "main.vo"], cwd=d, capture_output=True, text=True)
        won += r.stdout.count("SECRET")
finally:
    stop = True
    t.join()
    subprocess.run(["rm", "-rf", d])
print(f"nested_race: {RUNS} runs, {won} reads outside the outer scope")
sys.exit(1 if won else 0)
