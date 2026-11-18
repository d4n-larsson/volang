# Feature Specification: Binary data

**Feature Branch**: `021-bytes`

**Created**: 2026-11-18

**Status**: Draft

**Input**: User description: "Binary data for vo: a `bytes` type, because `str` must be UTF-8 and so images,
archives, hashes and binary protocols cannot be handled. Roadmap gap "Binary data"; also lifts spec 017's
"output is not UTF-8" error from std.process. Covers: the type and its literal-free construction,
length/indexing/slicing/concatenation/comparison, conversion to and from str (UTF-8 checked) and from/to
list[int], reading and writing files as bytes, process output as bytes, hex/base64 text. Prepares for
later crypto/compression from vendored libraries (not in scope). Must fit the reference budget and keep
determinism."

Background: the roadmap's next gap after spec 020. vo has one sequence-of-bytes type, `str`, documented
as "immutable UTF-8 text". In practice the rule is enforced in some places and not others:
- `std.process.run` refuses output that is not UTF-8 (`err("output is not UTF-8")`, spec 017), and
  `std.fs` leaves non-UTF-8 names out of listings;
- `read_file` and `read_all` accept any bytes, so a `str` can hold invalid UTF-8 today; `chars` then
  counts each invalid byte as a character, and `print` passes it through.

So a program cannot handle an image, an archive, a checksum or a binary protocol reliably: it either
gets an error or a `str` whose text operations give meaningless results. An agent that needs a file's
SHA-256, a PNG's dimensions or a program's binary output has no correct way to write it.

What this spec must keep:
- **Principle I**: one way to do each thing; text is `str`, binary is `bytes`, and nothing converts
  between them implicitly.
- **Principle III**: the same bytes in give the same bytes out on every platform; every operation has
  defined behavior (out of range is a runtime error, invalid UTF-8 is a defined result).
- **Effects**: reading and writing bytes needs the same effects and scopes as reading and writing text.
- **Record and replay**: runs that read binary input or process output replay exactly.
- **The reference budget**.

## Clarifications

### Session 2026-11-18

- Q: Does `str` become strictly UTF-8? → A: Yes. Every place where data from outside becomes a `str`
  checks it: `read_file`, `read_line` and `env` (which return a `result`) give an `err` naming the first
  invalid byte offset; `read_all` and `args` (which do not) stop with R0007 naming it; HTTP bodies read
  as text and process output read as text give an `err`. Invalid UTF-8 can then exist only as `bytes`,
  and every text operation sees valid text. This changes behavior for programs that read non-UTF-8
  files as text today; the change states it.
- Q: Are HTTP bodies in scope? → A: Yes. `std.http` can send and receive bodies as bytes (`get`/`post`
  responses and request bodies, and `serve` requests and responses), alongside the text bodies.

## User Scenarios & Testing *(mandatory)*

The users are the same as in specs 001–020: AI coding agents that write vo programs from the reference
and `vo doc`, and the people who review and run those programs.

### User Story 1 - Read and inspect a binary file (Priority: P1)

An agent writes a program that reads a PNG file and prints its width and height, or reads a file and
checks its magic number to tell a ZIP from a gzip archive. It reads the file as bytes, reads individual
bytes as integers, and slices out the header.

**Why this priority**: this is the gap. Without it, binary files are unreadable or silently corrupted
into text.

**Independent Test**: golden cases that read committed binary files (a small PNG, a gzip file, a file
with every byte value 0–255) and print values computed from their bytes.

**Acceptance Scenarios**:

1. **Given** a file whose bytes are `89 50 4E 47 ...` (a PNG), **When** a program reads it as bytes and
   prints the first four as integers, **Then** it prints `137 80 78 71`.
2. **Given** a file containing all 256 byte values, **When** a program reads it as bytes, **Then** its
   length is 256 and byte `i` is `i` for every `i`.
3. **Given** a byte index outside the data, **When** it is read, **Then** the program stops with the
   existing out-of-range runtime error (R0003), as for lists.

---

### User Story 2 - Write binary data (Priority: P1)

An agent builds binary data (a small file format, a protocol message) from integers and from encoded
text, and writes it to a file.

**Why this priority**: reading without writing covers only half of binary formats and protocols.

