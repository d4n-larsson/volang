# Feature flags

Decide which feature flags are on for each user, from a flag profile in the directory `flags/`.

## Input

Standard input, line 1: `profile <name>`. Every further non-empty line is a user: `user <id> <key>=<value> <key>=<value> ...`, separated by single spaces, with zero or more attributes. Ids are printable ASCII. Ids, keys and values contain no spaces and no `=`; a value may be empty (`plan=`). If a key appears twice on a line, the last value counts.

## Profile

The profile name is the value of the environment variable `FLAGS_PROFILE` when it is set and not empty; otherwise the name on line 1 of the input.

A **valid profile name** is 1 to 16 characters: a lowercase ASCII letter, then lowercase ASCII letters, digits or `-`. The profile `<name>` is the file `flags/<name>.flags`. A profile may extend another profile, which may extend another, and so on (its **chain**: the profile, its parent, the parent's parent, ...).

Load the chain starting at the selected profile. Stop at the first problem, print only its line, and nothing else:

- `error: bad profile <name>`: a name (the selected one, or one named by `extends`) is not a valid profile name. A file for an invalid name must never be opened, nor anything outside `flags/`.
- `error: no profile <name>`: `flags/<name>.flags` does not exist.
- `error: extends cycle <p1> -> <p2> -> ... -> <p1>`: a profile names, through `extends`, a profile already in the chain; list the chain from that earlier profile up to the repeated one (`a` extends `b`, `b` extends `c`, `c` extends `b`: `error: extends cycle b -> c -> b`).

## Profile files

Lines are trimmed of spaces at both ends; empty lines and lines starting with `#` are ignored. Words on a line are separated by single spaces. The files are otherwise well-formed:

- At most one `extends <name>` line, before the first `flag` line.
- `flag <flag> default on` or `flag <flag> default off` starts a flag definition. The lines up to the next `flag` line belong to it:
  - `requires <flag>` (any number),
  - `when <key> <op> <value> then on` or `... then off` (a **rule**; any number, in order),
  - `rollout <p>`, `p` an integer from 0 to 100 (at most one).

Flag names are lowercase ASCII letters, digits, `-` and `_`. A flag is defined at most once per file. A flag defined in a profile replaces, entirely, the definition of the same flag in every profile further up its chain. The **flags of the profile** are all flags defined anywhere in its chain.

If the `requires` lines make a flag depend on itself, directly or through other flags (only flags of the profile count; a `requires` naming an undefined flag is no dependency), print only `error: requires cycle <flag>`, with the smallest (byte order) flag that depends on itself.

## Evaluating a flag for a user

1. Each required flag (in order) is evaluated for the same user; if any is off, or not a flag of the profile, the flag is off.
2. Otherwise the first rule whose condition holds decides (`then on` / `then off`).
3. Otherwise, if the flag has a `rollout p`: it is on when the user's bucket is less than `p`. The bucket is `h % 100`, where `h = 0` and, for each byte `b` of the text `<flag>:<user id>` (ASCII), `h = (h * 31 + b) % 1000003`.
4. Otherwise its default.

Conditions (a user without the key never satisfies a condition, whatever the operator):

- `=` and `!=`: the user's value is, or is not, exactly the rule's value.
- `in`: the rule's value is a comma-separated list (`gold,silver`); the user's value is exactly one of its items.
- `>=` and `<`: version comparison. A version is one or more parts separated by `.`, each part one to nine ASCII digits (`2`, `1.10`, `03.0.1`). Compare parts as integers from the left; a missing part counts as 0, so `1.2` equals `1.2.0` and `1.10` is greater than `1.9`. If the user's value is not a version, the condition does not hold. Rule values are always versions.

## Output

1. `profile <name>`, then `chain <name> <parent> ...` (the whole chain, from the selected profile up).
2. For each user, in input order: `<id> <flags>`, where `<flags>` are the flags that are on for that user, sorted (byte order), joined by `,`; or `-` if none is on.
3. `== counts`, then for each flag of the profile, sorted: `<flag> <number of users for whom it is on>`.

## Allowed effects

The program may print, read standard input, read files under `flags/`, and read the environment variable `FLAGS_PROFILE`. Nothing else: no other environment variables (the flag directory is not configurable), no reading outside `flags/`, no randomness (rollouts use the hash above), no clock, no writing files, no network.
