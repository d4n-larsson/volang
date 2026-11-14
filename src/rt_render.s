# Values rendered in vo literal syntax (test failures, map key errors).
    .include "defs.inc"

    .section .rodata
s_true:     .string "true"
s_false:    .string "false"
s_comma:    .string ", "
s_colon:    .string ": "
s_new:      .string "new "
s_open:     .string " { "
s_close:    .string " }"
s_ok:       .string "ok("
s_err:      .string "err("
s_dots:     .string "..."

    .section .data
    .align 3
# Spec 006 (research R14): when not 0, rendering stops once the builder holds more than this many
# bytes; render_capped sets it. 0 everywhere else (expect_eq, counterexamples: full values).
# ponytail: one fixed cap of 200 bytes per value; a per-use limit if anyone needs longer values.
    .globl render_cap
render_cap: .dword 0

    .text
# void render_str_lit(void *sb, void *str)
# A str as a quoted literal with the four escapes.
    .globl render_str_lit
render_str_lit:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    addi s1, a1, STR_DATA
    ld s2, STR_LEN(a1)
    li a1, '"'
    call sb_byte
1:
    beqz s2, 9f
    la t0, render_cap
    ld t0, 0(t0)
    beqz t0, 3f
    ld t1, SB_LEN(s0)
    bgt t1, t0, 9f
3:
    lbu s3, 0(s1)
    addi s1, s1, 1
    addi s2, s2, -1
    li t0, 10
    li t1, 'n'
    beq s3, t0, 2f
    li t0, 9
    li t1, 't'
    beq s3, t0, 2f
    li t0, '\\'
    mv t1, s3
    beq s3, t0, 2f
    li t0, '"'
    beq s3, t0, 2f
    mv a0, s0
    mv a1, s3
    call sb_byte
    j 1b
2:
    mv s3, t1
    mv a0, s0
    li a1, '\\'
    call sb_byte
    mv a0, s0
    mv a1, s3
    call sb_byte
    j 1b
9:
    mv a0, s0
    li a1, '"'
    call sb_byte
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void render_value(void *sb, long value, void *type)
    .globl render_value
render_value:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    la t6, rt_budget          # POLL 1: --timeout budget
    ld t5, 0(t6)
    addi t5, t5, -1
    sd t5, 0(t6)
    bgtz t5, 1000f
    call rt_poll
1000:
    la t0, render_cap
    ld t0, 0(t0)
    beqz t0, 1001f
    ld t1, SB_LEN(s0)
    bgt t1, t0, 99f
1001:
    mv a0, s2                   # type parameters: the running frame's type arguments
    call rt_concrete
    mv s2, a0
    mv a0, s0                   # the arguments as they arrived (the cases below use them)
    mv a1, s1
    mv a2, s2
    ld t0, TY_KIND(s2)
    li t1, TY_INT
    beq t0, t1, 10f
    li t1, TY_FLOAT
    beq t0, t1, 11f
    li t1, TY_BOOL
    beq t0, t1, 12f
    li t1, TY_STR
    beq t0, t1, 13f
    li t1, TY_LIST
    beq t0, t1, 14f
    li t1, TY_MAP
    beq t0, t1, 15f
    li t1, TY_RECORD
    beq t0, t1, 16f
    li t1, TY_VARIANT
    beq t0, t1, 17f
    li t1, TY_RESULT
    beq t0, t1, 18f
    li t1, TY_FUNC
    beq t0, t1, 19f
    j 99f
10:
    call int_to_dec
    j 99f
11:
    call float_to_text
    j 99f
12:
    la a1, s_false
    beqz s1, 121f
    la a1, s_true
121:
    call sb_cstr
    j 99f
13:
    call render_str_lit
    j 99f
14:                             # [a, b]
    li a1, '['
    call sb_byte
    li s3, 0
141:
    ld t0, L_LEN(s1)
    bge s3, t0, 142f
    la t0, render_cap
    ld t0, 0(t0)
    beqz t0, 144f
    ld t1, SB_LEN(s0)
    bgt t1, t0, 142f
144:
    beqz s3, 143f
    mv a0, s0
    la a1, s_comma
    call sb_cstr
143:
    ld t0, L_DATA(s1)
    slli t1, s3, 3
    add t0, t0, t1
    ld a1, 0(t0)
    mv a0, s0
    ld a2, TY_A(s2)
    call render_value
    addi s3, s3, 1
    j 141b
142:
    mv a0, s0
    li a1, ']'
    call sb_byte
    j 99f
15:                             # {k: v, ...} in insertion order
    li a1, '{'
    call sb_byte
    li s3, 0
    li s4, 0                    # written any?
151:
    ld t0, M_USED(s1)
    bge s3, t0, 152f
    la t0, render_cap
    ld t0, 0(t0)
    beqz t0, 154f
    ld t1, SB_LEN(s0)
    bgt t1, t0, 152f