**Independent Test**: golden step cases that build bytes, write them to a file, and compare the file's
bytes with the expected ones.

**Acceptance Scenarios**:

1. **Given** a list of integers each from 0 to 255, **When** a program converts it to bytes and writes
   it, **Then** the file contains exactly those bytes.
2. **Given** an integer outside 0–255 in that list, **When** it is converted, **Then** the program stops
   with R0007 naming the value and its position.
3. **Given** text, **When** it is converted to bytes and written, **Then** the file holds its UTF-8
   encoding, byte for byte.
4. **Given** two byte sequences, **When** they are joined and sliced, **Then** the results are new byte
   sequences and the originals are unchanged.

---

### User Story 3 - Run programs with binary output (Priority: P1)

An agent runs a program that prints binary data (an image converter, `gzip -c`, `git cat-file` of a
binary blob) and receives its output as bytes instead of `err("output is not UTF-8")`.

**Why this priority**: it lifts spec 017's documented limit; programs that call tools often meet binary
output.

**Independent Test**: step cases that run a helper program writing every byte value to stdout and
compare the received bytes.

**Acceptance Scenarios**:

1. **Given** a granted program that writes the bytes 0–255 to its output, **When** a vo program runs it
   asking for bytes, **Then** it receives exactly those 256 bytes and the exit code.
2. **Given** the same program run asking for text, **When** its output is not UTF-8, **Then** the result
   is still `err("output is not UTF-8")`, unchanged from spec 017.

---

### User Story 4 - Convert between bytes and text forms (Priority: P2)

An agent prints a checksum as hex, embeds binary data in JSON as base64, decodes a base64 field from an
API response, or turns received bytes into text when they are UTF-8.

**Why this priority**: hex and base64 are how binary data travels through text (JSON, HTTP headers,
logs, golden outputs); without them every program writes its own encoder.

**Independent Test**: golden cases with the RFC 4648 test vectors for base64 and hex, round trips of
every byte value, and the invalid inputs.

**Acceptance Scenarios**:

1. **Given** the bytes of `"foobar"`, **When** encoded as base64, **Then** the text is `"Zm9vYmFy"`, and
   every RFC 4648 test vector matches.
2. **Given** text that is not valid hex or base64, **When** it is decoded, **Then** the result is an
   `err` naming the first invalid position, never a partial value.
3. **Given** bytes that are valid UTF-8, **When** converted to text, **Then** the result is `ok` with
   that text; **given** bytes that are not, **Then** the result is an `err` naming the first invalid
   byte offset.

---

### User Story 5 - Compare and test binary data (Priority: P2)

An agent writes tests that compare received bytes with expected bytes, and a failure shows where they
differ.

**Why this priority**: agents verify their work through tests (spec 018); a failing comparison of two
large byte sequences must point at the difference.

**Independent Test**: golden test cases with `expect_eq` on equal and differing byte sequences.

**Acceptance Scenarios**:

1. **Given** two byte sequences that differ at offset 1,000, **When** `expect_eq` fails, **Then** the
   report names offset 1,000 and shows the differing bytes in hex, not the whole sequences.
2. **Given** bytes printed or shown in a runtime error's locals, **When** they are rendered, **Then**
   they appear in one fixed, readable form (hex, truncated for long values with the length stated).

### User Story 6 - Download and serve binary content over HTTP (Priority: P2)

An agent downloads an image or an archive over HTTP(S) and saves it, posts a binary upload, or writes a
`serve` handler that returns an image or accepts a binary upload.

**Why this priority**: with `str` strictly UTF-8, a binary body would otherwise be an error; downloads are
a common agent task.

**Independent Test**: step cases against the local HTTP and HTTPS test servers with a route that returns
every byte value, a route that echoes a binary request body, and a `serve` handler answering with
binary data, each compared byte for byte; record and replay of each.

**Acceptance Scenarios**:

1. **Given** a route that returns the bytes 0–255, **When** a program gets it as bytes and writes it to a
   file, **Then** the file holds exactly those 256 bytes.
2. **Given** the same route got as text, **When** the body is not UTF-8, **Then** the result is an `err`
   saying the body is not UTF-8 (and that the bytes version reads it), not a corrupted string.
3. **Given** a `serve` handler that answers with a bytes body, **When** a client requests it, **Then** the
   client receives exactly those bytes, with the right length header.
