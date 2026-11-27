# Configuration includes

Resolve configuration files that include each other, all kept in the directory `conf/`, and print the effective settings.

## Input

Each non-empty line of standard input names a **root** file, relative to `conf/`. Handle the roots one after the other, each from scratch (no settings carry over from one root to the next).

## Paths

Every file is named by a path relative to `conf/`. A root name is resolved from `conf/` itself; an include path is resolved from the directory of the file that contains the include line (`include b.conf` in `env/a.conf` means `env/b.conf`).

To resolve a path: if it starts with `/`, it is **outside**. Otherwise append its `/`-separated parts to the parts of the starting directory, one at a time: skip empty parts and `.`; `..` removes the last part collected so far, and if there is none the path is **outside**. The **normalized name** of the file is the remaining parts joined with `/` (`env/../db.conf` from `conf/` is `db.conf`). A path that is outside must not be opened, and nothing outside `conf/` may be read in any other way either. Paths never name a directory.

## File format

Remove spaces at both ends of every line (lines contain no tabs). Line numbers count every line of the file, starting at 1. Each line, after removing the spaces, is handled by the first rule that applies:

1. Empty, or starts with `#`: ignored.
2. Starts with `include? ` (with that space): an **optional include** of the path that follows (spaces removed at both ends).
3. Starts with `include ` (with that space): an **include** of the path that follows (spaces removed at both ends).
4. Starts with `[` and ends with `]`: a **section** line. The text between the brackets must be empty or 1 or more lowercase ASCII letters, digits and `_`; otherwise it is the error `bad section` and the line is ignored. Otherwise it sets the current section of this file (empty: no section). Every file starts without a section, and including another file does not change the including file's section.
5. Contains `=`: a **setting**. Split at the first `=`; the key is the text before it and the value the text after it, both with spaces removed at both ends. The key must be 1 or more lowercase ASCII letters, digits, `_` and `.`; otherwise it is the error `bad key` and the line is ignored. In a section `s`, the full key is `s.` followed by the key; otherwise it is the key.
6. Anything else: the error `syntax error`; the line is ignored.

**Values.** Before a value is stored, each `${name}` in it (from `${` to the next `}`) is replaced by the current value of the full key `name`. If that key has no value yet, it is the error `undefined ${name}` and it is replaced by nothing. A `${` with no `}` after it is kept as written, and so is everything after it. Replaced text is not scanned again. Then the full key gets the value, replacing any earlier one; it remembers the file and line that set it last.

**Includes**, checked in this order:

1. The path is outside: the error `include outside conf: <path>` (the path as written). This applies to optional includes too.
2. The normalized name is one of the files currently being read (the root, the file that included the current file, ..., the current file itself): the error `include cycle: <chain>`, where the chain is the normalized names of the files currently being read, from the root to the current file, followed by the normalized name being included, joined by ` -> `.
3. `conf/<normalized name>` does not exist: for an include, the error `missing include: <normalized name>`; for an optional include, nothing happens.
4. Otherwise the file is read at this point, completely, before the next line of the including file. A file may be included any number of times; it is then read each time.

Every error is printed as `<normalized name of the file>:<line number>: <message>` at the moment it happens.

## Output

For each root, in input order, print `== <root>` (the name as given), then:

- if the root name is outside: `error: outside conf`, and nothing else for this root;
- if `conf/<normalized name>` does not exist: `error: not found`, and nothing else for this root;
- otherwise read the root file, printing the errors as they happen. Then print each setting, sorted by full key (byte order), as `<full key>=<value> (<file>:<line>)` with the normalized name of the file and the line that set it last. Finally print `files <f> errors <e>`, where `f` is the number of times a file was read for this root (the root and every include that was read, each time it was read) and `e` the number of errors printed for this root.

## Allowed effects

The program may print, read standard input, and read files under `conf/`. Nothing else: no reading outside `conf/` (not even to check whether a file exists), no writing files, no environment variables, no clock, no randomness, no network.
