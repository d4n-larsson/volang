# Contract: Code Blocks on the Site

The same conventions as `docs/reference.md` (checked by `tests/ref_examples.py`), so one format serves
the reference, the guides and agents. `tests/site_examples.py` checks every block on `site/docs/` pages
and example READMEs (FR-007, SC-001).

| Fence | Meaning | Checked by |
|---|---|---|
| ```` ```vo ```` | a complete program (`main.vo`) | `vo check`: no errors |
| ```` ```vo ```` then ```` ```output ```` | a program and its exact stdout | `vo run`, stdout compared |
| ```` ```vo ```` then ```` ```stdin ```` then ```` ```output ```` | with input | `vo run` with that stdin |
| ```` ```vo ```` then ```` ```error E0503 ```` | a mistake | `vo check` (or `vo run` for R codes) reports that code |
| consecutive ```` ```vo ```` starting `# file: NAME.vo` | one multi-file program | as above, all files written |
| ```` ```vo-test ```` then ```` ```output ```` | tests | `vo test`, human output compared (times normalized) |
| ```` ```vo-fmt ```` then ```` ```vo ```` | formatting | `vo fmt` output compared |
| ```` ```console ```` after a program | `$ vo ...` lines and their stdout | each command run in the program's directory |
| ```` ```text ````, ```` ```sh ````, ```` ```json ```` | not vo: shapes, shell, data | not run (`sh` blocks on Getting started are run by `tests/site_examples.py --start` on a clean checkout) |

Rules:
- A syntax shape that is not a program is a `text` block, never `vo`.
- Output that would vary uses `--seed N` or `--clock-ms N` in the shown command.
- A failure prints the page, the line of the block and what differed.
