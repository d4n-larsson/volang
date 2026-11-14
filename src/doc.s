# vo doc (spec 007 US1, research R7): exact signatures from the checker's own declarations,
# descriptions and examples from the comments above each item in the library source, builtins
# from the reference's table. `vo doc [--json] [NAME]` (contracts/cli.md).
    .include "defs.inc"

    .section .rodata
    .align 3
doc_modules: .dword dm_json, dm_time, dm_text, dm_random, dm_http, 0
dm_json:    .string "json"
dm_time:    .string "time"
dm_text:    .string "text"
dm_random:  .string "random"
dm_http:    .string "http"
s_std_dot:  .string "std."
s_dot:      .string "."
s_std_slash: .string "std/"
s_dot_vo:   .string ".vo"
s_builtins_hdr: .string "## Builtins"
s_h_builtins: .string "builtins:\n"
s_h_modules: .string "modules:\n"
s_two:      .string "  "
s_four:     .string "    "
s_example:  .string "    example: "
s_fn:       .string "fn "
s_record:   .string "record "
s_variant:  .string "variant "
s_open_br:  .string " { "
s_close_br: .string " }"
s_semi_sp:  .string "; "
s_colon_sp: .string ": "
s_comma_sp: .string ", "
s_arrow:    .string " -> "
s_uses:     .string " uses "
s_ordered:  .string ": ordered"
s_effects:  .string ": effects"
code_t10:   .string "T0010"
m_not_found: .string "Nothing is documented as `$1`; `vo doc` lists the builtins and modules."
m_did_you_mean: .string "$1 Did you mean `$2`?"
j_head:     .string "{\"v\":1,\"kind\":\"doc\",\"module\":"
j_name:     .string ",\"name\":"
j_item:     .string ",\"item\":\""
j_sig:      .string "\",\"signature\":"
j_desc:     .string ",\"description\":"
j_example:  .string ",\"example\":"
j_close:    .string "}\n"
j_items:    .string ",\"items\":"
j_tparams:  .string ",\"tparams\":"
j_params:   .string ",\"params\":["
j_returns:  .string "],\"returns\":"
j_effects:  .string ",\"effects\":"
j_fields:   .string ",\"fields\":["
j_cases:    .string ",\"cases\":["
j_pair_name: .string "{\"name\":"
j_pair_type: .string ",\"type\":"
j_null:     .string "null"
w_fn:       .string "fn"
w_record:   .string "record"
w_variant:  .string "variant"
w_builtin:  .string "builtin"
w_module:   .string "module"
w_builtins: .string "builtins"

    .text
# ---- small helpers ----

# void *doc_line_text(long file, void *node, long skip)
# The text of a one-line comment node after its `#` and `skip` more characters.
    .globl doc_line_text
doc_line_text:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a1
    mv s1, a2
    call file_get
    ld t0, FILE_TEXT(a0)
    lwu t1, N_START(s0)
    lwu t2, N_END(s0)
    addi t1, t1, 1              # past `#`
    add t1, t1, s1
    bge t1, t2, 1f
    add a0, t0, t1
    sub a1, t2, t1
    call str_new
    j 9f
1:
    li a0, 0
    li a1, 0
    call str_new
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# long doc_is_example(long file, void *node)
# 1 when the comment is `#` followed by five spaces (an example line).
    .globl doc_is_example
doc_is_example:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a1
    call file_get
    ld t0, FILE_TEXT(a0)
    lwu t1, N_START(s0)
    lwu t2, N_END(s0)
    sub t3, t2, t1
    li a0, 0
    li t4, 7
    blt t3, t4, 9f
    add t0, t0, t1
    li t4, 1
1:
    li t5, 6
    bge t4, t5, 2f
    add t6, t0, t4
    lbu t6, 0(t6)
    li t5, ' '
    bne t6, t5, 9f
    addi t4, t4, 1
    j 1b
2:
    li a0, 1
9:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void doc_comments(long file, void *items, long index, void **out)
# The description (out[0], the first comment line) and example (out[1]) of the item at index:
# the comments on the lines right above it. Both 0 when it has none (an internal item).
    .globl doc_comments
doc_comments:
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
    mv s4, a3
    sd zero, 0(s4)
    sd zero, 8(s4)
    mv a0, s1                   # the item's first line
    mv a1, s2
    call vec_get
    lwu a1, N_START(a0)
    mv a0, s0
    call pos_line
    mv s3, a0                   # the line the next comment must be on, plus one
1:
    addi s2, s2, -1
    bltz s2, 9f
    mv a0, s1
    mv a1, s2
    call vec_get
    lhu t0, N_KIND(a0)
    li t1, N_COMMENT
    bne t0, t1, 9f
    sd a0, 0(sp)
    lwu a1, N_START(a0)
    mv a0, s0
    call pos_line
    addi t0, a0, 1
    bne t0, s3, 9f
    mv s3, a0
    mv a0, s0
    ld a1, 0(sp)
    call doc_is_example
    beqz a0, 2f
    mv a0, s0                   # the example (the last example line above wins: the first one)
    ld a1, 0(sp)
    li a2, 5
    call doc_line_text
    sd a0, 8(s4)
    j 1b
