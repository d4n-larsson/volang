# Specification Quality Checklist: Scoped Effects Through Package Functions

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

- The spec names vo's effects, diagnostic codes and `uses` clauses because they are the feature.
- One question was answered before writing (2026-11-23, while planning spec 049): fix the checker
  first, in its own spec.
- Defaults recorded in the spec, for the reviewer to confirm or change before planning:
  1. the rule: an effect without scopes means everything in `main`, a test or a property, and "within
     the callers' scopes" in any other function; no new syntax (no target parameters);
  2. the run-time check covers every scoped declaration on the call path, not only the running
     function and the root, so a scoped function in the middle is enforced on what it calls;
  3. a handle (`Database[E]`, `Conn[E]`) used inside a function whose scopes exclude the handle's
     target is R0010; this can refuse a program that runs today and is the one behaviour change for
     existing programs that the plan must measure (SC-003);
  4. function values follow the same rule;
  5. `process` is unchanged (E0512 everywhere);
  6. the fix of E0503 no longer offers the plain effect;
  7. the figures in SC-005 and SC-006 are estimates.
