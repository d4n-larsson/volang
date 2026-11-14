# Contract: Command Line

New commands. Existing commands keep their meaning; `--json` works with each.

```sh
vo doc     [--json] [NAME]                     # NAME: a module (std.json, money.format), an item (std.json.parse, print), or nothing
vo package [--json] ENTRY NAME DIRECTORY       # add or update a manifest entry (computes the hash)
vo package [--json] --remove ENTRY NAME        # remove an entry
```

## vo doc

- No NAME: the builtins (one line each, from the reference table) and every standard module, then every package module of the nearest manifest (from the working directory), each with its one-line description.
- A module: every item, in source order: `fn parse_json(s: str) -> result[Json]` and its description; records and variants with their fields and cases.
- An item: its signature, description and example. Examples are written `EXPR is VALUE` (e.g. `pad_left("7", 3, "0") is "007"`); `tests/doc_conformance.py` runs every one.
- Unknown NAME: T0010 (exit 64) with the closest module or item name.
- Text example:

  ```text
  $ vo doc std.text.pad_left
  fn pad_left(s: str, width: int, fill: str) -> str
      Pads s on the left with fill to width characters.
      example: pad_left("7", 3, "0") is "007"
  ```

- JSON: one record per item, `{"v":1,"kind":"doc","module":"std.text","name":"pad_left","item":"fn","tparams":[],"params":[{"name":"s","type":"str"},...],"returns":"str","effects":[],"signature":"fn pad_left(...) -> str","description":"...","example":"..."}`; records and variants have `fields`/`cases` instead of `params`/`returns`. A module listing ends with a `summary` record (`"command":"doc","items":N`).

## vo package

- Adds or updates `NAME DIRECTORY sha256:HEX` in the nearest `vo.packages` (created next to ENTRY when there is none). Output: `added money ../shared/money sha256:3f2a...` / `updated money: sha256:3f2a... -> sha256:91c0...` / `unchanged money` / `removed money`. JSON: `{"v":1,"kind":"package","name":"money","change":"added","directory":"../shared/money","hash":"sha256:..."[,"old_hash":"..."]}`.
- Errors: DIRECTORY missing or without `.vo` files: E0702 (exit 65); NAME not an identifier or `std`: T0001; `--remove` of a name not listed: T0001.

## Exit statuses

| Command | 0 | Other |
|---|---|---|
| `doc` | printed | 64 unknown name (T0010), 65 a package fails its check |
| `package` | manifest written | 64 usage, 65 package error |