2:
    mv a0, s0                   # a description line: the topmost one is the description
    ld a1, 0(sp)
    li a2, 1
    call doc_line_text
    sd a0, 0(s4)
    j 1b
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void doc_out(void *sb)
    .globl doc_out
doc_out:
    mv t0, a0
    li a0, 1
    ld a1, SB_PTR(t0)
    ld a2, SB_LEN(t0)
    tail out_bytes

# ---- signatures ----

# void doc_tparams(void *sb, void *decl)
# [T, U: ordered, E: effects] when the item has type parameters.
    .globl doc_tparams
doc_tparams:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    mv a0, a1
    call item_fx
    beqz a0, 9f
    ld s1, FX_TPARAMS(a0)
    beqz s1, 9f
    ld t0, VEC_N(s1)
    beqz t0, 9f
    mv a0, s0
    li a1, '['
    call sb_byte
    li s2, 0
1:
    ld t0, VEC_N(s1)
    bge s2, t0, 8f
    beqz s2, 2f
    mv a0, s0
    la a1, s_comma_sp
    call sb_cstr
2:
    mv a0, s1
    mv a1, s2
    call vec_get
    sd a0, 0(sp)
    ld t0, N_A(a0)
    ld a1, N_A(t0)
    mv a0, s0
    call sb_str
    ld t0, 0(sp)
    ld t0, N_B(t0)
    li t1, TPC_ORDERED
    la a1, s_ordered
    beq t0, t1, 3f
    li t1, TPC_EFFECTS
    la a1, s_effects
    bne t0, t1, 4f
3:
    mv a0, s0
    call sb_cstr
4:
    addi s2, s2, 1
    j 1b
8:
    mv a0, s0
    li a1, ']'
    call sb_byte
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void doc_signature(void *sb, void *item)
# fn name[T](a: A, b: B) -> R uses E; record Name { f: T; ... }; variant Name { c(T); d; }.
    .globl doc_signature
doc_signature:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    lhu t0, N_KIND(s1)
    li t1, N_RECORD
    beq t0, t1, 20f
    li t1, N_VARIANT
    beq t0, t1, 30f
    la a1, s_fn                 # a function
    call sb_cstr
    ld t0, N_A(s1)
    ld a1, N_A(t0)
    mv a0, s0
    call sb_str
    mv a0, s0
    mv a1, s1
    call doc_tparams
    mv a0, s0
    li a1, '('
    call sb_byte
    ld s2, N_B(s1)              # parameters
    li s3, 0
1:
    ld t0, VEC_N(s2)
    bge s3, t0, 2f
    beqz s3, 11f
    mv a0, s0
    la a1, s_comma_sp
    call sb_cstr
11:
    mv a0, s2
    mv a1, s3
    call vec_get
    sd a0, 0(sp)
    ld t0, N_A(a0)
    ld a1, N_A(t0)
    mv a0, s0
    call sb_str
    mv a0, s0
    la a1, s_colon_sp
    call sb_cstr
    ld t0, 0(sp)
    mv a0, s0
    ld a1, N_TYPE(t0)
    call ty_name_into
    addi s3, s3, 1
    j 1b
2:
    mv a0, s0
    li a1, ')'
    call sb_byte
    ld t0, N_TYPE(s1)           # the return type, unless void
    beqz t0, 3f
    ld t1, TY_KIND(t0)
    li t2, TY_VOID
    beq t1, t2, 3f
    mv a0, s0
    la a1, s_arrow
    call sb_cstr
    mv a0, s0
    ld a1, N_TYPE(s1)
    call ty_name_into
3:
    mv a0, s1                   # effects
    call item_fx
    beqz a0, 9f
    ld s2, FX_DECLARED(a0)
    beqz s2, 9f
    mv a0, s2
    call es_is_empty
    bnez a0, 9f
    mv a0, s0
    la a1, s_uses
    call sb_cstr
    mv a0, s2
    mv a1, s0
    call es_render
    j 9f
20:
    la a1, s_record             # record Name { f: T; ... }
    li s3, 1
    j 31f
30:
    la a1, s_variant
    li s3, 0
31:
    call sb_cstr
    ld t0, N_A(s1)
    ld a1, N_A(t0)
    mv a0, s0
    call sb_str
    mv a0, s0
    mv a1, s1
    call doc_tparams
    mv a0, s0
    la a1, s_open_br
    call sb_cstr
    li s2, 0
