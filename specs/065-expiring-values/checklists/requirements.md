# Specification Quality Checklist: Expiring values

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-11-28
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

- The users of this feature are programmers, agents and reviewers, so the spec names vo constructs
  (`vo check`, `now_ms`, `clock`, `try`); that is the subject, not an implementation detail. The names
  of the type and its functions, and the module they live in, are left to the plan.
- Decided without asking, as Assumptions: the read takes the time as an argument (it does not read the
  clock); the wall clock is the only clock; an expiring value has no stored form of its own.
- Two decisions to review: exactly at the expiry moment a value is expired; `std.security` does not
  adopt the type in this spec (FR-011).
- FR-007 makes the plan stop if the library alone cannot hide the value. A prototype run on 2026-11-28
  showed that it can (see research.md, R1).
