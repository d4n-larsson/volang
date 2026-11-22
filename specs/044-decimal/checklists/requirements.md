# Specification Quality Checklist: Decimal (exact decimal numbers)

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

- The spec names vo's module and function names because they are the feature; Python's `decimal` is
  named as the reference the results are compared with.
- No clarification questions: the open choices have defaults recorded in Assumptions. For the reviewer
  to confirm or change before planning:
  1. the places are kept (1.50 stays 1.50), so `==` and map keys tell 1.5 from 1.50; `equal`,
     `compare` and `normalize` compare by value;
  2. the names: record `Decimal { units, places }`, variant `Rounding` (`half_even`, `half_up`, `down`,
     `up`, `floor`, `ceiling`), functions `parse_decimal`, `from_int`, `decimal`, `decimal_text`, `grouped_text`, `add`,
     `sub`, `mul`, `div`, `round_to`, `negate`, `absolute`, `compare`, `equal`, `is_zero`, `is_negative`,
     `normalize`, `to_int`, `to_float`, `total`, `split_evenly`, `allocate`
     (the builtins `split` and `abs`, and `round` of `std.math`, are taken);
  3. no default rounding mode: `round_to` and `div` always take one;
  4. overflow stops with R0001 and division by zero with R0002, as for `int`;
  5. `parse_decimal` is strict: no `+`, no exponent, no `.5` or `5.`, no separators, no spaces;
  6. the expenses example keeps integer cents in its JSON input and changes only inside;
  7. a decimal in JSON or SQLite is its text; those modules are not changed.
