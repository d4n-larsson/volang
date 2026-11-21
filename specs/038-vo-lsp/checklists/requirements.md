# Specification Quality Checklist: The Toolchain in the Editor (`vo lsp`)

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-11-21
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs)
- [x] Focused on user value and business needs
- [x] Written for non-technical stakeholders
- [x] All mandatory sections completed

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain
- [x] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic (no implementation details)
- [x] All acceptance scenarios are defined
- [x] Edge cases are identified
- [x] Scope is clearly bounded
- [x] Dependencies and assumptions identified

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria
- [x] User scenarios cover primary flows
- [x] Feature meets measurable outcomes defined in Success Criteria
- [x] No implementation details leak into specification

## Notes

- The spec names vo's commands and the Language Server Protocol because answering that protocol with
  the toolchain's own results is the feature.
- No clarification questions: the open choices have defaults recorded in Assumptions. The ones that
  shape the feature most, for the reviewer to confirm or change before planning:
  1. features in this version: diagnostics, quick fixes, hover, go to definition, references, rename,
     format, document symbols; **no completion** (it needs new checker work and is left for its own spec);
  2. the program of a file: the nearest `main.vo` at or above it (within the opened folder) when that
     program imports the file, otherwise the file alone;
  3. the server never runs the user's program and never writes a file: rename and format return edits
     for the editor to apply;
  4. limits per request: 5 seconds and the default memory limit; whole documents are sent on each
     change, and a check starts after a short pause in typing;
  5. the VS Code folder gains a few lines of code and a client library to start the server; nothing is
     published to a marketplace.
