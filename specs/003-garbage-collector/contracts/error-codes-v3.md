# Contract: Code Changes in v3

No new codes. Two existing codes change.

| Code | Name | Change |
|---|---|---|
| R0006 | out_of_memory | **Meaning:** the program holds more live data than the limit, measured right after a reclamation pass. It no longer means "allocated a lot in total".<br>**Message:** "The program holds X of live data, over the Y limit."<br>**JSON:** gains `in_use` and `limit`.<br>**Catalog advice:** keep less data alive, or raise the limit with `--max-memory`. |
| T0003 | internal_error | **Adds one message:** "a reclaimed value was used". It is reported only by the stress build, or when a reclamation fault is detected, and is not fixable by the program. |

The catalog example for R0006 must still trigger it (a program whose live data grows without bound), and the Fixed version must run within the limit.
