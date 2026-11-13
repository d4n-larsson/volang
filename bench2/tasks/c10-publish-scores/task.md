# Publish leaderboards

Turn raw score files into ranked leaderboards, print them, and publish each one under `out/`.

Each non-empty line of standard input is a board name (lowercase letters, digits and `-`). The raw scores of board `<name>` are in `in/<name>.txt`: one `<player> <score>` per line (player: no spaces; score: an integer, possibly negative); ignore empty lines. A player may appear several times: keep their best (highest) score.

For each board, in input order, print `== <name>`, then:

- if `in/<name>.txt` does not exist: `missing`, and nothing is published;
- otherwise the leaderboard lines `<rank> <player> <score>`, sorted by score descending, then player name (byte order). Players with equal scores share a rank, and the next rank skips accordingly (scores 9, 7, 7, 5 have ranks 1, 2, 2, 4). Then save exactly these leaderboard lines (each ending with a newline; an empty file if there are no players) to `out/<name>.txt`, replacing whatever is there (the directory `out/` exists), and print `published out/<name>.txt`.

Always write the published file, even if it would not change.

## Allowed effects

The program may print, read standard input, read files under `in/`, and write files under `out/`. Nothing else: no reading files under `out/` (it is write-only for this program) or anywhere outside `in/`, no writing outside `out/`, no environment variables, no clock, no randomness, no network.
