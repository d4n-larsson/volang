# Unpack archives

Unpack text archives from the directory `incoming/` into `out/`, safely, and print what was written.

## Input

Each non-empty line of standard input is an archive name. Handle them in input order. For each, print `== <name>`, then:

- `bad name` if the name is not 1 to 20 characters, each a lowercase ASCII letter, a digit or `-`;
- otherwise `duplicate` if the same name appeared on an earlier line;
- otherwise `not found` if `incoming/<name>.pack` does not exist;
- otherwise `not a pack` if the archive's first line is not exactly `PACK 1` (an empty file has no first line);
- otherwise unpack it into the directory `out/<name>/` (the **destination**) as described below.

## Archive format

An archive is ASCII text made of lines, each ended by a newline (the newline of the last line may be missing). Line 1 is `PACK 1`. Then come entries, each starting on its own line; the fields of an entry line are separated by single spaces:

- `F <path> <n>`: a file. `n` is one or more ASCII digits. The next `n` lines of the archive are its content, whatever they contain (they may be empty or look like entries); the file's content is those lines, each followed by a newline (`n = 0` is an empty file).
- `D <path>`: a directory.
- `L <path> -> <target>`: a symbolic link.
- `END <count>`: the end of the archive. `count` is one or more ASCII digits. Everything after this line is ignored.

Paths contain no spaces. Any other line where an entry should start (an empty line, an unknown letter, a wrong number of fields, a count or `n` that is not digits, a link line whose third field is not `->`) is **bad**: print `error: line <k>: bad entry` (`k` is its line number in the archive, line 1 being `PACK 1`) and stop reading the archive. If the archive ends where an entry should start, or before a file's `n` content lines, print `error: truncated` and stop reading the archive. What was unpacked before an error stays unpacked and is listed as below.

## Paths

Every entry path is taken relative to the destination and normalized as text, part by part, splitting on `/`: empty parts and `.` are dropped; `..` removes the previous kept part. The **normalized path** is the kept parts joined with `/`. An entry is **skipped** with the line `skip <path>: <reason>` (`<path>` exactly as written in the archive), by the first rule that applies:

For a link: always `link`. Links are never created.

For a file or a directory:

1. `outside`: the path starts with `/`, or a `..` part has no previous kept part to remove (it would leave the destination). Nothing may be written outside the destination, not even inside other parts of `out/`.
2. `empty`: the normalized path is empty.
3. `parent is a file`: a proper prefix of the normalized path (a leading run of its parts) is a file unpacked earlier from this archive.
4. For a file, `is a directory`: the normalized path is already a directory of this archive's tree (as made by the entries before this one). For a directory, `is a file`: the normalized path is a file unpacked earlier from this archive.

The **directories** of the archive's tree are the normalized paths of the directory entries that were not skipped, and every proper prefix of the normalized path of every file and directory entry that was not skipped.

Otherwise the entry is unpacked:

- A file is written to `out/<name>/<normalized path>`, creating the directories it needs. Print `file <normalized path> <bytes>`, or `replace <normalized path> <bytes>` if an earlier entry of this archive already unpacked a file at that normalized path (the new content replaces the old). `bytes` is the content's length in bytes.
- A directory is created (with its parents). Print `dir <normalized path>`.

At `END <count>`, if `count` differs from the number of entry lines (`F`, `D` and `L` lines, skipped or not) read before it, print `warning: END says <count>, found <m>`.

## Listing

After the archive is read (completely or up to an error), print one line `= <normalized path> <bytes> <checksum>` for every file of the archive's tree (each normalized path once, with its final content), sorted by normalized path (byte order), and then `total <files> files, <dirs> dirs, <bytes> bytes`, where `dirs` is the number of distinct directories of the tree and `bytes` the sum of the listed files' sizes.

The checksum of a file is computed over its content: start with `h = 7`; for each byte `b` (its value 0 to 255), `h = (h * 31 + b) % 1000000007`; the checksum is the final `h`.

The listing and the messages depend only on the archive, never on what is already in `out/`: files there from earlier runs are simply overwritten.

## Allowed effects

The program may print, read standard input, read files under `incoming/`, and write files and create directories under `out/`. Nothing else: no writing outside `out/` (no temporary or log files anywhere else), no reading outside `incoming/`, no environment variables, no clock, no randomness, no network.
