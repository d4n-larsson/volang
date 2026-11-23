<!--
Sync Impact Report
- Version change: 1.5.0 → 1.6.0
- Bump rationale: MINOR. A new principle, VI (Backend Network Services), states the product's target
  and adds one question to every plan's Constitution Check. No principle or rule is removed or
  redefined; the target was decided with the user on 2026-11-23 and first applied in spec 053
  (HTML rendering removed from the core).
- Modified principles: none
- Added principles: VI. Backend Network Services
- Modified sections: none
- Removed sections: none
- Templates reviewed (read-only, not modified): plan-template.md (its Constitution Check is filled
  from this file's principles, so new plans list VI), spec-template.md, tasks-template.md; all
  compatible.
- Follow-up TODOs: none. Plans in progress: none (specs 049 to 053 are implemented; 053 already
  follows VI). The README and docs/roadmap.md state the same target.
- Earlier amendments: 1.4.1 → 1.5.0 (public-domain vendored code), 1.4.0 → 1.4.1 (pinning to a
  commit when no releases exist), 1.3.0 → 1.4.0 (rules for vendored third-party code).
-->

# vo-lang Constitution

## Core Principles

### I. Agent-First Language Design

The primary user of vo-lang is an AI coding agent that has never seen the language before.
Every language and toolchain decision is judged first by how it affects an agent's
write → feedback → fix loop.

- Each construct MUST have exactly one syntactic form. There are no optional or alternative
  spellings.
- The kind of every statement MUST be identifiable from its first token. Indentation MUST NOT
  carry meaning.
- Implicit conversions, shadowing, overloading, and hidden control flow (exceptions, implicit
  propagation) MUST NOT be added.
- When agent effectiveness and human brevity conflict, agent effectiveness wins. Explicit
  verbosity is preferred over terse shortcuts.

Rationale: an agent learns the language only from in-context documentation. Every ambiguity or
alternative spelling is an opportunity to guess wrong.

### II. Machine-Readable Feedback (NON-NEGOTIABLE)

Everything the toolchain reports MUST also be available as structured output that follows a
versioned, documented schema.

- Every diagnostic MUST carry a stable code, the exact span, the covered source text, and a
  one-sentence message. It MUST also carry a mechanical fix whenever exactly one correction
  is unambiguous.
- Error codes MUST NOT be reused or redefined. Retired codes stay in the catalog, marked
  retired.
- Within a schema version, fields MAY be added but MUST NOT be removed or renamed. Removing or
  renaming a field requires a new schema version.
- All independent errors MUST be reported in one pass, and cascading errors MUST be suppressed.

Rationale: diagnostics are the main interface between the toolchain and its primary user.

### III. Determinism and Defined Behavior

- Given the same source, arguments, and input, a program MUST produce byte-identical output and
  exit status. Nondeterminism enters only through explicitly named builtins.
- Every operation MUST have defined behavior. Overflow, division by zero, out-of-bounds access,
  resource exhaustion, and timeouts MUST stop execution with a structured runtime error report.
- No toolchain invocation may end in a crash, a hang (when a time limit is given), or a silent
  failure. Every exit path produces a documented exit status and a report.

Rationale: agents cannot debug nondeterministic or silent failures, and they treat undefined
behavior as a correct result.

### IV. Test-First Through the CLI (NON-NEGOTIABLE)

- Every behavior change MUST first land as a golden test case in `tests/cases/` (source plus
  expected stdout, JSONL, and exit status), and that case MUST fail before the implementation
  is written.
- Every error and runtime code in the catalog MUST have at least one golden case that triggers
  it.
- Every example in the language reference MUST be executed and its output verified by the test
  suite.
- The malformed-input fuzzer and the determinism check MUST pass before a change is complete.

Rationale: all behavior is observable through the CLI, so CLI-level golden tests are the
cheapest complete specification and also exercise the implementation beneath them.

### V. Single Source of Truth, Minimal Surface

- The language reference and the error catalog are product inputs embedded in the binary. A
  change to syntax, builtins, CLI, schema, or codes MUST update them, and the matching
  `contracts/` documents, in the same change.
- The complete language reference MUST stay within 15,000 tokens. A feature that does not fit
  the budget does not ship until something else is simplified.
- Build only what a current requirement needs (YAGNI). A deliberate shortcut with a known limit
  (for example, the arena with no GC) MUST be marked in code with a `ponytail:` comment naming
  the ceiling and the upgrade path.

Rationale: every feature costs reference tokens, which every agent pays for on every task, and
costs toolchain code, which is expensive to write and to change.

### VI. Backend Network Services

vo is for backend network services written by AI coding agents and reviewed by people: APIs,
integrations, workers and services behind a frontend. The standard library and the toolchain serve
that target.

- Every spec MUST answer, in its plan's Constitution Check, how it makes an agent-written network
  service easier to write, review or operate. A feature that answers none of the three does not
  enter the core; it may be a package.
- Rendering user interfaces (HTML pages and templates, client-side scripts, graphical windows) is
  out of scope for the core: pages belong to the frontend in front of a service. HTTP itself
  (requests, responses, form bodies, cookies, sessions) is in scope.
- Open gaps in `docs/roadmap.md` are ordered by the question above.

Rationale: checked effects, scopes, review records and record/replay pay off most where a program
holds credentials and talks to other systems; a clear target keeps the language small and tells
agents and reviewers what vo is for. Decided 2026-11-23 (spec 053).

## Implementation Constraints

- The toolchain is written in C (C11), freestanding: it links no C library and uses only the
  Linux system call interface. It is shipped as a single static Linux user-space ELF per
  platform, with no runtime dependencies beyond the Linux syscall ABI. The language reference,
  the error catalog and the standard library are embedded in it.
- Supported platforms: Linux on x86-64, ARM64 (AArch64) and RISC-V 64 (RV64IMAFD plus Zicsr,
  lp64d). Every supported platform MUST build and pass the full quality gate.
- Hand-written assembly is permitted only in (a) the per-platform layer (system calls, register
  and stack access for the collector, non-local exits), at most 300 lines per platform, and
  (b) toolchain files not yet ported from assembly to C. Every assembly function MUST begin with
  a C prototype comment followed by its global label, and MUST pass the SESI lint for its
  architecture where a SESI pack exists (riscv64 today). A report may be left in place only when
  it is shown to be a false positive, and the reason MUST be recorded in the change.
- C code MUST NOT rely on undefined behavior, or on implementation-defined behavior that differs
  between supported platforms. Arithmetic whose result the language defines (overflow, division
  by zero, the minimum integer divided by -1) MUST be computed with explicit checks, never left
  to the compiler or the hardware.
- Floating-point code MUST round every operation to IEEE 754 double: no contraction into fused
  multiply-add, no extended precision, no value-changing optimizations. Results MUST be
  bit-identical on every supported platform.
- Every C source MUST compile with zero warnings under the project's strict warning set, with
  warnings treated as errors. An instrumented build (memory-error and undefined-behavior
  detection) MUST exist, and the full test suite MUST pass under it with no reports. These are
  the per-edit checks for C code, as the SESI lint is for assembly.
- While toolchain code is still being ported from assembly and no native build exists, the
  instrumented build is the build that traps on undefined behavior and out-of-bounds array
  access, run together with the collector stress build (which poisons reclaimed memory).
  Address sanitizing (memory errors in any memory, including the collected heap) is required
  from the first native build on.
- Third-party source code (vendored code) MAY be built into the toolchain only under
  `third_party/`, pinned to a released version or, when the upstream publishes no releases, to a
  commit. The archive checksum (for a commit: the SHA-256 of each kept file), the license and the
  list of kept files are recorded in `third_party/<name>/VENDOR.md`. Vendored files are kept unmodified; a
  necessary change is a patch file in `third_party/<name>/patches/` with its reason. Licenses
  MUST be permissive (Apache-2.0, MIT, BSD), or the code MUST be dedicated to the public domain
  with no conditions (as SQLite is); `VENDOR.md` records the license or the dedication.
- Vendored code is built under the same constraints as the rest of the binary (freestanding, no
  system libraries, `-ffp-contract=off`, no assembly, identical results on every supported
  platform) and MUST pass the instrumented build. It is exempt from the project's strict warning
  set and from the rules that assume vo's authors wrote the C code: it compiles with its
  upstream's warnings, without warnings treated as errors, and a new warning on an update is
  reviewed. The other C rules in this section apply to vo's own code.
- A security release of a vendored library MUST be applied in the spec cycle in which it
  appears.
- Developer scripts (shell, Python) are allowed for building, testing, and benchmarking only.
  They MUST NOT become part of the shipped product.
- `bin/vo` runs the native build on a supported host. RISC-V builds (the mixed toolchain while
  files are being ported, and the last assembly release kept as a behavioral oracle) run through
  `qemu-riscv64` on other hosts. Performance targets are measured on the native x86-64 build once
  it exists; until then, and for RISC-V regressions, under `qemu-riscv64`.

## Development Workflow and Quality Gates

- Features follow the Spec Kit flow: specify → (clarify) → plan → tasks → implement. A plan
  MUST pass the Constitution Check before tasks are generated.
- A change is complete only when `make test` passes (golden cases, reference examples, catalog
  coverage), the fuzzer and determinism checks pass, and no SESI lint report on assembly is left
  unexplained. A change to C code is complete only when it also builds with zero warnings and the
  full suite passes under the instrumented build.
- A change to the grammar, CLI, output schema, or error catalog MUST be reviewed against
  Principles I, II, and V. Any increase in the size of the language reference MUST be stated
  explicitly in the change.
- Complexity beyond these principles MUST be justified in the plan's Complexity Tracking table,
  including the simpler alternative that was rejected and why.

## Governance

This constitution takes precedence over all other project practices. If it conflicts with any
other document, this constitution wins until it is amended.

- **Amendments**: Edit this file through `/speckit-constitution`. Each amendment updates the
  Sync Impact Report, the version, and the Last Amended date. Plans affected by the change
  MUST have their Constitution Check re-evaluated.
- **Versioning**: Versions follow semantic versioning. MAJOR is for removing or redefining a
  principle in a backward-incompatible way, MINOR is for adding a principle or section or for
  materially expanding guidance, and PATCH is for clarifications and wording changes.
- **Compliance**: Every `/speckit-plan` runs the Constitution Check gate, and every change is
  reviewed against the quality gates above before it is considered complete.

**Version**: 1.6.0 | **Ratified**: 2026-11-13 | **Last Amended**: 2026-11-23
