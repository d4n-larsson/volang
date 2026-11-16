# Installing vo

Build the vo toolchain from source with GCC and GNU make, check it with `vo version`, and put it on your `PATH`.

The whole toolchain is one static binary, `vo`, written in freestanding C with no dependencies: the
checker, formatter, test runner, compiler, virtual machine, the language reference and the standard
library are all inside it. There are no packages to install and no runtime to set up.

## Prerequisites

- Linux on x86-64, ARM64 (aarch64) or RISC-V 64.
- GCC and GNU make. The build uses nothing else: no libc headers, no other libraries.
- Optional, only for building or running vo for another architecture: the cross compilers
  `gcc-aarch64-linux-gnu` and `gcc-riscv64-linux-gnu`, and `qemu-user`.

A clean build takes a few seconds.

## Build

```sh
git clone https://github.com/d4n-larsson/volang.git
cd volang
make
bin/vo version
```

```text
vo 0.1.0 schema 1
```

`make` builds `build/vo` for the machine you are on. `bin/vo` is a small script that runs that binary;
use it from the repository instead of `build/vo` directly, because it also runs binaries built for
another architecture (see below).

`vo version` prints the toolchain's version and the version of its JSON output schema, which every
JSON record also carries as `"v":1` (see [Machine-readable output](../reference/machine-readable-output.md)).

## Put vo on your PATH

The rest of this site writes `vo`, not `bin/vo`. Either add the repository's `bin` directory to your
`PATH` (run this in the `volang` directory, and add the line to your shell's startup file to keep it):

```sh
export PATH="$PWD/bin:$PATH"
```

or copy `build/vo` to a directory that is already on your `PATH`, such as `~/.local/bin`. The binary is
self-contained, so the copy works on its own. (Do not symlink `bin/vo`: the script finds the binary
relative to its own location.) Then `vo version` works from any directory.

## Building for another architecture

`ARCH` selects the target: `x86_64`, `aarch64` or `riscv64`. Building for an architecture other than the
host's is a cross build, written to `build-<arch>/vo`, and needs that architecture's cross compiler:

```sh
make ARCH=aarch64
```

To run a cross-built binary on your machine, point `bin/vo` at it with `VO_BIN`; it runs the binary
through `qemu-user` (for example `VO_BIN=build-aarch64/vo bin/vo version`). The language behaves
byte-for-byte the same on all three architectures. `make check-tools` lists any cross compilers or
emulators that are missing.

## Editor support

An IntelliJ plugin, [volang-intellij-plugin](https://github.com/d4n-larsson/volang-intellij-plugin),
highlights `.vo` files, `vo.effects` and `vo.packages`, toggles `#` comments, matches brackets and jumps
to declarations across imports. It is built from source with Gradle; its README explains how to install
it. For other editors, `vo fmt` keeps the layout canonical, so plain-text editing works well.

## Next

Write and run [your first program](first-program.md).
