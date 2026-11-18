# Quickstart: validating binary data

1. **The type** (US1, US2): `make quick AREA=bytes` runs `tests/cases/bytes` and `tests/cases/bytes_steps`.
   Expected:
   - reading `tests/bytes/png-1x1.png` prints `137 80 78 71`;
   - the 0..255 fixture round-trips through a file, `to_ints`/`from_ints`, hex and base64.
2. **Strict text** (US7):

       printf 'a\377b' > /tmp/x.bin
       echo 'fn main() uses console, file.read("/tmp") { match read_file("/tmp/x.bin") { case ok(s) { print(s); } case err(e) { print(e); } } }' > /tmp/t.vo
       build/vo run /tmp/t.vo     # not UTF-8 at byte 1 (std.fs.read_bytes reads any bytes)

3. **Processes** (US3): the step case runs `tests/process_helpers/allbytes` and compares the 256 bytes.
4. **HTTP** (US6): step cases against `tests/http_server.py` and `tests/https_server.py`, with the
   `/allbytes` and `/echo` routes, and the record and replay of each.
5. **Fuzz** (SC-007): `python3 tests/bytes_fuzz.py`, 10,000 inputs per path, 0 invalid `str`.
6. **Scale** (SC-004, SC-008): `python3 tests/bytes_scale.py`.
7. **Complete**: `make test` once, then `make ARCH=aarch64 test` and `make ARCH=riscv64 test` at the end of
   the spec; the reference stays within budget (`tests/ref_examples.py`).