32:
    ld a0, N_B(s1)
    mv a1, s2
    call member_at
    beqz a0, 38f
    sd a0, 0(sp)
    ld t0, N_A(a0)
    ld a1, N_A(t0)
    mv a0, s0
    call sb_str
    ld t0, 0(sp)
    ld t1, N_B(t0)              # the field's or the case's type
    beqz t1, 36f
    beqz s3, 34f
    mv a0, s0
    la a1, s_colon_sp
    call sb_cstr
    ld t0, 0(sp)
    ld t1, N_B(t0)
    mv a0, s0
    ld a1, N_TYPE(t1)
    call ty_name_into
    j 36f
34:
    mv a0, s0
    li a1, '('
    call sb_byte
    ld t0, 0(sp)
    ld t1, N_B(t0)
    mv a0, s0
    ld a1, N_TYPE(t1)
    call ty_name_into
    mv a0, s0
    li a1, ')'
    call sb_byte
36:
    mv a0, s0
    li a1, 59                   # ;
    call sb_byte
    mv a0, s0
    li a1, ' '
    call sb_byte
    addi s2, s2, 1
    j 32b
38:
    mv a0, s0
    li a1, '}'
    call sb_byte
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void doc_json_pair(void *sb, void *name, void *type)
# {"name":NAME,"type":TYPE} (type 0: null).
    .globl doc_json_pair
doc_json_pair:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    la a1, j_pair_name
    call sb_cstr
    mv a0, s0
    mv a1, s1
    call json_str_into
    mv a0, s0
    la a1, j_pair_type
    call sb_cstr
    beqz s2, 1f
    mv a0, s2
    call ty_name
    mv a1, a0
    mv a0, s0
    call json_str_into
    j 2f
1:
    mv a0, s0
    la a1, j_null
    call sb_cstr
2:
    mv a0, s0
    li a1, '}'
    call sb_byte
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void doc_json_split(void *sb, void *text)
# Appends text as a JSON list of strings, cut at ", " outside parentheses ("" gives []).
    .globl doc_json_split
doc_json_split:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    sd s4, 0(sp)
    mv s0, a0
    mv s1, a1
    li a1, '['
    call sb_byte
    ld t0, STR_LEN(s1)
    beqz t0, 8f
    li s2, 0                    # the current piece starts here
    li s3, 0                    # index
    li s4, 0                    # parenthesis depth
1:
    ld t0, STR_LEN(s1)
    bge s3, t0, 5f
    addi t1, s1, STR_DATA
    add t1, t1, s3
    lbu t2, 0(t1)
    li t3, '('
    bne t2, t3, 2f
    addi s4, s4, 1
2:
    li t3, ')'
    bne t2, t3, 3f
    addi s4, s4, -1
3:
    li t3, ','
    bne t2, t3, 4f
    bnez s4, 4f
    mv a0, s0                   # a piece ends at this comma
    mv a1, s1
    mv a2, s2
    mv a3, s3
    call doc_json_piece
    mv a0, s0
    li a1, ','
    call sb_byte
    addi s3, s3, 2
    mv s2, s3
    j 1b
4:
    addi s3, s3, 1
    j 1b
5:
    mv a0, s0
    mv a1, s1
    mv a2, s2
    mv a3, s3
    call doc_json_piece
8:
    mv a0, s0
    li a1, ']'
    call sb_byte
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    ld s4, 0(sp)
    addi sp, sp, 48
    ret

# void doc_json_piece(void *sb, void *text, long from, long to)
    .globl doc_json_piece
doc_json_piece:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    addi a0, a1, STR_DATA
    add a0, a0, a2
    sub a1, a3, a2
    call str_new
    mv a1, a0
    mv a0, s0
    call json_str_into
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void doc_json_shape(void *sb, void *item)
# ,"tparams":[...],"params":[...],"returns":"T","effects":[...] for a function;
# ,"tparams":[...],"fields":[...] or "cases":[...] for a record or variant.
    .globl doc_json_shape
doc_json_shape:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    la a1, j_tparams            # the type parameters as written: "T", "T: ordered"
    call sb_cstr
    call sb_new
    mv s2, a0
    mv a1, s1
    call doc_tparams
    ld t0, SB_LEN(s2)
    beqz t0, 1f
    ld t1, SB_PTR(s2)           # drop the brackets
    addi a0, t1, 1
    addi a1, t0, -2
    call str_new
    j 2f
1:
    li a0, 0
    li a1, 0
    call str_new
2:
    mv a1, a0
    mv a0, s0
    call doc_json_split
    lhu t0, N_KIND(s1)
    li t1, N_FN
    bne t0, t1, 20f
    mv a0, s0                   # a function: parameters, return type, effects
    la a1, j_params
    call sb_cstr
    ld s2, N_B(s1)
    li s3, 0
11:
    ld t0, VEC_N(s2)
    bge s3, t0, 12f
    beqz s3, 13f
    mv a0, s0
    li a1, ','
    call sb_byte
13:
    mv a0, s2
    mv a1, s3
    call vec_get
    ld t0, N_A(a0)
    ld a1, N_A(t0)
    ld a2, N_TYPE(a0)
    mv a0, s0
    call doc_json_pair
    addi s3, s3, 1
    j 11b
