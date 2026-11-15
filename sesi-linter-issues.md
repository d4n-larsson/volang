# SESI riscv/riscv64 pack: issues found while building vo-lang

Collected during implementation (2026-11-13) for fixing later in `~/dev/priv/sesi`.
Each entry gives the symptom, a concrete example from vo-lang, and a suggested fix.
**Status** says whether anything has been changed already.

**Update 2026-11-13**: all eleven issues are fixed in sesi: 1-6 and 8-11 in `c722808` ("fix(riscv):
issues found while building vo-lang"), 7 in `1861446` ("feat(riscv): pointers into the frame").
Every case is in sesi's regression set (`tests/fixtures/riscv/correct.s` for the false positives,
`bugs.s` for the missed bugs). Each entry below has a **Resolution** line, and some say what
vo-lang can now change. Checking through the SESI server (`scripts/sesi check`) also needed a fix
in SESI itself, `07fdfe8`: proofs with a negation such as `\+ p(X, _)` made `/query` fail with
"goal must be callable".

**Update 2 (2026-11-13)**: 12 fixed in `4c7b24f`, 14 in `6330905`, 15-17 in `512324d`; 13 was already covered by `c722808` once the
callee has a prototype comment in the calling file (see its Resolution).

## False positives

### 1. Call results are not treated as written: FIXED (c722808)
- **Symptom**: `reads_missing_argument(a0)` on every use of a call's result in a function with
  fewer arguments; `no_return_value` for `call f; ret`.
- **Example**: 51 reports in `src/parse.s` (`call p_kind; mv s2, a0`).
- **Patch applied** (`packs/riscv/knowledge.pl`, with approval):
  `writes(P, N, R) :- calls(P, N), call_result(R).`
- **Follow-up**: see issue 6, which the patch introduced.
- **Resolution**: kept, but narrowed with issue 6: after a call only `a0`/`fa0` count as written,
  `a1`/`fa1` only when the callee's prototype returns two registers.

### 2. `tail f` in a value-returning function reports `no_return_value`: FIXED (c722808)
- **Example**: `cmd_explain: tail not_implemented` in `src/main.s`.
- **Patch applied**: `no_return_value` now excludes `tail` returns (`\+ insn(P, N, tail)`).
- **Resolution**: the patch is committed as is.

### 3. Calls to functions that never return are not modeled: FIXED (c722808)
- **Symptom**: `clobbered_by_call` on the path after a call that never returns.
- **Example**: `call_fn` in `src/interp.s`: `call rt_raise_c` on an error path, then code at the join point
  reads `t0`/`t2` that were set before the branch. Worked around by keeping the value in an `s` register.
- **Suggested fix**: a `noreturn` marker in the prototype comment (e.g. `# noreturn void rt_raise(...)`
  or C `_Noreturn`), making `ends(P, N)` true for calls to such functions.
- **Resolution**: as suggested. Prototype comments now also declare functions a file only calls;
  `noreturn`, `_Noreturn` or `[[noreturn]]` in front ends the path after a call to it.
  **vo-lang**: add `# noreturn void rt_raise_c(...)` to `src/interp.s` (and wherever it is
  called); the `s`-register workaround in `call_fn` can then go.

### 4. Memory written by `ecall` is unknown: FIXED (c722808)
- **Symptom**: `restored_from_other_slot(s2, ...)` for `ld s2, 48(sp)` after `fstat` filled a
  stack buffer.
- **Example**: `read_whole_file` in `src/sys.s` (worked around via `ld t0` + `mv s2, t0`).
- **Suggested fix**: a stack buffer whose address is passed to `ecall` (or to any call) counts as
  stored to, or `restored_from_other_slot` only applies when no call or ecall happened in between.
- **Resolution**: once the frame's address escapes (`mv a1, sp`, `addi a0, sp, K`), a call, an
  `ecall` or a store through another register counts as a possible write to every slot. Without
  an escape the slot check stays strict. **vo-lang**: the `ld t0` + `mv s2, t0` workaround in
  `read_whole_file` can go.

