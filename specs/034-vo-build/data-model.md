# Data Model: One Executable per Program

## Executable layout

```text
| base toolchain bytes | payload | trailer (64 bytes) |
```

## Trailer (last 64 bytes, little-endian)

| Bytes | Field |
|---|---|
| 0–15 | magic `VO-PROGRAM-v1\0\0\0` |
| 16–23 | payload offset (= the base's length) |
| 24–31 | payload length |
| 32–63 | SHA-256 of the payload |

Checks at start: magic, offset + length + 64 = file size, hash. Any failure with the magic present is
T0014; no magic means a toolchain.

## Payload

A sequence of records, each `u64 kind`, `u64 length`, `length` bytes (little-endian lengths):

| Kind | Content | Count |
|---|---|---|
| 1 | version text (`vo 0.1.0 schema 1`) | one, first |
| 2 | entry path | one |
| 3 | a file's path, followed by a kind 4 record | one per file, in load order |
| 4 | that file's bytes | one per file |
| 5 | the package manifest's path, followed by a kind 4 record | zero or one |

No times, no host data: the payload depends only on the build's paths and file contents.
