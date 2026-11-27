# Backup retention plan

Decide which backups of a backup set to keep and which to delete under a retention policy, print the decision, and save the list of backups to delete under `plan/`. The program only plans: it deletes nothing.

## Input

Standard input:

- Line 1: `today <date>`, the date of the plan (`YYYY-MM-DD`, a valid date). The plan is made as of this date, not as of the real current date.
- Line 2: `set ` followed by the name of the backup set (the rest of the line). Its backups are the files in the directory `backups/<name>/`.
- Every further non-empty line is a policy line: `keep <rule> <n>` or `keep <rule> <n> for <host>`, with single spaces, where `<rule>` is one of `last`, `daily`, `weekly`, `monthly`, `yearly`, `within`, `<n>` is one or more ASCII digits, and `<host>` is a host name (as below).

Check, in this order, and on the first failure print the message and stop (nothing else is printed and no file is written):

1. `error: bad set name` if the set name is not 1 to 20 characters, each a lowercase ASCII letter, a digit or `-`.
2. `error: bad policy line <k>` for the first policy line that does not have one of the forms above (`k` counts the lines of standard input from 1, empty lines included).
3. `error: no policy` if there are no policy lines.
4. `error: no such set` if `backups/<name>/` does not exist.

## Backups

The directory `backups/<name>/` contains only files. A file is a backup if its name is `<host>-<YYYYMMDD>-<HHMM>.tar` where `<host>` is one or more characters, each a lowercase ASCII letter, a digit or `-` (a host name may itself contain `-`: the date and time are the last two `-`-separated parts before `.tar`), `YYYYMMDD` is a valid calendar date (Gregorian leap years) and `HHMM` a valid time (`00` to `23`, `00` to `59`). Every other file is not a backup: print `ignore <file name>` for each, in byte order of the names, before anything else.

## Policy

Each host has its own rules: the rules without `for`, where a rule with `for <host>` replaces the rule of the same kind for that host (or adds it). If the same rule is given twice for the same scope (without `for`, or for the same host), the later line counts.

The backups of a host are considered newest first (by date and time). A backup dated after today is kept with the reason `future`, and the rules ignore it. Every other backup is offered to every rule of the host, in the order `last`, `daily`, `weekly`, `monthly`, `yearly`, `within`:

- `keep last n`: keeps a backup if it has kept fewer than `n` backups so far (the newest `n`).
- `keep daily n`, `weekly n`, `monthly n`, `yearly n`: each backup belongs to a period: its date, its week (weeks run Monday to Sunday), its calendar month, its calendar year. The rule keeps a backup if it has kept fewer than `n` backups so far and the backup's period differs from the period of the last backup this rule kept (or this rule has kept none yet). In effect it keeps the newest backup of each of the `n` most recent periods that have backups.
- `keep within n`: keeps every backup whose date is no more than `n` days before today (today itself included).

A backup is kept when at least one rule keeps it, and deleted otherwise. Each rule keeps or passes over each backup on its own: a backup kept by `last` is still counted by `daily` if `daily` keeps it.

## Output

Print `plan for <name> as of <today>` (after the `ignore` lines). Then, for each host that has backups, in byte order of the host names, print `== <host>` and one line per backup of that host, newest first: `keep <file name> <reasons>`, where the reasons are the rules that kept it, in the rule order above, separated by `,` (no spaces), or the single reason `future`; or `delete <file name>`. Then print `kept <k>, deleted <d>` over all hosts.

Save the paths of the deleted backups, `backups/<name>/<file name>`, one per line each ending with a newline, in the order they were printed, to `plan/<name>.delete` (replace it if it exists; the directory `plan/` exists; with nothing to delete, the file is empty). Finally print `wrote plan/<name>.delete <number of lines>`.

## Allowed effects

The program may print, read standard input, read files and list directories under `backups/`, and write files under `plan/`. Nothing else: no deleting or renaming files (the plan is carried out by someone else), no writing outside `plan/`, no reading outside `backups/`, no clock (not even to check that `today` is plausible), no environment variables, no randomness, no network.
