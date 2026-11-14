# Contract: Porting One File

The procedure every migration step follows (research R1, R2, R10, R13, R14).

1. **Audit** the assembly file for hardware-defined behavior (R13: division, shift counts, float
   conversion, wrapping, NaN bits) and for native-stack recursion limits (R7). Note findings.
2. **Write the C file** `src/<name>.c`: the same global symbols with the prototypes from the
   assembly's prototype comments; layouts through `build/defs.h` structs; heap references held
   across allocations registered with `GC_ROOT` (R4); `ponytail:` comments carried over where the
   shortcut remains.
3. **Delete the assembly file** (or the ported functions of a large one); regenerate
   `build/protos.h`.
4. **Gate**: `make gate`. A failure is fixed in this step; behavior changes are not allowed
   (FR-017), so an expected-output change is a bug unless it is an R4 memory-figure consequence,
   which is recorded.
5. **Record** the step in `results.md` (data-model.md "Migration step").

A step never changes a golden file, the reference, the catalog or a contract.
