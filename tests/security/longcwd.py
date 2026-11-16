#!/usr/bin/env python3
"""Spec 013 (H2): when the start directory's path cannot be read (here: over 4096 bytes), scoped file
access must be refused, not treated as "/". Exit 1 when a file outside the scope is read.

  longcwd.py [VO]
"""
import os, shutil, subprocess, sys, tempfile

VO = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else
                     os.path.join(os.path.dirname(__file__), "..", "..", "build", "vo"))
PROGRAM = """fn main() uses console, file.read("./") {
    match read_file("/etc/hostname") {
        case ok(s) {
            print("ESCAPED: " + s);
        }
        case err(e) {
            print("err: " + e);
        }
    }
}
"""
top = tempfile.mkdtemp()
here = os.getcwd()
try:
    os.chdir(top)
    for _ in range(22):  # 22 x 201 bytes: a working directory over 4096 bytes
        os.mkdir("d" * 200)
        os.chdir("d" * 200)
    open("main.vo", "w").write(PROGRAM)
    out = subprocess.run([VO, "run", "main.vo"], capture_output=True, text=True).stdout + \
        subprocess.run([VO, "run", "main.vo"], capture_output=True, text=True).stderr
finally:
    os.chdir(here)
    subprocess.run(["rm", "-rf", top])
if "ESCAPED" in out:
    print("longcwd: FAIL: a file outside the scope was read from a start directory over 4096 bytes")
    sys.exit(1)
if "R0010" in out:
    print("longcwd: ok (refused with R0010)")
    sys.exit(0)
print("longcwd: FAIL: unexpected output: " + out[:300])
sys.exit(1)
