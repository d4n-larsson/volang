# Specification Quality Checklist: Security (who may call what, for `std.web` applications)

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

- The spec names `std.web`, HTTP status codes, header names and JWT because they are the feature: the
  users are developers and the product is a library.
- No names of functions or types are fixed here; the plan chooses them.
- No clarification questions: the open choices have defaults recorded in Assumptions. For the reviewer
  to confirm or change before planning:
  1. it is a standard-library module `std.security` on top of `std.web`, not a downloadable package;
  2. this version is stateless (Basic, signed tokens, application-checked keys): no sessions, cookies
     or CSRF, because worker processes share no memory;
  3. tokens are JWT with HMAC-SHA256 only;
  4. the module does not store users or lock accounts: the application supplies an account lookup and
     may act on security events;
  5. first matching rule decides, unmatched requests are refused, and the review reports hidden and
     unused rules;
  6. wrong credentials are refused with 401 on open routes too;
  7. `bearer_auth` and `api_key_auth` stay as they are.