4. **Given** a recorded run that downloaded binary data, **When** it is replayed, **Then** the program
   receives the same bytes without the network.

---

### User Story 7 - Text stays text (Priority: P2)

A reviewer can rely on every `str` being valid UTF-8: a program that reads a file that is not UTF-8 as
text gets a clear error at the read, naming the offset, instead of text operations quietly giving
meaningless results later.

**Why this priority**: it is what makes `str` and `bytes` two distinct things (Principle I) and every
text operation defined (Principle III).

**Independent Test**: golden cases reading a non-UTF-8 file, standard input, argument and environment
variable as text.

**Acceptance Scenarios**:

1. **Given** a file containing `a 0xFF b`, **When** a program reads it with the text function, **Then**
   the result is an `err` naming byte offset 1; reading it as bytes succeeds.
2. **Given** standard input that is not UTF-8, **When** a program calls the text function that cannot
   return an error, **Then** it stops with R0007 naming the offset; the bytes version reads it.
3. **Given** a UTF-8 file, **When** it is read as text, **Then** nothing changes from today.

### Edge Cases

- **Empty bytes**: length 0; slicing, joining, encoding and decoding work; base64 of empty is `""`.
- **Every byte value**: 0x00 to 0xFF round-trip through files, process output, lists, hex and base64.
- **Zero bytes inside**: a `0x00` byte is an ordinary byte, never a terminator.
- **Large data**: a 100 MB file reads, slices and writes within the existing memory limit and time
  limits; reading beyond the memory limit is R0006, as for text.
- **Equality and order**: two byte sequences are equal when their lengths and bytes are equal; ordering
  (for sorting and ordered maps) is byte by byte, shorter first when one is a prefix of the other.
- **Map keys**: whether `bytes` can be a map key follows the existing rule (keys are `int` or `str`
  only); a program converts to hex text to use bytes as a key.
- **No implicit conversion**: `bytes + str`, `print(b)` and `str == bytes` are type errors with a fix
  where one conversion is unambiguous.
- **Replay**: a recording of a run that read binary files or binary process output replays the same
  bytes; recordings stay valid JSON (the bytes are encoded).