12:
    mv a0, s0
    la a1, j_returns
    call sb_cstr
    ld a0, N_TYPE(s1)
    call ty_name
    mv a1, a0
    mv a0, s0
    call json_str_into
    mv a0, s0
    la a1, j_effects
    call sb_cstr
    call sb_new
    mv s2, a0
    mv a0, s1
    call item_fx
    beqz a0, 14f
    ld a0, FX_DECLARED(a0)
    beqz a0, 14f
    mv a1, s2
    call es_render
14:
    mv a0, s2
    call sb_finish
    mv a1, a0
    mv a0, s0
    call doc_json_split
    j 9f
20:
    la a1, j_fields             # a record's fields or a variant's cases
    li t1, N_RECORD
    beq t0, t1, 21f
    la a1, j_cases
21:
    mv a0, s0
    call sb_cstr
    li s3, 0
22:
    ld a0, N_B(s1)
    mv a1, s3
    call member_at
    beqz a0, 25f
    beqz s3, 23f
    sd a0, 0(sp)
    mv a0, s0
    li a1, ','
    call sb_byte
    ld a0, 0(sp)
23:
    ld t0, N_A(a0)
    ld a1, N_A(t0)
    ld t1, N_B(a0)
    li a2, 0
    beqz t1, 24f
    ld a2, N_TYPE(t1)
24:
    mv a0, s0
    call doc_json_pair
    addi s3, s3, 1
    j 22b
25:
    mv a0, s0
    li a1, ']'
    call sb_byte
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# ---- one item ----

# void doc_print_item(void *module, void *item, void *desc, void *example, long full)
# Text: the signature, the description indented, and (full) the example; or one JSON record.
    .globl doc_print_item
doc_print_item:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    sd s5, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    mv s3, a3
    mv s4, a4
    call sb_new
    mv s5, a0
    call sb_new                 # the signature on its own
    sd a0, 0(sp)
    mv a1, s1
    call doc_signature
    la t0, opt_json
    ld t0, 0(t0)
    beqz t0, 5f
    mv a0, s5
    la a1, j_head
    call sb_cstr
    mv a0, s5
    mv a1, s0
    call json_str_into
    mv a0, s5
    la a1, j_name
    call sb_cstr
    ld t0, N_A(s1)
    ld a1, N_A(t0)
    mv a0, s5
    call json_str_into
    mv a0, s5
    la a1, j_item
    call sb_cstr
    lhu t0, N_KIND(s1)
    la a1, w_fn
    li t1, N_RECORD
    bne t0, t1, 1f
    la a1, w_record
1:
    li t1, N_VARIANT
    bne t0, t1, 2f
    la a1, w_variant
2:
    mv a0, s5
    call sb_cstr
    mv a0, s5
    la a1, j_sig
    call sb_cstr
    ld a0, 0(sp)
    call sb_finish
    mv a1, a0
    mv a0, s5
    call json_str_into
    mv a0, s5
    mv a1, s1
    call doc_json_shape
    mv a0, s5
    la a1, j_desc
    call sb_cstr
    mv a0, s5
    mv a1, s2
    call json_str_into
    beqz s3, 3f
    mv a0, s5
    la a1, j_example
    call sb_cstr
    mv a0, s5
    mv a1, s3
    call json_str_into
3:
    mv a0, s5
    la a1, j_close
    call sb_cstr
    j 8f
5:
    ld t0, 0(sp)
    mv a0, s5
    ld a1, SB_PTR(t0)
    ld a2, SB_LEN(t0)
    call sb_bytes
    mv a0, s5
    li a1, 10
    call sb_byte
    mv a0, s5
    la a1, s_four
    call sb_cstr
    mv a0, s5
    mv a1, s2
    call sb_str
    mv a0, s5
    li a1, 10
    call sb_byte
    beqz s4, 8f
    beqz s3, 8f
    mv a0, s5
    la a1, s_example
    call sb_cstr
    mv a0, s5
    mv a1, s3
    call sb_str
    mv a0, s5
    li a1, 10
    call sb_byte
8:
    mv a0, s5
    call doc_out
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    ld s5, 8(sp)
    addi sp, sp, 64
    ret

# ---- a module ----

# long doc_std_file(void *path)
# The file id of an embedded std/ file, registered once; -1 when there is none.
    .globl doc_std_file
doc_std_file:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    call file_find
    bgez a0, 9f
    mv a0, s0
    call std_lookup
9:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# long doc_module(char *module_word, void *item_name)
# Checks std/MODULE.vo and prints its documented items (all of them, or the one named item_name);
# returns how many were printed, or -1 when there is no such module.
    .globl doc_module
