# Results: Typed Errors

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | pending | |
| SC-002 | pending | |
| SC-003 | pending | |
| SC-004 | pending | |
| SC-005 | pending | |
| SC-006 | pending | |
| SC-007 | pending | needs isolated agent runs scheduled with the user |

## Verification

| Check | Result |
|---|---|

## Deviations

- **std.web's error type is per operation, not per app** (research R9): `typed(op, answer)` turns a typed
  operation into an ordinary one, so `App` and `Route` keep their types.