- **Strict boundaries**: a byte order mark, an overlong encoding, a surrogate code point (U+D800–U+DFFF)
  and a truncated sequence at the end are all invalid UTF-8 (the standard's definition); the offset is
  that of the first byte of the invalid sequence.
- **Recordings made before this spec** that recorded non-UTF-8 text input replay with the new error, the
  same as a live run would.
- **Mutation**: bytes are immutable, like `str`; building large data goes through a list of integers
  and one conversion, or repeated joining.

## Requirements *(mandatory)*

### Functional Requirements

**The type**

- **FR-001**: vo MUST have a built-in type `bytes`: an immutable sequence of integers from 0 to 255.
- **FR-002**: `bytes` MUST support `len`, indexing to an `int` (R0003 out of range), slicing (R0003 out
  of range), joining two values with `+`, `==`/`!=`, and ordering (`<` etc., `sort`, `min`, `max`,
  ordered collections) byte by byte.
- **FR-003**: There MUST be no `bytes` literal; values are made by conversion (FR-004, FR-005), by reading
  (FR-007, FR-008), or as the empty value of the type.
- **FR-004**: Conversions with `list[int]` MUST exist both ways; an element outside 0–255 is R0007
  naming its index and value.
- **FR-005**: Conversions with `str` MUST exist both ways: text to bytes is its UTF-8 encoding and cannot
  fail; bytes to text returns a `result`, `err` naming the first invalid byte offset when the bytes are
  not UTF-8.
- **FR-006**: Nothing converts implicitly between `bytes` and `str` or `list[int]`.

**Input and output**

- **FR-007**: A program MUST be able to read a whole file as bytes and write or append bytes to a file,
  under exactly the effects and scopes of the text versions (`file.read`, `file.write`).
- **FR-008**: A program MUST be able to read all of standard input as bytes, under `input`, and write
  bytes to standard output, under `console`.
- **FR-009**: `std.process` MUST be able to return a program's output and error output as bytes, under
  the existing `process` effect; the text version keeps returning `err("output is not UTF-8")`.
- **FR-010**: Every `str` MUST be valid UTF-8. Where outside data becomes a `str`, it MUST be checked:
  `read_file`, `read_line`, `env`, HTTP text bodies and process text output return an `err` naming the
  first invalid byte offset; `read_all` and `args` stop with R0007 naming it. The reference and the
  catalog state the check, and the change lists the behavior it changes.
- **FR-011**: `std.http` MUST be able to receive response bodies as bytes and send request bodies as
  bytes, under the existing `net` effect and scopes, over HTTP and HTTPS; `serve` handlers MUST be able
  to read request bodies as bytes and answer with bytes bodies, under `serve`. The text versions return
  an `err` for a body that is not UTF-8 instead of a corrupted string. Record and replay cover bytes
  bodies.

**Text forms**

- **FR-012**: Encoding to and decoding from lowercase hex and RFC 4648 base64 (standard alphabet, with
  padding) MUST exist; decoding returns a `result` whose `err` names the first invalid position;
  decoding accepts uppercase hex.

**Rendering, testing, determinism**

- **FR-013**: Where vo shows a value (`expect_eq` reports, runtime error locals, traces, `vo test`
  output), `bytes` MUST appear as hex, truncated with the length stated beyond a fixed size, the same on
  every platform.
- **FR-014**: `expect_eq` on two differing `bytes` values MUST report the first differing offset and the
  bytes around it.
- **FR-015**: Record and replay MUST reproduce binary input exactly; recordings stay valid JSON.
- **FR-016**: Property tests (spec 005) MUST be able to generate `bytes` arguments, shrinking toward
  shorter and smaller values.

**Documentation**

- **FR-017**: The reference MUST document the type and its operations, stating the size increase; it
  MUST stay within 15,000 tokens. Library functions (hex, base64, process bytes) are documented by
  `vo doc` with checked examples.

### Key Entities

- **bytes**: an immutable sequence of integers 0–255; length, element access, ordering, conversions.
- **Text forms**: hex (two lowercase digits per byte) and base64 (RFC 4648, standard alphabet, padded).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Every byte value 0–255 round-trips unchanged through files, standard input and output,
  process output, `list[int]`, hex and base64: verified by golden and step cases on all three platforms.
- **SC-002**: All RFC 4648 base64 and hex test vectors pass, and every invalid-input case returns an
  `err` naming the right position.
- **SC-003**: A program computes the dimensions of a committed PNG and the type of a committed gzip file
  correctly, written only from the reference and `vo doc`.
- **SC-004**: Reading, slicing and writing a 100 MB file takes at most twice as long as reading and
  writing it as text, and stays within the default memory limit.
- **SC-005**: Every new error path (out of range, element out of 0–255, invalid UTF-8, invalid hex,
  invalid base64) has a golden case.
- **SC-006**: The reference stays within 15,000 tokens, and the increase is stated.
- **SC-007**: No text operation can see invalid UTF-8: every input path that creates a `str` has a golden
  or step case showing the check, and a fuzz run of 10,000 random byte inputs through each path never
  yields a `str` that is not valid UTF-8.
- **SC-008**: A binary download of 10 MB over HTTP and HTTPS arrives byte-identical and takes at most
  as long as the same download as text.

## Assumptions

- `bytes` is a new built-in type name and keyword, like `str`; programs using `bytes` as a variable or
  function name break, which the change states (none of the examples or benchmark tasks do).
- Hex and base64 live in the standard library (a module), not as builtins, to save reference tokens.
- Only standard base64 with padding: base64url and unpadded variants are out of scope until a program
  needs them.
- No streaming or partial reads: whole files and whole outputs, as for text today.
- Hashing, encryption and compression are out of scope; they come later from the vendored libraries,
  taking and returning `bytes`.
- Integers in `list[int]` conversions use the existing `int`; there is no separate byte or unsigned type.
- Directory names that are not UTF-8 (spec 017) stay left out of listings; listing names as bytes is out
  of scope.
- File paths, environment variable names, program names and HTTP headers stay `str` (and so UTF-8); a
  non-UTF-8 header value is an `err`, as a non-UTF-8 text body is.
- Programs whose input today happens to be non-UTF-8 text (for example Latin-1 files) must read bytes
  and decode them themselves; vo offers no other text encodings.
