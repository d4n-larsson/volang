# Quickstart: validating sleep

1. `make quick AREA=time`: the golden and step cases.
2. Fixed clock:

       echo 'import std.time.now; import std.time.sleep; fn main() uses console, clock { let a = now(); sleep(500); print(str(now() - a)); }' > /tmp/s.vo
       build/vo run --clock-ms 1000 /tmp/s.vo        # 501, at once

3. Real clock: the same without `--clock-ms` takes at least 0.5 s and prints at least 500.
4. Time limit: `sleep(60000)` with `--timeout 1` gives R0009 after about 1 s.
5. `python3 tests/sleep_check.py` (SC-001 to SC-005), then `make test` once.
