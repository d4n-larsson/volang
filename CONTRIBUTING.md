# Contributing to vo

vo is developed in specifications: each change to the language, the library or the toolchain has a
folder under [`specs/`](specs/) with what it is for, how it is built and what was measured. The rules
every change follows are in the constitution, [`.specify/memory/constitution.md`](.specify/memory/constitution.md).
Read it first; it is short.

## Building and testing

```sh
make                      # build/vo for this machine, in a few seconds
make quick                # the tests of the areas your change touches, between edits
make test                 # the whole suite on this machine (about 8 minutes)
make gate                 # everything: three architectures, sanitizer and stress builds (long)
make check-tools          # lists the cross compilers and tools the gate needs
```

GCC and GNU make are enough to build. `make test` also needs Python 3. Run one suite at a time: several
of them measure time.

## What a change needs

- **A test first.** Behavior is specified by golden cases in `tests/cases/` (a program, the command, the
  expected output and exit status). Add the case, see it fail, then write the code.
- **Diagnostics with a code.** A new mistake the toolchain reports gets a stable code, a catalog entry in
  `docs/errors.md` with a broken and a fixed example, and a mechanical fix whenever exactly one
  correction exists. Codes are never reused.
- **The reference kept true and small.** `docs/reference.md` is what agents read, and it is embedded in
  the binary. A change to syntax, builtins or commands updates it in the same change, and it must stay
  under 15,000 tokens (`python3 tests/ref_examples.py`).
- **Determinism.** The same program and input give the same output on x86-64, ARM64 and RISC-V. Anything
  that reaches outside the program gets an effect and is recorded and replayed.
- **One way to write each thing.** vo is small on purpose. A feature that only adds a second spelling is
  declined; see "Out of scope for now" in `docs/roadmap.md`.

Small fixes can go straight to a pull request with their test. Anything that changes the language,
the library's surface or a command starts as a specification: open an issue describing the problem
before writing code.

## Code

- The toolchain is freestanding C11 with no libc, under a strict warning set; assembly lives only in
  `src/platform/`. `make lint` checks both.
- The standard library is vo, in `std/`, one module per file, with a documented, runnable example for
  every exported function (`python3 tests/doc_conformance.py`).
- Third-party code is vendored under `third_party/` with a permissive license, a pinned version and a
  checksum; see the constitution before proposing more.
- The time zone data in `third_party/tzdata/` is updated before every release:
  `tools/vendor_tzdata.sh VERSION SHA512` with the newest release on <https://www.iana.org/time-zones>,
  then `make test`.

## License

vo is under the MIT license ([`LICENSE`](LICENSE)). By contributing you agree that your contribution is
under the same license.