doc_module:
    addi sp, sp, -80
    sd ra, 72(sp)
    sd s0, 64(sp)
    sd s1, 56(sp)
    sd s2, 48(sp)
    sd s3, 40(sp)
    sd s4, 32(sp)
    sd s5, 24(sp)
    mv s0, a0
    mv s1, a1
    call sb_new                 # the path std/MODULE.vo and the module's name std.MODULE
    mv s2, a0
    la a1, s_std_slash
    call sb_cstr
    mv a0, s2
    mv a1, s0
    call sb_cstr
    mv a0, s2
    la a1, s_dot_vo
    call sb_cstr
    mv a0, s2
    call sb_finish
    call doc_std_file
    li t0, -1
    bltz a0, 90f
    mv s3, a0                   # the file
    li a1, 0
    call check_program
    call sb_new
    mv s5, a0
    la a1, s_std_dot
    call sb_cstr
    mv a0, s5
    mv a1, s0
    call sb_cstr
    mv a0, s5
    call sb_finish
    mv s5, a0                   # std.MODULE
    mv a0, s3
    call file_get
    ld t0, FILE_AST(a0)
    ld s2, N_A(t0)              # items
    li s4, 0                    # index
    sd zero, 16(sp)             # printed
1:
    ld t0, VEC_N(s2)
    bge s4, t0, 8f
    mv a0, s2
    mv a1, s4
    call vec_get
    sd a0, 0(sp)
    lhu t0, N_KIND(a0)
    li t1, N_FN
    beq t0, t1, 2f
    li t1, N_RECORD
    beq t0, t1, 2f
    li t1, N_VARIANT
    bne t0, t1, 7f
2:
    beqz s1, 3f                 # only the named item
    ld t0, 0(sp)
    ld t0, N_A(t0)
    ld a0, N_A(t0)
    mv a1, s1
    call str_eq
    beqz a0, 7f
3:
    mv a0, s3
    mv a1, s2
    mv a2, s4
    addi a3, sp, 32
    call doc_comments
    ld a2, 32(sp)
    beqz a2, 7f                 # internal: not documented
    la t0, doc_collect
    ld t0, 0(t0)
    beqz t0, 4f
    la a0, s_dot                # collecting: std.MODULE.ITEM
    call str_from_cstr
    mv a1, a0
    mv a0, s5
    call str_concat
    ld t0, 0(sp)
    ld t0, N_A(t0)
    ld a1, N_A(t0)
    call str_concat
    mv a1, a0
    la t0, doc_collect
    ld a0, 0(t0)
    call vec_push
    j 7f
4:
    mv a0, s5
    ld a1, 0(sp)
    ld a3, 40(sp)
    snez a4, s1
    call doc_print_item
    ld t0, 16(sp)
    addi t0, t0, 1
    sd t0, 16(sp)
7:
    addi s4, s4, 1
    j 1b
8:
    ld t0, 16(sp)
90:
    mv a0, t0
    ld ra, 72(sp)
    ld s0, 64(sp)
    ld s1, 56(sp)
    ld s2, 48(sp)
    ld s3, 40(sp)
    ld s4, 32(sp)
    ld s5, 24(sp)
    addi sp, sp, 80
    ret

# ---- builtins (the reference's table) ----

# char *doc_builtin_rows(void)
# The first `| ` row of the reference's Builtins table after its header and separator rows.
    .globl doc_builtin_rows
doc_builtin_rows:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    la s0, ref_md
1:
    lbu t0, 0(s0)
    beqz t0, 8f
    mv a0, s0
    la a1, s_builtins_hdr
    call doc_starts
    bnez a0, 2f
    mv a0, s0
    call doc_next_line
    mv s0, a0
    j 1b
2:
    li t2, 0                    # skip to the table: header, then separator, then rows
3:
    mv a0, s0
    call doc_next_line
    mv s0, a0
    lbu t0, 0(s0)
    beqz t0, 8f
    li t1, '|'
    bne t0, t1, 3b
    mv a0, s0                   # the header row
    call doc_next_line
    call doc_next_line          # past the separator row
    j 9f
8:
    li a0, 0
9:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# char *doc_next_line(char *p)
    .globl doc_next_line
doc_next_line:
1:
    lbu t0, 0(a0)
    beqz t0, 9f
    addi a0, a0, 1
    li t1, 10
    bne t0, t1, 1b
9:
    ret

# long doc_starts(char *p, char *prefix)
    .globl doc_starts
doc_starts:
1:
    lbu t1, 0(a1)
    beqz t1, 2f
    lbu t0, 0(a0)
    bne t0, t1, 3f
    addi a0, a0, 1
    addi a1, a1, 1
    j 1b
2:
    li a0, 1
    ret
3:
    li a0, 0
    ret

# void *doc_cell(char *row, long index)
# The index-th cell of a table row, trimmed, with `\|` unescaped; the signature cell (1) also
# without its backquotes.
    .globl doc_cell
doc_cell:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    mv s1, a1
    li t2, -1                   # cells start after each unescaped `|`
