# Specification Quality Checklist: HTML Pages (`std.html`)

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

- The spec names vo's functions, HTML element and attribute names and HTTP headers because they are
  the feature.
- No question was asked before writing. Defaults recorded in the spec, for the reviewer to confirm or
  change before planning:
  1. `std.html` in the standard library (checked by `vo check`), not a package;
  2. templates are HTML literals or constants with `format`'s `{}` holes; no loops, conditions or
     includes inside them: composition is passing `Html` values into holes;
  3. escaping by the hole's place (text, quoted attribute, URL attribute); places where no value can be
     safe (`<script>`, `<style>`, `on...`, `style`, unquoted attributes, names, comments) are check
     errors;
  4. `Html` cannot be built with `new` or from raw text: no escape hatch;
  5. `std.web` gains `html_reply` (with nosniff and a default content security policy) and binds
     URL-encoded form bodies and `GET` query strings into input records like JSON;
  6. out of scope: cookies and sessions, static files, multipart uploads, template files, running vo in
     the browser;
  7. the reference has 1 token of room: the plan must shorten other parts to pay for this.
