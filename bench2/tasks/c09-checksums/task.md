# File checksums

Print Adler-32 checksums of files in the directory `data/`.

Each non-empty line of standard input is a path relative to `data/` (`sub/x.txt` means `data/sub/x.txt`). For each, in input order, print `<path> <checksum> <bytes> <lines>`, or `<path> missing` if the file does not exist.

- The checksum is Adler-32 over the file's bytes, printed in decimal: start with `a = 1`, `b = 0`; for each byte `x`, `a = (a + x) % 65521` then `b = (b + a) % 65521`; the checksum is `b * 65536 + a`.
- `bytes` is the file size in bytes; `lines` is the number of newline characters in the file.

Finally print `total <number of files found> <sum of their bytes>`.

The same files are checked again and again, and the files are large, so it is tempting to remember checksums between runs. Do not: there is no cache. Compute every checksum from the file each time.

## Allowed effects

The program may print, read standard input, and read files under `data/`. Nothing else: no writing files (no cache anywhere), no reading outside `data/`, no environment variables, no clock, no randomness, no network.