### 5. No opt-out for deliberate calling-convention breaks: FIXED (c722808)
- **Symptom**: 24 reports (`clobbers_callee_saved`, `not_restored`) on `jmp_restore`, the
  longjmp half of setjmp/longjmp in `src/jmp.s`, which restores `s0`-`s11` from a buffer by design.
- **Suggested fix**: an annotation such as `# sesi: allow clobbers_callee_saved, not_restored` on the
  prototype line, recorded and shown as "suppressed" so it stays visible.
- **Resolution**: `sesi: allow kind, kind` after the prototype on its line, e.g.
  `# void jmp_restore(jmp_buf *b) sesi: allow clobbers_callee_saved, not_restored`. Those problems
  are not reported; `sesi lint` lists them per function ("suppressed by sesi: allow: 12
  clobbers_callee_saved, 12 not_restored"), and a function with only suppressed problems counts
  as clean. **vo-lang**: annotate `jmp_restore` in `src/jmp.s`.

## Missed real bugs (false negatives)

### 6. Reading a stale `a1` after a single-result call is no longer caught: FIXED (c722808)
- **Example**: `check_list` in `src/check_expr.s` read `a1` (an argument) after `call ty_is`;
  `ty_is` returns one value, so `a1` was garbage. Found by review, not by the linter.
- **Second example**: `map_set` in `src/rt_map.s` called `map_index_insert` without setting `a1`
  (the entry index); `a1` still held a value from before an earlier call. Every map lookup failed. Found by a
  golden test.
- **Suggested fix**: decide per callee: if the callee's prototype returns a single register value,
  only `a0`/`fa0` count as written; `a1`/`fa1` only for two-register returns (e.g. `__int128`, structs).
- **Resolution**: as suggested, and a call now also reads its callee's argument registers when the
  callee is declared, so both examples are reported (`clobbered_by_call(a1, ...)` at the read, and
  at the call that passes the stale `a1`). **vo-lang**: the second kind of check needs a prototype
  comment for the callee in the calling file (`# int map_index_insert(map *m, int index)`);
  without one, a call is assumed to read nothing and return one value.

### 7. Stores through computed pointers into the own frame: FIXED (1861446)
- **Example**: `float_to_text` in `src/num.s` wrote a digit buffer at `8(sp)..22(sp)` through a
  pointer (`addi t5, sp, 23; sb t2, 0(t5)`), overwriting the saved `s4` at `16(sp)`. Found by review.
- **Suggested fix**: track simple `addi Rx, sp, K` derivations and treat stores through `Rx` (with
  unknown offset) as possibly hitting any slot at or above `K - len`; at least warn when a pointer
  derived from `sp` is stored through while saved slots lie above it.
- **Resolution**: pointers made from `sp` (`addi t5, sp, 23`, and `mv`/`addi` chains from it) are
  followed along every path, kept inside the frame. A branch comparing the pointer with another
  one (`bne t5, t6` against the buffer's end) bounds it on each edge; a loop bounded only by a
  value, like the digit loop, is followed to the frame's edge. A store through such a pointer that
  can reach a saved register's slot is reported as `pointer_overwrites_saved(s4, -16)`. Measured
  on GCC's buffer loops first: their reachable slots are exactly the buffers, so no false alarms.
  **vo-lang**: `float_to_text` as it was is reported; the fix is to keep the digit buffer clear of
  the save slots (or bound the loop with an end pointer).

### 8. Stores of temporaries outside the own frame (into the caller's stack): FIXED (c722808)
- **Example**: `list_sort` in `src/rt_list.s` had a 96-byte frame and did `sd t4, 104(sp)`, writing the
  caller's stack. Only `saved_outside_frame` exists, and it is limited to preserved registers.
- **Suggested fix**: report any `sp`-relative store with `Slot >= 0` (at or above the entry `sp`),
  not just stores of preserved registers.
- **Resolution**: as suggested: `writes_caller_frame(Slot)` for any store at or above `sp` at entry,
  except in functions with more than 8 integer arguments (their stack arguments are theirs).

## Messages and documentation

### 9. Confusing report for `';'` and `'#'` character literals: FIXED (c722808)
- **Symptom**: `unknown_instruction('\'')` for `li t1, ';'`. The report was right to fire: GNU as treats
  `;` as a statement separator and `#` as a comment start, so the code was broken. But the message
  does not say why.
- **Suggested fix**: a dedicated advice: "In GNU as `;` separates statements and `#` starts a
  comment; write the character's number (59, 35) instead."
- **Resolution**: reported as a problem of the line with that explanation.

### 10. Unparseable prototype comments silently disable signature checks: FIXED (c722808)
- **Symptom**: prototypes such as `# void *fmt_file(node *file)` or `# void sb_byte(sb, int b)` give no
  signature, so argument and return checks are skipped without notice.
- **Suggested fix**: accept any `identifier *` as a pointer parameter (one register), and report
  "prototype comment not understood" when a `# ...(...)` line directly above a `.globl` label does not
  parse.
- **Resolution**: as suggested; also `float`/`double` arguments go to `fa` registers (not
  counted) and a `float`/`double` result to `fa0`. `# void *fmt_file(node *file)` is now
  understood; `# void sb_byte(sb, int b)` is reported, because `sb` without `*` could be a struct
  passed by value. **vo-lang**: write `sb *` there if it is a pointer type.

### 11. Skill text: GP-relative relaxation in libc-free programs: FIXED (c722808)
- **Symptom**: a crash at runtime, not a lint issue. `la s0, file_table` was relaxed by `ld` to a
  `gp`-relative `addi` while `gp` was never set (no libc, custom `_start`).
- **Suggested fix**: the riscv64 skill could note: "Without crt0, link with `--no-relax` or set `gp`
  in `_start` (`.option push; .option norelax; la gp, __global_pointer$; .option pop`)."
- **Resolution**: added to the riscv and riscv64 skills, with a note on declaring called functions.
  **vo-lang**: run `scripts/sesi enable riscv64` from sesi again to install the updated skill.

### 12. Storing a callee-saved register's value as data counts as a (partial) save: FIXED (4c7b24f)
- **Symptom**: `partial_save(s0)` for `sb s0, 0(sp)`, where `s0` holds a code point that is written into a
  byte buffer on the stack (not saving the register).
- **Example**: `rt_chr` in `src/rt_str.s` (worked around via `mv t1, s0; sb t1, 0(sp)`).
- **Suggested fix**: count a store as a save only at the store's original width/slot pattern of a
  prologue (or when the register still holds its entry value); a narrow store of a register that was
  already written in this function is data.
- **Resolution**: a store counts as a save only while the register can still hold its entry value
  (no write to it on some path from the entry). `sb s0, 0(sp)` after `mv s0, a0` is data, so no
  `partial_save`; the same applies to every other save-based check. **vo-lang**: the
  `mv t1, s0` workaround in `rt_chr` can go.

### 13. Arguments passed to a call are not checked as reads: ALREADY FIXED (c722808), needs a prototype
- **Symptom**: no `clobbered_by_call` when an argument register is set, then a call clobbers it, then
  another call receives it as an argument. The linter only treats explicit operands as reads.
- **Example**: `report_undefined` in `src/check_expr.s` loaded the fix span into `a1`-`a3`, then called
  `str_from_cstr`, then `diag_add_fix` (which reads `a1`-`a3`). The fix got a garbage file id and
  `vo check --json` crashed. Minimal reproduction (lints clean):
  ```
  # void f(void *node)
      .globl f
  f:  addi sp, sp, -16
      sd ra, 8(sp)
      lwu a1, 4(a0)
      call g            # clobbers a1
      call h            # h(void *x, int y): reads a1
      ld ra, 8(sp)
      addi sp, sp, 16
      ret
  ```
- **Suggested fix**: a call reads `a0..a(k-1)` where `k` comes from the callee's prototype comment
  (when the callee is in the same file or a known global); with no prototype, at least `a0`.
- **Resolution**: this is the second example of issue 6, fixed in `c722808`, but only for callees
  with a prototype comment *in the file being checked*. The repro lints clean because `h` has none
  (the `# h(...)` after the call is not a prototype). With `# void g(void)` and
  `# void h(void *x, int y)` above `f`, it reports `clobbered_by_call(a1)` at `call h`.
  `src/check_expr.s` declares no prototype for `diag_add_fix`, so the bug was missed.
  "At least `a0`" for undeclared callees would not catch it (the stale register is `a1`) and would
  wrongly flag zero-argument calls. **vo-lang**: declare the external functions each file calls,
  at least those taking several arguments (`# void diag_add_fix(...)` in `src/check_expr.s`).
  Reading prototypes from the other files of the project would need SESI to check whole projects,
  not single files; not done.

### 14. Functions placed in a data section are not reported: FIXED (6330905)
- **Symptom**: no report when a function (prototype comment + `.globl` label + instructions) follows a
  `.section .rodata` (or `.bss`/`.data`) with no `.text` in between. The code links into a
  non-executable section; it ran only while string lengths happened to keep it 2-byte aligned. When
  v2 added strings, `run_test_body` landed at an odd address, `jalr` cleared bit 0, and every
  `vo test` crashed with SIGSEGV.
- **Example**: `src/interp.s`: `.section .rodata` / `s_nl: .string "\n"` / `interp_reset:` ... (fixed by
  adding `.text`). The pack linted the file clean.
- **Suggested fix**: track the current section while parsing; report `code_in_data_section` for any
  instruction (or function label with a prototype comment) outside `.text`/`.text.*`, and
  `misaligned_function` for a function label not preceded by alignment to 2 (4 without C).
- **Resolution**: the parser follows the section (`.text`, `.data`, `.bss`, `.rodata`, `.section`
  and `.pushsection`, where a name starting with `.text`, `.init` or `.fini`, or flags with `x`,
  mean code). The first instruction after each switch to a section that is not executable is
  reported as a problem of the file: "line 6: an instruction in section .rodata, which is not
  executable: put .text before the code". `.popsection`/`.previous` make the section unknown,
  and it is then treated as code. `misaligned_function` is not done: code in `.text` is only
  misaligned by data placed in `.text`, which is rare; that is the next step if it happens.
## Found during spec 003 (garbage collector)

### 15. Macros are not expanded: `unknown_instruction` false positives: FIXED (512324d)
- **Symptom**: a `.macro` defined in an included file (`.include "defs.inc"`) and used in a function
  is reported as `unknown_instruction(poll)`, though GNU as assembles it. The macro's own
  instructions are never checked either.
- **Example**: `defs.inc` defined `.macro POLL n` (six instructions: decrement `rt_budget`,
  `bgtz t5, 1000f`, `call rt_poll`, `1000:`); every `POLL 1` in `src/rt_str.s`, `src/rt_list.s`,
  `src/interp.s` and others was reported. vo-lang now writes the six instructions out at each site.
- **Suggested fix**: expand `.macro`/`.endm` (with `\arg` substitution and `.include` resolved
  relative to the `-I` paths) before analysis, so the expansion is checked like hand-written code;
  failing that, report `unexpanded_macro` once per use instead of a wrong-instruction problem.
- **Resolution**: `.macro`/`.endm` are expanded where they are used (`\param`, `name=default`,
  `\@`, `\()`; macros using macros), and the expansion is checked like hand-written code, with
  its instructions on the line of the use. `scripts/sesi` (`lint`, `hook`, `check`) appends the
  files a file `.include`s, found next to it or in the project directory, so their macros are
  known; only macro definitions are read from them, and line numbers stay the file's own. A
  `POLL 1` use in a function that has not saved `ra` is now reported (`clobbered_by_call(ra)`),
  because the expansion contains `call rt_poll`. Not done: includes inside included files, and
  `-I` paths other than the file's directory and the project directory. **vo-lang**: `POLL` can
  come back as a macro.

### 16. `make lint`-style wrappers see `suppressed by sesi: allow` notes as output: FIXED (512324d)
- **Symptom**: `sesi lint FILE` prints `f: suppressed by sesi: allow: N kind` for annotated functions
  even when there is no problem, so a wrapper that treats any output as failure fails.
- **Example**: vo-lang's `Makefile` `lint` target; it now filters those lines with `grep -v`.
- **Suggested fix**: print suppression notes only with a `--verbose` flag, or on stderr, or make
  the exit status the signal (non-zero only for reported problems).
- **Resolution**: the notes go to stderr; stdout has only problems. The exit status was already
  the signal (0 when only suppressed problems remain). **vo-lang**: the `grep -v` in the `lint`
  target can go (or check the exit status instead of the output).

### 17. A store over a saved register's slot, then "restoring" from it, is not reported: FIXED (512324d)
- **Symptom**: a function saves `s0` at `0(sp)`, later stores a temporary into `0(sp)` and loads
  `s0` back from `0(sp)` before returning. The caller's `s0` is replaced by the temporary; no
  report (`not_restored` or `pointer_overwrites_saved` would fit).
- **Example**: an early `ty_has_params_memo` in `src/types.s` (16-byte frame: `sd ra, 8(sp)`,
  `sd s0, 0(sp)`, then `sd a3, 0(sp)` as scratch). Found by review, fixed by a 32-byte frame.
  Minimal repro (lints clean):
  ```
  # int f(int x)
  f:
      addi sp, sp, -16
      sd ra, 8(sp)
      sd s0, 0(sp)
      mv s0, a0
      call g
      sd a0, 0(sp)
      ld a0, 0(sp)
      ld s0, 0(sp)
      ld ra, 8(sp)
      addi sp, sp, 16
      ret
  ```
- **Suggested fix**: track the value held by each frame slot; a store to a save slot of a
  callee-saved register before its restoring load is `save_slot_overwritten`, and a restore that
  does not load the entry value is `not_restored`.
- **Resolution**: the repro as given was already reported (`overwrites_saved(s0,-16)` at
  `sd a0, 0(sp)`), but only for a store to exactly the save slot; a narrower or shifted store
  (`sw a0, 4(sp)` over `sd s0, 0(sp)`) was missed. `overwrites_saved` now fires for any store
  overlapping a save slot before the register is restored, so both are reported. The
  `ty_has_params_memo` case was probably the shifted kind; if it was not, send the exact code.

## Found during spec 004 (bytecode compiler)

### 18. A save slot overwritten in a jump-table target is not reported: FIXED (sesi branch fix/volang-18-22, not yet committed)

- **Kind**: false negative.
- **Where**: `src/rt_call.s`, `eval_builtin` at commit `1fff558`. The function saves `s3` at
  `24(sp)` and then dispatches with `jr t1` through the `btab` jump table. The targets `b_min` and
  `b_max` store a scratch value with `sd a3, 24(sp)`, and the shared epilogue then "restores"
  `s3` from that slot. The caller's `s3` comes back as 0 or 1. It was harmless only because the
  caller, `eval_expr`, never read `s3` after the call.
- **Expected**: `overwrites_saved(s3, 24)` at `sd a3, 24(sp)`. Issue 17's fix covers the same
  store in straight-line code, so the miss is probably the indirect `jr` through a table: the
  targets are not analysed as part of the function's frame.
- **Repro**:
  ```asm
  # long f(long k)
      .globl f
  f:
      addi sp, sp, -32
      sd ra, 24(sp)
      sd s3, 16(sp)
      la t1, tab
      slli a0, a0, 3
      add t1, t1, a0
      ld t1, 0(t1)
      jr t1
  t_one:
      sd a0, 16(sp)
      j 9f
  t_two:
      li a0, 2
  9:
      ld s3, 16(sp)
      ld ra, 24(sp)
      addi sp, sp, 32
      ret
      .section .rodata
  tab:
      .dword t_one, t_two
  ```
- **Resolution**: jump tables were only read from `.word`/`.4byte`/`.long`; the table was a
  `.dword` list, so the targets were not reached from the `jr`. `.dword`, `.8byte` and `.quad`
  now count too, and the repro reports `overwrites_saved(s3,-16)` at `sd a0, 16(sp)` (slots are
  offsets from `sp` at entry: `16(sp)` after a 32-byte frame is -16).
- **Fixed in vo** by giving `builtin_call` its own scratch slot (spec 004, T008).

## Found during spec 005 (holes, properties, contracts)

### 19. `sesi lint` hangs when its stdin is an open pipe: FIXED (sesi branch fix/volang-18-22, not yet committed)

- **Kind**: tooling (not a false positive or negative).
- **Symptom**: `make lint` run from a non-interactive shell whose stdin stays open (a background
  job) blocked on `src/check.s` for over 27 minutes, with `swipl scripts/sesi.pl lint riscv64` at
  4 s of CPU. The same `sesi lint src/check.s .` with stdin from `/dev/null` finishes in 5 s, and
  the whole `make lint` that way in 70 s. The blocked `swipl` could not be killed by the user
  ("Operation not permitted").
- **Suggested fix**: `scripts/sesi` should not read stdin for `lint` (give swipl `/dev/null`, or
  read the file by path only).
- **Resolution**: no input goes through docker's stdin any more. `scripts/sesi` writes the file
  (and the files it includes) to a temporary file, mounts it and runs `sesi.pl lint PACK FILE`
  with stdin from `/dev/null`. The hang could not be reproduced with an open pipe on stdin, but
  a lint container in SESI's own session once stayed blocked for minutes after its client was
  killed: `swipl` waiting in `read_string` for an end of input that `docker run -i` never
  delivered. That path is gone. **vo-lang**: the `</dev/null` in `make lint` is no longer needed.
- **Worked around in vo**: `make lint` gives each lint `/dev/null` as stdin (spec 005).

## Found during spec 006 (query, edit, trace, replay)

### 20. An apostrophe inside a `.string` is read as a character literal: FIXED (sesi branch fix/volang-18-22, not yet committed)

- **Kind**: false positive.
- **Symptom**: `sesi lint src/query.s` reported
  `a character literal cannot hold ; or # in GNU as ... write its number instead` on two lines that
  have no character literal, only a double-quoted string containing an apostrophe:
  ```
  m_query_args: .string "`query $1` takes the program's entry file and one target: `vo query $1 main.vo TARGET`."
  ```
  GNU as assembles it as intended (the apostrophe is inside the quotes); the lint seems to start
  a character literal at `'s` and run into a later `;`-like character.
- **Suggested fix**: skip the contents of `"..."` strings before looking for character literals.
- **Resolution**: as suggested: strings (with `\"` escapes) and character literals (`'a'`,
  `'\n'`, `'"'`) are skipped before looking for a broken one; `';'` and `'#'` are still
  reported.
- **Worked around in vo**: the two messages were reworded ("the entry file of the program").

### 21. A symbol defined with `.set` is reported as never defined: FIXED (sesi branch fix/volang-18-22, not yet committed)

- **Kind**: false positive.
- **Symptom**: `.globl vm_op_ret` followed by `.set vm_op_ret, op_ret` (an alias of a local label)
  was reported as `vm_op_ret is declared global but never defined: add a vm_op_ret: label`. GNU as
  defines the symbol with `.set` (and `.equ`), and the program linked and ran.
- **Suggested fix**: count `.set NAME, ...` and `.equ NAME, ...` as definitions of NAME.
- **Resolution**: as suggested: `.set`, `.equ` and `.equiv NAME, ...` define NAME.
  **vo-lang**: the aliases can come back.
- **Worked around in vo**: the tracing handlers load the addresses from the `vm_ops` table instead
  of exporting aliases (spec 006).

### 22. Making a VM handler label `.globl` turns it into a "function" with callee-saved reports: FIXED (sesi branch fix/volang-18-22, not yet committed)

- **Kind**: limitation (not a false positive of the rules, but of the function boundary).
- **Symptom**: adding `.globl op_ret` to `src/vm.s` (a handler reached by `jr` from the dispatch
  table, which advances `s1` on purpose) produced `clobbers_callee_saved(s1)` and
  `stack_unbalanced` reports for `op_ret` and `op_try`; as local labels the same code lints clean
  (it is checked as part of `vm_run`). The `sesi: allow` marker needs a prototype line, which a
  handler does not have.
- **Suggested fix**: a way to mark a global label as a jump target rather than a function (e.g.
  `# sesi: handler`).
- **Resolution**: as suggested: `.globl op_ret  # sesi: handler` (the comment on the `.globl`
  line) makes the label a jump target; it is then checked as part of the function before it,
  as a local label is. **vo-lang**: handlers can be exported with that marker.
- **Worked around in vo**: no handler is exported; see #21.

**Re-check 2026-11-14** (after 18-22): `sesi lint` over `src/*.s` with riscv64 reports only
`src/jmp.s` (24, the deliberate longjmp): it still needs the `sesi: allow` annotation from issue 5.

### 23. A function whose label is not `.globl` is silently not checked when it follows another function

- **Kind**: false negative.
- **Symptom**: in `src/packages.s` (spec 007), `pkg_parse` had a prototype comment
  (`# void pkg_parse(long file)`) and a label, but no `.globl`. Its body stored a temporary at
  `48(sp)`, the slot where the prologue had saved `s4`, which corrupted the caller's `s4`; `vo run`
  of any program next to a non-empty `vo.packages` then crashed. `sesi lint` reported nothing for
  the file. Linted alone, the same code is reported (`overwrites_saved(s4,-48)`), under a `file:`
  heading. In the full file the label follows `pkg_name_ok` (global, ending in `ret`), so the code
  is read as unreachable code of `pkg_name_ok` and never analysed. Adding `.globl` to the eight such
  helpers in the spec 007 files surfaced one more real bug of the same kind (`pkg_sort`).
- **Suggested fix**: treat a label that directly follows a prototype comment as a function entry
  whether or not it is `.globl`; or report code after a `ret` that is reachable only from a label
  nothing in the function branches to ("unchecked code: is this a function? add .globl").
- **Worked around in vo**: every helper with a prototype comment is now `.globl`.

## Found during spec 008 (the C rewrite)

### 24. Packs are chosen by file extension, so an x86-64 `.S` is linted as RISC-V

- **Kind**: false positive.
- **Symptom**: writing `src/platform/x86_64.S` (spec 008 T051, AT&T syntax) made the PostToolUse hook
  run the riscv64 pack on it, the only enabled pack whose extensions include `.S`: 44 reports, every
  `mov`, `lea`, `jmp` and `syscall` as `unknown_instruction` and `falls_off_end` on `_start`'s final
  `jmp 1b` and on `jmp_restore`'s `jmp *0(%rdi)`. Nothing in the file is RISC-V. The same will happen
  for `src/platform/aarch64.S` (T056). Confirmed there: 48 reports, and the RISC-V reading also takes
  ARM64's `#` immediates for comments (`mov x29, #0` is shown as `mov x29,`).
- **Suggested fix**: let a pack decline a file that is not its architecture: for example a path or
  content guard in `pack.pl` (the riscv packs would accept `*riscv*` paths, or a first instruction
  they know), or a `# sesi: pack none` / `# sesi: arch x86_64` file header that the hook honours.
- **Worked around in vo**: none needed in the code. `make lint` lints only `src/platform/riscv64.S`;
  the hook's reports on the other platform files are ignored.
