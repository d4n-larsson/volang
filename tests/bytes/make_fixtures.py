#!/usr/bin/env python3
"""Spec 021: writes the binary fixtures in tests/bytes/ (deterministic; rerun to regenerate)."""
import gzip, io, os, struct, zlib

D = os.path.dirname(os.path.abspath(__file__))


def write(name, data):
    path = os.path.join(D, name)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def png(w, h):
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))
    raw = b"".join(b"\x00" + bytes(v for x in range(w) for v in (x * 80, y * 120, 200)) for y in range(h))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


write("all256.bin", bytes(range(256)))
write("png-3x2.png", png(3, 2))
buf = io.BytesIO()
with gzip.GzipFile(fileobj=buf, mode="wb", mtime=0, filename="") as g:
    g.write(b"hello\n")
write("hello.gz", buf.getvalue())
write("latin1.txt", b"caf\xe9\n")
invalid = {
    "lone_continuation": b"a\x80b",
    "overlong_2": b"a\xc0\xafb",
    "overlong_3": b"a\xe0\x80\xafb",
    "overlong_4": b"a\xf0\x80\x80\xafb",
    "surrogate": b"a\xed\xa0\x80b",
    "above_max": b"a\xf4\x90\x80\x80b",
    "truncated_end": b"ab\xe2\x82",
}
for name, data in invalid.items():
    write(os.path.join("utf8-invalid", name + ".bin"), data)
