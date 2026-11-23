# Specification Quality Checklist: PostgreSQL Package (`postgres`)

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-11-23
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

- The spec names vo's function names, PostgreSQL, SCRAM-SHA-256 and SQLSTATE because they are the
  feature: the package's API is defined as "the names of `std.sqlite`".
- No question was asked before writing. Defaults recorded in the spec, for the reviewer to confirm or
  change before planning:
  1. replaceable means the same API shape by convention (edit imports, the opening call, SQL); it does
     not mean one handle type for several databases or a choice at run time (vo has no interfaces);
  2. placeholders are `?` on PostgreSQL too, rewritten by the package; `$1` is refused;
  3. errors are text with a leading name as in `std.sqlite` (`constraint: 23505 ...`), not a typed
     variant, so `try` and `transaction` in a moved program still check;
  4. TLS is required by default; SCRAM-SHA-256 only;
  5. booleans are integer 0 or 1, `numeric` is text, timestamps are text with a `get_time` getter
     (the one addition to the shared names), every other type is text;
  6. `Changes.last_id` is always 0; `returning` is the portable way;
  7. lost against `std.sqlite`: the literal rule and check-time SQL checks; no in-memory database, so
     `vo test` of server code needs a server;
  8. module name `postgres.client`; the package lives in `packages/postgres/` of this repository;
  9. the figures in SC-007 are estimates: no measurement of a vo-written protocol client exists yet.