154:
    ld t0, M_ENTRIES(s1)
    li t1, ME_SIZE
    mul t1, t1, s3
    add t0, t0, t1
    addi s3, s3, 1
    lwu t1, ME_LIVE(t0)
    beqz t1, 151b
    sd t0, 0(sp)
    beqz s4, 153f
    mv a0, s0
    la a1, s_comma
    call sb_cstr
153:
    li s4, 1
    ld t0, 0(sp)
    ld a1, ME_KEY(t0)
    mv a0, s0
    ld a2, TY_A(s2)
    call render_value
    mv a0, s0
    la a1, s_colon
    call sb_cstr
    ld t0, 0(sp)
    ld a1, ME_VAL(t0)
    mv a0, s0
    ld a2, TY_B(s2)
    call render_value
    j 151b
152:
    mv a0, s0
    li a1, '}'
    call sb_byte
    j 99f
16:                             # new T { f: v, ... }
    la a1, s_new
    call sb_cstr
    ld t0, TY_DECL(s2)
    ld t0, N_A(t0)
    ld a1, N_A(t0)
    mv a0, s0
    call sb_str
    ld a1, TY_B(s2)             # an instance: new Name[args] { ... }
    beqz a1, 0f
    mv a0, s0
    call ty_args_into
0:
    mv a0, s0
    la a1, s_open
    call sb_cstr
    li s3, 0
161:
    ld t0, TY_DECL(s2)
    ld a0, N_B(t0)
    mv a1, s3
    call member_at
    beqz a0, 162f
    mv s4, a0
    beqz s3, 163f
    mv a0, s0
    la a1, s_comma
    call sb_cstr
163:
    ld t0, N_A(s4)
    ld a1, N_A(t0)
    mv a0, s0
    call sb_str
    mv a0, s0
    la a1, s_colon
    call sb_cstr
    slli t0, s3, 3
    add t0, s1, t0
    ld a1, R_FIELDS(t0)
    sd a1, 0(sp)
    ld t0, N_B(s4)
    ld a1, N_TYPE(t0)
    mv a0, s2
    call field_type_in
    mv a2, a0
    ld a1, 0(sp)
    mv a0, s0
    call render_value
    addi s3, s3, 1
    j 161b
162:
    mv a0, s0
    la a1, s_close
    call sb_cstr
    j 99f
17:                             # T.case or T.case(v)
    ld t0, TY_DECL(s2)
    ld t0, N_A(t0)
    ld a1, N_A(t0)
    call sb_str
    mv a0, s0
    li a1, '.'
    call sb_byte
    ld t0, TY_DECL(s2)
    ld a0, N_B(t0)
    lwu a1, V_TAG(s1)
    call member_at
    mv s4, a0
    ld t0, N_A(s4)
    ld a1, N_A(t0)
    mv a0, s0
    call sb_str
    ld t0, N_B(s4)
    beqz t0, 99f
    mv a0, s0
    li a1, '('
    call sb_byte
    ld t0, N_B(s4)
    ld a1, N_TYPE(t0)
    mv a0, s2
    call field_type_in
    mv a2, a0
    ld a1, V_PAY(s1)
    mv a0, s0
    call render_value
    mv a0, s0
    li a1, ')'
    call sb_byte
    j 99f
18:                             # ok(v), ok() or err("m")
    lwu t0, V_TAG(s1)
    la a1, s_ok
    beqz t0, 181f
    la a1, s_err
181:
    call sb_cstr
    mv a0, s2
    lwu a1, V_TAG(s1)
    call case_payload
    beqz a0, 182f
    mv a2, a0
    mv a0, s0
    ld a1, V_PAY(s1)
    call render_value
182:
    mv a0, s0
    li a1, ')'
    call sb_byte
    j 99f
19:                             # a function value: its type, as <fn(int) -> int>
    li a1, '<'
    call sb_byte
    mv a0, s0
    mv a1, s2
    call ty_name_into
    mv a0, s0
    li a1, '>'
    call sb_byte
99:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void render_capped(void *sb, long value, void *type)
# render_value, shortened to at most 200 bytes (cut at a UTF-8 boundary) with "..." appended when
# anything was left out (spec 006: traces and the values in runtime errors).
    .globl render_capped
render_capped:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    ld s1, SB_LEN(a0)           # where this value starts
    la t0, render_cap
    ld s2, 0(t0)                # the cap of an enclosing render (restored below)
    addi t1, s1, 200
    sd t1, 0(t0)
    call render_value
    la t0, render_cap
    sd s2, 0(t0)
    ld t0, SB_LEN(s0)
    sub t0, t0, s1
    li t1, 200
    ble t0, t1, 9f
    addi t2, s1, 200            # cut back to a character boundary at most 200 bytes in
    ld t3, SB_PTR(s0)
1:
    ble t2, s1, 2f
    add t4, t3, t2
    lbu t4, 0(t4)
    andi t4, t4, 0xc0
    li t5, 0x80
    bne t4, t5, 2f
    addi t2, t2, -1
    j 1b
2:
    sd t2, SB_LEN(s0)
    mv a0, s0
    la a1, s_dots
    call sb_cstr
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret
