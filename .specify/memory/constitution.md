<!--
Sync Impact Report
- Version change: 1.0.0 → 1.1.0
- Bump rationale: MINOR. The Implementation Constraints target changes from RV32IMAFD/ilp32d to
  RV64IMAFD/lp64d. No principle was added, removed, or redefined, and no artifact written under
  1.0.0 is invalidated (no assembly exists yet).
- Modified sections: Implementation Constraints (ISA, ABI, SESI pack name, emulator)
- Added sections: none
- Removed sections: none
- Templates reviewed (read-only, not modified): plan-template.md, spec-template.md,
  tasks-template.md, all compatible
- Follow-up TODOs: none. specs/001-vo-lang-core/plan.md Constitution Check re-evaluated against
  1.1.0 (it no longer records a vacuous PASS)
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
cheapest complete specification and also exercise the assembly beneath them.

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
costs assembly, which is expensive to write and to change.

## Implementation Constraints

- The toolchain is written in RISC-V assembly: RV64IMAFD plus Zicsr, the lp64d calling
  convention, GNU as syntax. It is shipped as a single static Linux user-space ELF with no
  libc and no runtime dependencies beyond the Linux syscall ABI.
- Every assembly function MUST begin with a C prototype comment followed by its global label.
  Every function MUST pass the SESI riscv64 lint. A report may be left in place only when it is
  shown to be a false positive, and the reason MUST be recorded in the change.
- Developer scripts (shell, Python) are allowed for building, testing, and benchmarking only.
  They MUST NOT become part of the shipped product.
- Developers on non-RISC-V hosts run the toolchain through `qemu-riscv64` via `bin/vo`.
  Performance targets are measured in that environment.

## Development Workflow and Quality Gates

- Features follow the Spec Kit flow: specify → (clarify) → plan → tasks → implement. A plan
  MUST pass the Constitution Check before tasks are generated.
- A change is complete only when `make test` passes (golden cases, reference examples, catalog
  coverage), the fuzzer and determinism checks pass, and no SESI lint report is left
  unexplained.
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

**Version**: 1.1.0 | **Ratified**: 2026-11-13 | **Last Amended**: 2026-11-13
