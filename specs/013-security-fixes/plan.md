# Implementation Plan: Security Fixes from Review 1

**Branch**: `013-security-fixes` | **Date**: 2026-11-16 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/013-security-fixes/spec.md`

## Summary

Make effects a real boundary and fix all 32 findings of review 1 (research R1-R7):
- **Root grant** (R1): every run-time effect check also checks `main`'s (or the test's) declared grant;
  effect parameters take the caller's grant at run time instead of a requirement set.
- **Nested function types** (R2): generic inference checks every argument and variant payload against the
  instantiated type; literal targets outside the scopes are E0503.
- **Opening files** (R3): NUL refused; one start-directory descriptor; scoped opens with
  `openat2(RESOLVE_BENEATH)` from the scope's directory, so races and renames cannot redirect them.
- **HTTP** (R4): one C framing implementation for requests and responses (ASCII heads, decoded bodies,
  a total size cap, continuous deadline and signal checks); request parsing inside the handler guard;
  header injection refused.
- **Links in the toolchain** (R5): exclusive temporary files; imports and package files contained in the
  program's tree or package directory (E0211); packages load the bytes they hashed; regular files only.
- **The rest** (R6): `sort_by` snapshot; replay program path; lexical replay checks; server state restore
  and no timeout inside the collector; stack checks in equality and rendering; DNS ID; escaped
  diagnostics.

## Technical Context

**Language/Version**: C (gnu11, freestanding) for the toolchain; vo for `std/http.vo`.

**Primary Dependencies**: Linux 5.6+ system calls (two added: `openat2`, `getrandom`); GCC for three
architectures; qemu-user 8.2; Python 3 for tests and attack scripts.

**Storage**: N/A (recordings and manifests are read more strictly).

**Testing**: one case per finding from its reproduction (golden, step, or `tests/security/` scripts);
`make security-check` (the race and rename scripts 1,000 times each, the server attacks, the review's
programs re-run); the full gate on three platforms; sanitizer runs of every memory finding.

**Target Platform**: Linux x86-64 (native), ARM64 and RISC-V (qemu), kernel 5.6 or later.

**Project Type**: compiler and runtime of a programming language, as a CLI.

**Performance Goals**: the speed gate stays under 1.10; one extra set check per effectful builtin call.

**Constraints**: Principle III (behavior of honest programs byte-identical); Principle II (R0010 reports
name the refusing grant; new code E0211 with catalog entry); the reference grows by at most 150 tokens.

**Scale/Scope**: about 1,200 lines of C changed, 150 of vo; 32 finding cases; 1 new code.

## Constitution Check

| Principle | Assessment |
|---|---|
| I. Agent-first design | **PASS.** No syntax change; the grant rule gets simpler to state ("nothing outside `main`'s clause, ever"). |
| II. Machine-readable feedback | **PASS.** Refusals are existing codes (R0010, E0503, E0702, E0703, T0002, T0008, R0007) with messages naming the cause; one new code (E0211) with a catalog entry. No field removed. |
| III. Determinism | **PASS.** The root-grant check and the stricter parsers are deterministic; DNS IDs are random but never reach program output (answers are matched, not shown). |
| IV. Test-first through the CLI | **PASS.** Every finding's reproduction is a failing case before its fix. |
| V. Single source of truth, minimal surface | **PASS.** `docs/security.md` updated (system call count, link and kernel rules), the reference states the root-grant rule, the review records each fix. |
| Implementation constraints | **PASS with a note.** Two system calls are added to the platform headers (no assembly). Linux 5.6+ becomes a stated requirement (openat2); recorded in Complexity Tracking. |

## Project Structure

### Documentation (this feature)

```text
specs/013-security-fixes/
├── spec.md, plan.md, research.md, data-model.md, quickstart.md
├── contracts/behavior.md    # the rules users see: root grant, links, HTTP framing, replay
├── contracts/codes.md       # E0211 (read by tests/catalog.py)
├── checklists/requirements.md
└── tasks.md
```

### Source Code (repository root)

```text
src/rt_effects.c, src/vm.c, src/interp.c, src/testrun.c, src/prop.c   # root grant (R1), E binding
src/types.c, src/check_generic.c, src/check_effects.c                  # nested function types, L1
src/rt_io.c, src/sys.c, src/platform/*.h                                # start fd, openat2, regular files
src/serve.c, src/net.c, std/http.vo                                     # one framing, guard, M5, DNS
src/main.c, src/edit.c, src/effects_record.c, src/packages.c, src/loader.c   # temp files, links, packages
src/rt_func.c, src/rt_render.c, src/gc.c, src/diag.c, src/replay.c      # C4, H11, L10, L7, C5, M1, L6
docs/security.md, docs/security-review-1.md, docs/reference.md, docs/errors.md
tests/cases/security/, tests/security/ (scripts), Makefile (security-check)
```

**Structure Decision**: fixes live where each mechanism is; no new source file.

## Complexity Tracking

| Item | Why needed | Simpler alternative rejected because |
|---|---|---|
| Linux 5.6+ (openat2) | The only race-free way to keep an open beneath a directory | A user-space `O_NOFOLLOW` walk does the same with more code; kept as the fallback if a platform lacks openat2 |
