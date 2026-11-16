# Contract: New Catalog Codes (spec 012)

Each code gets an entry in `docs/errors.md` and at least one golden case. No existing code changes meaning.

| Code | Name | Exit | Fixable | Trigger |
|---|---|---|---|---|
| E0116 | `unknown_annotation` | 65 | yes | an annotation name not in the set; fix: the closest known name. |
| E0117 | `misplaced_annotation` | 65 | no | an annotation on an item it does not apply to, or not followed by an item. |
| E0118 | `invalid_spec_id` | 65 | no | a `@spec` ID that is not `PART(/PART)*`, or `@spec()` without IDs. |
| E0119 | `repeated_annotation` | 65 | yes | two `@spec` on one item; fix: one annotation with all the IDs. |
| W0101 | `repeated_spec_id` | 0 | yes | the same ID twice in one `@spec`; fix: drop the repeat. |
