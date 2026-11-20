# Specification Quality Checklist: Hashing and Secrets (`std.crypto`)

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-11-20
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

- As in earlier specs, the spec names vo's own functions and effects because the library is the product.
- No clarification questions: the open choices have defaults recorded in Assumptions. The ones that
  shape the feature most, for the reviewer to confirm or change before planning:
  1. the new effect is named `entropy`, separate from `random`;
  2. passwords use PBKDF2-HMAC-SHA256 at 600,000 iterations, stored as one self-describing text;
     Argon2 and scrypt are left for later;
  3. under `vo test` the bytes are reproducible (from the test's seed); under `vo run` and in built
     executables no option affects them;
  4. a recording holds the random bytes, as it holds files and variables;
  5. only SHA-256 and HMAC-SHA256; no SHA-1, MD5, encryption or public-key operations.
