# Specification Quality Checklist: Time zones (local time in std.time)

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

- The spec names vo's function names and the TZif file format because they are the feature; Python's
  `zoneinfo` is named as the reference the results are compared with.
- Two questions were asked and answered before the spec was written:
  1. the tz database is embedded in the binary (not read from the system);
  2. the machine's zone is in scope, under `clock`.
- Defaults recorded in Assumptions, for the reviewer to confirm or change before planning:
  1. names: `Zone`, `load_zone`, `zone_names`, `tzdata_version`, `fixed_zone`, `zone_from_bytes`,
     `offset_at`, `abbreviation_at`, `in_zone`, `from_local`, `format_in`, `local_zone`;
  2. `from_local` returns a typed error for skipped and repeated times instead of choosing;
  3. `local_zone` reads `TZ` (a database name only) and then `/etc/localtime`, under `clock`, never
     fails, and is UTC with a fixed clock;
  4. `format_in` writes `+00:00`, never `Z`, and offsets with seconds where history has them;
     `parse_time` learns to read those;
  5. `DateTime` is unchanged; offsets are milliseconds;
  6. version 1 and leap-second zone files are refused;
  7. `vo version` shows the tz release.