1:
    lbu t0, 0(s0)
    beqz t0, 8f
    li t1, 10
    beq t0, t1, 8f
    li t1, 92
    bne t0, t1, 11f
    addi s0, s0, 2              # `\|`
    j 1b
11:
    li t1, '|'
    bne t0, t1, 2f
    addi t2, t2, 1
    beq t2, s1, 3f
2:
    addi s0, s0, 1
    j 1b
3:
    addi s0, s0, 1
4:
    lbu t0, 0(s0)               # skip leading spaces
    li t1, ' '
    bne t0, t1, 5f
    addi s0, s0, 1
    j 4b
5:
    call sb_new
    mv s2, a0
6:
    lbu t0, 0(s0)               # copy to the next unescaped `|`
    beqz t0, 7f
    li t1, 10
    beq t0, t1, 7f
    li t1, '|'
    beq t0, t1, 7f
    li t1, 92
    bne t0, t1, 61f
    lbu t3, 1(s0)
    li t1, '|'
    bne t3, t1, 61f
    addi s0, s0, 1              # `\|` is `|`
    mv t0, t3
61:
    mv a0, s2
    mv a1, t0
    call sb_byte
    addi s0, s0, 1
    j 6b
7:
    ld t0, SB_PTR(s2)           # trim trailing spaces, then (signature) the backquotes
    ld t1, SB_LEN(s2)
71:
    beqz t1, 72f
    add t2, t0, t1
    lbu t2, -1(t2)
    li t3, ' '
    bne t2, t3, 72f
    addi t1, t1, -1
    j 71b
72:
    li t3, 1
    bne s1, t3, 73f
    li t3, 2
    blt t1, t3, 73f
    lbu t2, 0(t0)
    li t3, 96
    bne t2, t3, 73f
    add t2, t0, t1
    lbu t2, -1(t2)
    bne t2, t3, 73f
    addi t0, t0, 1
    addi t1, t1, -2
73:
    mv a0, t0
    mv a1, t1
    call str_new
    j 9f
8:
    li a0, 0
    li a1, 0
    call str_new
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# long doc_names_has(void *names, void *name)
# 1 when name is one of the ", "-separated names of a builtin row ("upper, lower").
    .globl doc_names_has
doc_names_has:
    ld t5, STR_LEN(a1)
    ld t6, STR_LEN(a0)
    addi t0, a0, STR_DATA       # the current name's start
    add t6, t0, t6              # the end of names
1:
    mv t1, t0                   # its end: the next "," or the end
2:
    bge t1, t6, 3f
    lbu t2, 0(t1)
    li t3, ','
    beq t2, t3, 3f
    addi t1, t1, 1
    j 2b
3:
    sub t2, t1, t0
    bne t2, t5, 5f
    li t2, 0                    # same length: compare the bytes
4:
    bge t2, t5, 8f
    add t3, t0, t2
    lbu t3, 0(t3)
    addi t4, a1, STR_DATA
    add t4, t4, t2
    lbu t4, 0(t4)
    bne t3, t4, 5f
    addi t2, t2, 1
    j 4b
5:
    addi t0, t1, 2              # past ", "
    blt t0, t6, 1b
    li a0, 0
    ret
8:
    li a0, 1
    ret

# long doc_builtins(void *name)
# Prints every builtin row (name 0) or the one with this name; returns how many.
    .globl doc_builtins
doc_builtins:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s1, a0
    li s4, 0
    call doc_builtin_rows
    mv s0, a0
1:
    beqz s0, 9f
    lbu t0, 0(s0)
    li t1, '|'
    bne t0, t1, 9f
    mv a0, s0
    li a1, 0
    call doc_cell
    mv s2, a0                   # names
    beqz s1, 2f
    mv a0, s2
    mv a1, s1
    call doc_names_has
    beqz a0, 8f
2:
    la t0, doc_collect
    ld t0, 0(t0)
    beqz t0, 21f
    mv a0, t0                   # collecting names for a suggestion
    mv a1, s2
    call vec_push
    j 8f
21:
    mv a0, s0
    li a1, 1
    call doc_cell
    mv s3, a0                   # signature
    mv a0, s0
    li a1, 2
    call doc_cell
    sd a0, 0(sp)                # notes
    mv a0, s2
    mv a1, s3
    ld a2, 0(sp)
    call doc_print_builtin
    addi s4, s4, 1
8:
    mv a0, s0
    call doc_next_line
    mv s0, a0
    j 1b
9:
    mv a0, s4
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void doc_print_builtin(void *names, void *signature, void *notes)
    .globl doc_print_builtin
