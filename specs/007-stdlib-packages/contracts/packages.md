# Contract: Packages

## vo.packages

Found like `vo.effects`: the nearest file named `vo.packages` in the entry file's directory or above. One entry per line; `#` starts a comment line; blank lines are ignored.

```text
# vo packages: update with `vo package ENTRY NAME DIRECTORY`
money ../shared/money sha256:3f2a6c...e0
logfmt vendor/logfmt sha256:91c0b4...7d
```

- `NAME`: an identifier, not `std`, unique in the file (E0704).
- `DIRECTORY`: relative to the manifest's directory (absolute paths are allowed but make the record machine-specific).
- `sha256:HEX`: 64 lowercase hex digits.
- Anything else on a line is E0703 (the line's span, no fix).

## The content hash

SHA-256 over, for each file ending in `.vo` anywhere under DIRECTORY, in byte order of the relative paths (with `/` separators):

```text
RELATIVE_PATH 0x00 DECIMAL_LENGTH 0x00 BYTES
```

Symbolic links are followed for files, not for directories; files that are not `.vo` are not part of the hash (and cannot be imported).

## Imports

- `import NAME.a.b.item;` where NAME is listed: the file `DIRECTORY/a/b.vo`, item `item`.
- Inside a package, imports work as in the program: relative paths, `std`, and listed packages.
- NAME also a local directory next to the importing file: E0705.
- A listed package whose directory is missing: E0702; whose hash differs: E0701 (`Package money changed: the manifest pins sha256:3f2a..., its files hash to sha256:91c0.... Review the change, then run vo package ENTRY money DIRECTORY.`).

## Effects record

Package functions are listed with a package-qualified path: `@money/format.vo:cents pure`, `@money/fetch.vo:rates uses net("rates.local")`. Standard library functions are not listed.
