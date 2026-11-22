# Specification Quality Checklist: Iteration (two-variable `for`, list helpers)

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-11-22
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

- The spec shows vo syntax because the syntax is the feature.
- No clarification questions: the open choices have defaults recorded in Assumptions. For the reviewer
  to confirm or change before planning:
  1. `for i in 0..n by 2` is left out: no loop in `std/` or `examples/` steps by other than 1;
  2. the two-variable loop goes over maps, ordered maps, lists and deques, not text or ranges;
  3. adding or removing entries during a map loop is defined behaviour, not an error;
  4. the old patterns get a warning with a fix (one new warning), and `std/` and `examples/` are
     moved to the new form; for the fix to be safe, a two-variable loop over a list runs once per
     index the list had at the start (unlike `for x in xs`, which follows the list);
  5. the list helpers are a library module written in vo: contains, index, reversed copy, descending
     sort by key, smallest and largest by key, sum of ints, group by key; no `zip`, `first`, `last`;
  6. the reference has 95 tokens left, so text the feature makes obsolete is replaced rather than
     added to.