doc_print_builtin:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    call sb_new
    mv s3, a0
    la t0, opt_json
    ld t0, 0(t0)
    beqz t0, 5f
    la a1, j_head
    call sb_cstr
    la a0, w_builtins
    call str_from_cstr
    mv a1, a0
    mv a0, s3
    call json_str_into
    mv a0, s3
    la a1, j_name
    call sb_cstr
    mv a0, s3
    mv a1, s0
    call json_str_into
    mv a0, s3
    la a1, j_item
    call sb_cstr
    mv a0, s3
    la a1, w_builtin
    call sb_cstr
    mv a0, s3
    la a1, j_sig
    call sb_cstr
    mv a0, s3
    mv a1, s1
    call json_str_into
    mv a0, s3
    la a1, j_desc
    call sb_cstr
    mv a0, s3
    mv a1, s2
    call json_str_into
    mv a0, s3
    la a1, j_close
    call sb_cstr
    j 8f
5:
    mv a1, s0                   # NAMES SIGNATURE, then the notes indented
    call sb_str
    mv a0, s3
    li a1, ' '
    call sb_byte
    mv a0, s3
    mv a1, s1
    call sb_str
    mv a0, s3
    li a1, 10
    call sb_byte
    ld t0, STR_LEN(s2)
    beqz t0, 8f
    mv a0, s3
    la a1, s_four
    call sb_cstr
    mv a0, s3
    mv a1, s2
    call sb_str
    mv a0, s3
    li a1, 10
    call sb_byte
8:
    mv a0, s3
    call doc_out
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# ---- the module list ----

# void doc_module_list(void)
# Each standard module with the description from its first line (`# std.json: ...`).
    .globl doc_module_list
doc_module_list:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    la s0, doc_modules
1:
    ld s1, 0(s0)
    beqz s1, 9f
    addi s0, s0, 8
    call sb_new                 # the file's first line after "# std.MODULE: "
    mv s2, a0
    la a1, s_std_slash
    call sb_cstr
    mv a0, s2
    mv a1, s1
    call sb_cstr
    mv a0, s2
    la a1, s_dot_vo
    call sb_cstr
    mv a0, s2
    call sb_finish
    call doc_std_file
    bltz a0, 1b
    call file_get
    ld t0, FILE_TEXT(a0)
    ld t1, FILE_LEN(a0)
    mv t2, t0                   # the end of the first line
2:
    sub t3, t2, t0
    bge t3, t1, 3f
    lbu t4, 0(t2)
    li t5, 10
    beq t4, t5, 3f
    addi t2, t2, 1
    j 2b
3:
    mv a0, s1                   # skip "# std." + name + ": "
    sd t0, 0(sp)
    sd t2, 8(sp)
    call cstr_len
    ld t0, 0(sp)
    ld t2, 8(sp)
    addi a0, a0, 8
    add t0, t0, a0
    bge t0, t2, 4f
    mv a0, t0
    sub a1, t2, t0
    call str_new
    j 5f
4:
    li a0, 0
    li a1, 0
    call str_new
5:
    sd a0, 0(sp)                # the description
    call sb_new
    mv s2, a0
    la a1, s_std_dot
    call sb_cstr
    mv a0, s2
    mv a1, s1
    call sb_cstr
    mv a0, s2
    call sb_finish
    mv a1, a0
    ld a2, 0(sp)
    mv a0, a1
    call doc_print_module
    j 1b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void doc_print_module(void *name, void *unused, void *description)
    .globl doc_print_module
doc_print_module:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a2
    call sb_new
    mv s2, a0
    la t0, opt_json
    ld t0, 0(t0)
    beqz t0, 5f
    la a1, j_head
    call sb_cstr
    mv a0, s2
    mv a1, s0
    call json_str_into
    mv a0, s2
    la a1, j_name
    call sb_cstr
    mv a0, s2
    mv a1, s0
    call json_str_into
    mv a0, s2
    la a1, j_item
    call sb_cstr
    mv a0, s2
    la a1, w_module
    call sb_cstr
    mv a0, s2
    la a1, j_sig
    call sb_cstr
    mv a0, s2
    mv a1, s0
    call json_str_into
    mv a0, s2
    la a1, j_desc
    call sb_cstr
    mv a0, s2
    mv a1, s1
    call json_str_into
    mv a0, s2
    la a1, j_close
    call sb_cstr
    j 8f
5:
    la a1, s_two
    call sb_cstr
    mv a0, s2
    mv a1, s0
    call sb_str
    mv a0, s2
    li a1, ' '
    call sb_byte
    mv a0, s2
    mv a1, s1
    call sb_str
    mv a0, s2
    li a1, 10
    call sb_byte
8:
    mv a0, s2
    call doc_out
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# ---- the command ----

# void doc_not_found(void *name)
# T0010 doc_not_found, exit 64, with the closest documented name when one is within two edits.
    .globl doc_not_found
doc_not_found:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    call vec_new                # every documented name
    la t0, doc_collect
    sd a0, 0(t0)
    mv s1, a0
    li a0, 0
    call doc_builtins
    la s2, doc_modules
