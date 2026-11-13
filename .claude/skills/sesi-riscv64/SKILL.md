---
name: "sesi-riscv64"
description: "Write or change 64-bit RISC-V assembly (.s, .S). The SESI riscv64 pack checks every edit against RV64IMAFD, Zicsr and the standard calling convention (lp64d); this skill says how to work with its reports and how to look facts up before writing code."
---

# Writing 64-bit RISC-V assembly with the SESI riscv64 pack

## While you write

- Start every function with a prototype comment, then the global label:
  ```
  # int count_char(const char *s, int c)
      .globl count_char
  count_char:
  ```
  The checker reads the signature from it: reading `a2` in a two-argument function, or a
  function without arguments that never sets `a0`, is then reported. Only plain C prototypes
  with types that fit one register count (`long`, `long long` and pointers do; `double` and
  structs do not).
- Write the label exactly as declared (`count_char:`, not `.count_char:`).
- Registers are 8 bytes: save and restore `ra` and `s0`-`s11` with `sd`/`ld` (and `fs0`-`fs11`
  with `fsd`/`fld`) at offsets that are multiples of 8; `sw` keeps only half of a register.
- Values needed after a `call` belong in `s0`-`s11` / `fs0`-`fs11`, saved in the prologue and
  restored before every return. Keep `sp` moves multiples of 16.

## After every edit

A hook checks each function in the file you wrote. A report looks like this:

```
line 9: mv s1, t0
    clobbered_by_call(t0): the call on line 7 overwrote it, because ...
    fix: A call may change ra, t0-t6, a0-a7, ...
```

- Each problem happens on at least one path through the function (branches and jump tables are
  followed). Fix it as the `fix:` line says, then the hook checks again.
- If you are sure a report is wrong (for example a path the code can never take), say why in
  your reply and leave the code. Do not rewrite correct code to silence it.
- No report means the checks passed, not that the function computes the right result: still
  assemble, link and test it.

## Looking facts up

Ask the same knowledge the checker uses; every answer names the facts and rules behind it:

```
/home/dan/dev/priv/sesi/scripts/sesi ask riscv64 'callee_saved(R)'              # which registers a function must preserve
/home/dan/dev/priv/sesi/scripts/sesi ask riscv64 'access_width(fld, W)'         # bytes an instruction touches
/home/dan/dev/priv/sesi/scripts/sesi ask riscv64 'advice(clobbered_by_call, A)' # how to fix a kind of problem
/home/dan/dev/priv/sesi/scripts/sesi lint path/to/file.s                        # the hook's check, by hand
```

The knowledge is `/home/dan/dev/priv/sesi/packs/riscv64/knowledge.pl` (what RV64 adds) on top of the riscv pack's. If a correct instruction is reported as
`unknown_instruction`, or a real calling-convention bug is not reported, the pack is missing
something: tell the user rather than working around it.
