# Specification Quality Checklist: Workers

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

- The spec speaks of processes because isolation by process is the feature's promise to the user, as
  it is for `concurrent_map`.
- Five edge cases are left to the plan on purpose (workers inside workers, tasks and serve workers;
  SIGKILL of the program; time limits under a fixed clock); each must come out as defined behaviour.
- No clarification questions: the open choices have defaults recorded in Assumptions. For the reviewer
  to confirm or change before planning:
  1. a worker is started with one function that holds its link (not the review's setup and handler);
  2. a failing worker does not stop the program: the failure is an error value where the program
     talks to it, and makes the run fail at its end if nobody looked;
  3. workers never outlive the program; there is no detach;
  4. output of several processes may interleave by line; only a recording fixes the order;
  5. waiting on several workers needs them to have one message type;
  6. a deadline for one call runs that call in a process of its own, so it works on a copy: what the
     call changes in memory is lost, only its result comes back;
  7. no clients for server-sent events or WebSockets in this feature;
  8. no new effect for starting a worker.
