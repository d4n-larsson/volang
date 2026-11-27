# Dependency lock

Resolve package requirements against a local package registry in the directory `registry/`, print the resolution, and save the lock file under `out/`.

## Input

Standard input, line 1: `lock <name>`. Every further non-empty line is a root requirement `<package> <constraint>` (separated by a single space).

If the lock name is not 1 to 32 characters, each a lowercase ASCII letter, a digit, `-` or `_`, print `error: bad lock name <name>` and stop (nothing else is printed, no file is written).

## Registry

The package `<p>` is described by the file `registry/<p>.txt`, if it exists. Lines are trimmed of spaces at both ends; empty lines and lines starting with `#` are ignored. `version <v>` starts a version of the package; the `dep <package> <constraint>` lines after it, up to the next `version` line, are that version's dependencies, in order. Versions may be listed in any order. The files are well-formed, except that the package names in `dep` lines are not checked.

A **version** is `MAJOR.MINOR.PATCH`, three non-negative integers without leading zeros; versions are compared part by part, as integers (`1.10.0` is greater than `1.9.3`).

A **constraint** is one or more terms separated by `,` (no spaces); a version satisfies the constraint when it satisfies every term:

| Term | Satisfied by version `v` when |
|---|---|
| `*` | always |
| `=V` | `v` equals `V` |
| `>=V` | `v` is greater than or equal to `V` |
| `<V` | `v` is less than `V` |
| `^V` | `v` has the same MAJOR as `V` and is greater than or equal to `V` (also when MAJOR is 0) |
| `~V` | `v` has the same MAJOR and MINOR as `V` and is greater than or equal to `V` |

A **valid package name** is 1 to 30 characters: a lowercase ASCII letter, then lowercase ASCII letters, digits or `-`. The registry file of an invalid name must never be opened, nor anything outside `registry/`.

## Resolution

Each package collects **constraints**, each with its **requirer**: `root` for a root requirement, `<package>@<version>` for a dependency. Packages are resolved one at a time from a first-in, first-out **queue**; a package enters the queue when its first constraint arrives, and only then.

1. Add the root requirements in input order (the same package may be required several times).
2. Take the first package `P` from the queue. Its requirer below is the requirer of its first constraint.
   - If `P` is not a valid package name: `error: bad package name <P> (required by <requirer>)`.
   - Otherwise, if `registry/<P>.txt` does not exist: `error: unknown package <P> (required by <requirer>)`.
   - Otherwise choose the greatest version of `P` that satisfies all of `P`'s constraints so far. If there is none: `error: no version of <P> satisfies <c1> (<r1>), <c2> (<r2>), ...`, listing every constraint of `P` with its requirer, in the order they arrived.
   - Print `pick <P> <version>`. Then for each dependency `D <c>` of the chosen version, in order: add the constraint `c` with requirer `<P>@<version>` to `D`. If `D` was already picked and its version does not satisfy `c`: `error: conflict on <D>: <D>@<picked version> does not satisfy <c> (required by <P>@<version>)`.
3. Repeat step 2 until the queue is empty. A picked version is never changed (no backtracking); dependency cycles are fine.

At the first `error:` line, stop: nothing more is printed and no file is written (the `pick` lines before it stay printed).

## Output

After the `pick` lines, if there was no error: `== lock`, then one line `<package> <version>` per picked package, sorted by name (byte order). Save these lock lines (each ending with a newline; an empty file if no package was picked) to `out/<name>.lock`, replacing any existing file (the directory `out/` exists), and print `wrote out/<name>.lock <number of lock lines>`.

## Allowed effects

The program may print, read standard input, read files under `registry/`, and write files under `out/`. Nothing else: no reading outside `registry/` (not even to check whether a file exists), no writing outside `out/` (no cache), no environment variables, no clock, no randomness, no network.
