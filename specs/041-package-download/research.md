# Research: Downloading Packages

## R1. Fetching

**Decision**: `src/fetch.c`: parse the URL (scheme `https` only, host, optional port, path and query;
a `user@` part is refused; a `#PATH` fragment is split off first and never sent, and must be a safe
relative path: not empty, not absolute, no empty or `..` component, else T0001), build
`GET PATH HTTP/1.1` with `Host`, `Connection: close`, `User-Agent: vo` and `Accept-Encoding:
identity`, call `tls_exchange`, and frame the answer with `http_frame` (both exist: they are
`std.http`'s). Status 301, 302, 303, 307 or 308 with a `Location` is followed up to five times
(absolute `https://` URLs and absolute paths on the same host); any other status than 200 is a
failure naming it. The time limit is 60 s per download (`net_cap_ms`).

**Rationale**: `tls_exchange` already caps a response at 16 MiB, which is the spec's limit, verifies
certificates, and honours `--ca-file`. `http_frame` already removes chunked framing.

**Not sent**: `Accept-Encoding: gzip`: the archive is the body as it is; a server that compresses the
transfer on top would have to be undone twice.

## R2. gzip

**Decision**: an original DEFLATE reader in `src/inflate.c`: stored, fixed and dynamic blocks,
canonical Huffman decoding by counting (the method of zlib's `puff`), output into a mapped buffer
that doubles up to the 64 MiB limit and serves as the window. The gzip wrapper checks the magic, the
method, skips the optional fields (extra, name, comment, header CRC), and after the data verifies
CRC-32 and the length. A body that does not start with the gzip magic is taken as plain tar.
Concatenated gzip members are read one after another.

**Rationale**: about 250 lines. zlib would be 10,000 for one direction of one format; `puff.c` could be
vendored but is no smaller than writing it, and the constitution prefers code that passes the
project's own warning set.

**Checked by**: a differential corpus against Python's `zlib` (every level, stored blocks, empty input,
long runs, random data) and a mutation fuzz (truncate, flip bits): every outcome is either the right
bytes or a clean failure.

## R3. tar

**Decision**: `src/untar.c` walks 512-byte headers in the inflated buffer:

- the header checksum must match; two zero blocks (or the end of the buffer) end the archive;
- sizes are octal (the base-256 form is refused: nothing that large is allowed);
- type `0` or NUL: a file; `5`: a directory; `g`: a global pax header, skipped; `x`: a pax header whose
  `path=` record names the next entry; `L`: a GNU long name for the next entry; anything else (hard
  and symbolic links `1` `2`, devices `3` `4`, FIFOs `6`, sparse and multi-volume types): the archive
  is refused;
- the path is `prefix/name` (ustar) unless a long name or pax path applies; it is refused when it is
  empty, absolute, longer than 4,096 bytes, has a component `..`, a component over 255 bytes, or a
  byte below 0x20 or 0x7f; a leading `./` is dropped;
- more than 10,000 entries: refused.

The result is a list of (path, bytes) for regular files, in archive order; a later entry of the same
path replaces an earlier one. Then: if every path has the same first component and it is not itself a
file, drop it; with `#PATH`, keep what is under `PATH/` and drop that prefix; keep paths ending in
`.vo`.

## R4. All or nothing

**Decision**: nothing is written until the archive has been fetched, inflated, walked and filtered in
memory. Then:

1. the target directory `T` (default `packages/NAME` beside the manifest) must be absent, or be the
   directory the manifest's entry for `NAME` already names and still hash to that entry's pin;
   a directory no entry names is refused (it is not ours to replace), and one that no longer matches
   its pin is refused with E0701 (someone edited it: pin it with the local form, or remove it);
2. write the files under `T.new-PID` (created with its parents);
3. `pkg_hash` it; unchanged hash and directory: remove the staging directory, report `unchanged`;
4. rename `T` to `T.old-PID` if it exists, rename the staging directory to `T`, write the manifest
   (`write_file_safely`, itself a rename), remove `T.old-PID`.

A failure before step 4 removes the staging directory and leaves everything else untouched. Step 4 is
three renames; if the manifest cannot be written, the old directory is renamed back.

## R5. The manifest line

**Decision**: `NAME DIRECTORY sha256:HASH [URL]`. The URL is written exactly as given (with its
`#PATH`), must start with `https://` and have no space or control byte. `Pkg` gains `url`. The local
form writes no URL (and drops one that was there); `pkg_write` keeps URLs of entries it does not
touch. E0703's message names the optional field.

## R6. `--restore`

**Decision**: for each entry in name order: directory present and hashing to the pin: `present`;
present and different: E0701 as `vo check` words it; missing with a URL: fetch, unpack to staging,
hash; equal to the pin: rename into place, `restored`; different: remove staging, E0701 saying the
content at the URL changed; missing without a URL: E0702. The buffers of one package (download, inflated data) are unmapped before the next is fetched. The
manifest is not written. Exit status 0
when every entry is `present` or `restored`, 65 otherwise; every entry is attempted.

## R7. A package's effects

**Decision**: after pinning, a forked child runs what `vo doc NAME.MODULE --json` runs for each module
of the package (`pkg_doc_modules`), with its output captured (spec 038's `out_capture`). The doc code
finds the manifest from the working directory (`pkg_for_doc`), so the child first changes its working
directory to the manifest's (the directory of `ENTRY`), which makes the result independent of where
the command was started; the parent
reads the `doc` records' `effects` arrays with the shared JSON reader and unites the entries as text,
sorted in the effects' canonical order then by scope. On an update the same is done for the old
directory before it is replaced, and the difference of the two sets is `gained` and `lost`. A child
that exits non-zero or prints a diagnostic gives the note `effects: could not be read (first error)`.

**Rationale**: the doc records are already the public statement of what each exported function
declares, with effect sets and constants resolved by the checker. A child process means a package that
does not check cannot end the command.

**Known limit**: this is what the package *declares*; the checker has verified the declarations cover
what the functions do (E0501), which is why a package that does not check is reported as unreadable
instead of being trusted.

## R8. Options and exit statuses

**Decision**: `vo package [--json] [--dir DIRECTORY] [--ca-file FILE] ENTRY NAME URL`;
`vo package [--json] [--ca-file FILE] --restore ENTRY`. `--dir` and `--ca-file` with the local form or
`--remove` are T0001. Exit 0 on success, 64 usage, 65 for E0701, E0702, E0705 and E0706.

## R9. Documentation budget

**Decision**: the reference's packages sentence changes `vo package ENTRY NAME DIRECTORY adds or
updates an entry` to `... NAME DIRECTORY (or an https:// archive URL, unpacked into packages/NAME)`
and adds `--restore` in five words: about 25 tokens. The command page carries the rest. E0706 gets a
catalog entry (`docs/errors.md`), which `vo explain` and the site read.

## R10. Tests

**Decision**:

- `tests/http_server.py` (and so `https_server.py`) gains `/archive/NAME`, serving the file
  `$VO_ARCHIVES/NAME` and `/moved/NAME`, redirecting to it; the test writes archives there.
- `tests/package_fetch.py` (in `make test`): groups corpus (200 archives: `tarfile` in ustar, GNU and
  pax formats, gzip levels 0 to 9, system `tar` with and without `-z`, a top folder or none, `#PATH`,
  long names, empty files, a 5 MiB file: unpacked files and hash equal a local pin of the same tree),
  hostile (path escape, absolute, symlink, hard link, device, FIFO, bad checksum, truncated tar,
  truncated gzip, wrong CRC, bomb over 64 MiB, over 10,000 entries, over 16 MiB download, no `.vo`
  file: each E0706 or E0702, and the project tree byte-identical afterwards), fetch (redirects, six
  redirects, redirect to http, `http://`, credentials, 404, a bad certificate, refused connection),
  update and `--dir`, restore (three packages; a changed upstream; one without URL), effects (none,
  scoped, gained and lost, a package that does not check), quiet (strace: `check`, `run`, `test`,
  `build`, `doc` open no socket; skipped with a message without strace), fuzz (2,000 mutated archives:
  exit status 0 or 65, never a crash or a file outside the project).
- Step cases in `tests/cases/packages/`: one fetch with `server https`, and one for E0706
  (`vo explain E0706`, a 404).
- Existing package cases unchanged (the fourth field is optional).
