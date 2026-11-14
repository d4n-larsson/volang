# Contract: New Catalog Codes

Each code gets an entry in `docs/errors.md` and at least one golden case. No existing code changes meaning.

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0701 | `package_changed` | 65 | no | A used package's files hash differently from the manifest's pin. |
| E0702 | `package_missing` | 65 | no | A listed package's directory is missing or has no `.vo` files. |
| E0703 | `bad_manifest` | 65 | no | A malformed `vo.packages` line. |
| E0704 | `duplicate_package` | 65 | no | A package name listed twice. |
| E0705 | `package_name_clash` | 65 | no | A package name that is also a local directory next to the importing file. |
| T0010 | `doc_not_found` | 64 | no | `vo doc NAME` names no module or item. |

Existing codes in new situations:

| Code | New situations |
|---|---|
| E0201 `undefined_name` | A `__` primitive used outside the standard library. |
| E0205 `import_not_found` | `import std.nothing.x;`, or an import whose first segment is neither a file nor a listed package. |
| E0501 `missing_effect` | A call of a library function whose effect the caller does not declare (fix adds the effect). |
| R0007 `invalid_argument` | Documented misuses: `replace` with an empty pattern, `pad_left`/`pad_right` with a fill that is not one character. |
| R0010 `capability_denied` | An HTTP host outside the caller's `net` scope. |
| R0014 `replay_mismatch` | A replayed program that requests another host or port than recorded. |
