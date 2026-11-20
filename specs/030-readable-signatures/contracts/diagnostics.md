# Contract: diagnostics (spec 030)

| Code | Name | Change |
|---|---|---|
| E0509 | unknown_effect | also: a capitalized name that is neither an effect parameter nor a set in scope; fix: the nearest effect, parameter or set |
| E0513 | effect_set_cycle | new: a set that includes itself, directly or through others; the message names the chain |
| W0505 | unused_effect_set | new: a set that is not exported and that no clause of the program uses; fix: remove its declaration |
| E0501 / E0503 | missing_effect / scope_too_narrow | the fix keeps set names and adds the missing effects in canonical order (`uses console, Storage`) |
| W0501 | unused_effect | not reported for effects that come from a set, unless none of the set's effects is needed (then the set name, fix: drop it from the clause) |