1:
    ld a0, 0(s2)
    beqz a0, 2f
    mv s3, a0
    li a1, 0
    call doc_module
    la a0, s_std_dot
    call str_from_cstr
    mv s3, a0
    ld a0, 0(s2)
    call str_from_cstr
    mv a1, a0
    mv a0, s3
    call str_concat
    mv a1, a0
    mv a0, s1
    call vec_push
    addi s2, s2, 8
    j 1b
2:
    li s2, 0                    # the closest: distance below 3, first wins
    li s3, 3
    sd zero, 0(sp)
3:
    ld t0, VEC_N(s1)
    bge s2, t0, 5f
    mv a0, s1
    mv a1, s2
    call vec_get
    sd a0, 0(sp)
    mv a1, s0
    call name_distance
    bge a0, s3, 4f
    mv s3, a0
    ld t0, 0(sp)
    la t1, doc_best
    sd t0, 0(t1)
4:
    addi s2, s2, 1
    j 3b
5:
    mv a1, s0
    la a0, m_not_found
    li a2, 0
    li a3, 0
    call msg
    li t0, 3
    beq s3, t0, 6f
    mv a1, a0
    la t0, doc_best
    ld a2, 0(t0)
    la a0, m_did_you_mean
    li a3, 0
    call msg
6:
    mv a3, a0
    la t0, opt_json
    ld a0, 0(t0)
    la t0, report_fd
    ld a1, 0(t0)
    la a2, code_t10
    li a4, 0
    call toolchain_error
    li a0, EXIT_USAGE
    call vo_exit
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void doc_summary(long items)
    .globl doc_summary
doc_summary:
    la t0, opt_json
    ld t0, 0(t0)
    beqz t0, 9f
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    mv s0, a0
    call sb_new
    sd a0, 0(sp)
    la a1, j_items
    call sb_cstr
    ld a0, 0(sp)
    mv a1, s0
    call int_to_dec
    ld a0, 0(sp)
    call sb_finish
    sd a0, 8(sp)
    call cmd_name
    mv a2, a0
    li a3, 0
    ld a4, 8(sp)
    la t0, opt_json
    ld a0, 0(t0)
    la t0, report_fd
    ld a1, 0(t0)
    call summary_emit
    ld ra, 24(sp)
    ld s0, 16(sp)
    addi sp, sp, 32
9:
    ret

# int cmd_doc(void)
# `vo doc [--json] [NAME]` (contracts/cli.md).
    .globl cmd_doc
cmd_doc:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    la t0, pos_n
    ld t0, 0(t0)
    bnez t0, 1f
    la t0, opt_json             # no NAME: builtins, then modules
    ld t0, 0(t0)
    bnez t0, 11f
    li a0, 1
    la a1, s_h_builtins
    call out_cstr
11:
    li a0, 0
    call doc_builtins
    mv s2, a0
    la t0, opt_json
    ld t0, 0(t0)
    bnez t0, 12f
    li a0, 1
    la a1, s_h_modules
    call out_cstr
12:
    call doc_module_list
    addi a0, s2, 5
    call doc_summary
    j 8f
1:
    li t1, 1
    beq t0, t1, 2f
    la a0, m_doc_args
    call str_from_cstr
    call usage_error
2:
    la t0, pos_args
    ld t0, 0(t0)
    ld s0, 0(t0)                # NAME
    mv a0, s0
    call str_from_cstr
    sd a0, 0(sp)                # NAME as written, for the message
    mv a0, s0
    la a1, s_std_dot
    call doc_starts
    beqz a0, 5f
    addi s1, s0, 4              # std.MODULE[.ITEM]
    mv a0, s1
    li a1, '.'
    call cstr_index_doc
    bltz a0, 3f
    add t0, s1, a0              # split at the dot: module word, item name
    sb zero, 0(t0)
    addi a0, t0, 1
    call str_from_cstr
    mv a1, a0
    mv a0, s1
    call doc_module
    bgtz a0, 7f
    j 6f
3:
    mv a0, s1                   # a module
    li a1, 0
    call doc_module
    blez a0, 6f
    call doc_summary
    j 8f
5:
    mv a0, s0                   # a builtin
    call str_from_cstr
    call doc_builtins
    bgtz a0, 7f
6:
    ld a0, 0(sp)
    call doc_not_found
7:
    li a0, 1
    call doc_summary
8:
    li a0, 0
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# long cstr_index_doc(char *s, int c)
    .globl cstr_index_doc
cstr_index_doc:
    mv t0, a0
1:
    lbu t1, 0(t0)
    beqz t1, 8f
    beq t1, a1, 2f
    addi t0, t0, 1
    j 1b
2:
    sub a0, t0, a0
    ret
8:
    li a0, -1
    ret

    .bss
    .align 3
doc_collect: .dword 0           # a vec: doc_module and doc_builtins collect names instead of printing
doc_best:    .dword 0

    .section .rodata
m_doc_args: .string "`doc` takes at most one name: `vo doc std.json` or `vo doc std.json.parse_json`."
