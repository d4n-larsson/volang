# Contract: `vo.packages`

```text
# vo packages: update with `vo package ENTRY NAME DIRECTORY`
NAME DIRECTORY sha256:HEX                                   a local directory
NAME DIRECTORY sha256:HEX https://…/x.tar.gz[#PATH]         an archive (spec 041)
NAME DIRECTORY sha256:HEX https://HOST/PATH[#FOLDER]@VERSION           a versioned package, asked for
NAME DIRECTORY sha256:HEX https://HOST/PATH[#FOLDER]@VERSION indirect  one that a package requires
```

- Sorted by name, one entry per name (E0704), fields separated by one space when written.
- The hash covers the `.vo` files under DIRECTORY, as before; the package's own `vo.packages`, written
  beside them for versioned packages, is not part of it (research R5).
- The same file in a package's repository root (or `#FOLDER`) declares what the package requires: its
  versioned lines. Its directories and hashes describe the author's checkout.
- A manifest must be complete: every versioned line of every listed package's manifest is met by a line
  here with the same source and a compatible, equal or higher version (E0709 from `vo check`).
- Older toolchains report lines with `@VERSION` or `indirect` as malformed (E0703).
