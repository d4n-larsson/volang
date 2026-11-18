/* Shared C declarations of the vo toolchain (spec 008 research R14, data-model.md).
 *
 * Constants and layouts are defined here (spec 008 T050: formerly src/defs.inc); every struct field
 * is checked against its offset constant, which tools, docs and data-model.md also cite. Generic node and record fields are W
 * unions: one 8-byte word read as an integer, a pointer of the right kind, or float bits. */
#ifndef VO_H
#define VO_H

#include <stddef.h>
#include <stdint.h>

/* ---- constants and layouts (formerly src/defs.inc) ---- */
/* Layouts: specs/001-vo-lang-core/data-model.md. */

/* ---- exit statuses (contracts/cli.md) ---- */
#define EXIT_OK (0)
#define EXIT_FAIL (1)
#define EXIT_USAGE (64)
#define EXIT_CHECK (65)
#define EXIT_NOINPUT (66)
#define EXIT_RUNTIME (70)
#define EXIT_OUTPUT (74) /* T0009: a trace or recording cannot be written (spec 006) */
#define EXIT_TIMEOUT (124)
#define EXIT_INTERNAL (125)

/* ---- Linux flags shared by x86-64, ARM64 and RISC-V (call numbers and O_DIRECTORY, which differs
   on ARM64: src/platform/<arch>.h) ---- */
#define AT_FDCWD (-100)
#define O_RDONLY (0)
#define O_WRCREAT (0x241) /* O_WRONLY | O_CREAT | O_TRUNC */
#define O_NONBLOCK (0x800) /* spec 013: the same on x86-64, ARM64 and RISC-V */
#define O_CLOEXEC (0x80000)
#define O_PATH (0x200000)

/* ---- heap objects: header size:u32 | kind:u32 ---- */
#define OBJ_STR (1)
#define OBJ_LIST (2)
#define OBJ_MAP (3)
#define OBJ_RECORD (4)
#define OBJ_VARIANT (5)
#define OBJ_RAW (6)
#define OBJ_CLOSURE (7)
#define OBJ_BYTES (8) /* a raw buffer that holds no pointers (never scanned) */
#define OBJ_COLL (9)  /* spec 014: a std.collections value (Coll) */
#define OBJ_FREE (15) /* a reclaimed heap slot */
#define OBJ_MARK (0x80000000UL) /* mark bit in the kind word, set only while collecting */

/* ---- memory regions at fixed addresses (spec 003 data-model.md) ---- */
/* All below 2^38, so the map also fits Sv39 RV64 hardware (user space up to 0x3f_ffff_ffff). */
/* The instrumented build (VO_HOSTED, research R9) moves every region above ASan's shadow memory. */
#ifdef VO_HOSTED
#define ADDR_BASE 0x100000000000UL
#else
#define ADDR_BASE 0UL
#endif
#define ADDR_HEAP (ADDR_BASE + 0x1000000000UL) /* collected heap: 64 KiB pages, up to 64 GiB + headroom */
#define ADDR_PTAB (ADDR_BASE + 0x2100000000UL) /* heap page table: 8 bytes per page */
#define ADDR_MARKSTACK (ADDR_BASE + 0x2200000000UL) /* mark stack, 64 MiB reserved */
#define MARKSTACK_SIZE (0x4000000)
#define ADDR_ROOTS (ADDR_BASE + 0x2280000000UL) /* spec 008: root stack of C code (research R4), ROOTS_CAP words */
#define ROOTS_CAP (0x100000) /* 1 Mi entries (8 MiB) */
#define ROOTS_HWM_LIMIT (0x20000) /* stress builds: the suite must stay below 1/8 of ROOTS_CAP */
#define JB_SIZE (216) /* jmp_save buffer: ra, sp, s0-s11, gc_roots_n, fs0-fs11 */
#define ADDR_VSTACK (ADDR_BASE + 0x2300000000UL) /* interpreter value stack, 32 MiB */
#define VSTACK_SIZE (0x2000000)
#define ADDR_PERM (ADDR_BASE + 0x2400000000UL) /* permanent arena, contiguous 16 MiB chunks */
#define ADDR_CODE (ADDR_BASE + 0x2800000000UL) /* compiled code (spec 004): never scanned by the collector */
#define CODE_SIZE (0x10000000) /* 256 MiB of address space, committed only when touched */
#define ADDR_STACK (ADDR_BASE + 0x3000000000UL) /* native stack, 128 MiB (spec 008 R7: the value stack binds first) */
#define STACK_SIZE (0x8000000)
#define MAP_FIXED_NOREPLACE (0x100000)
#define MAP_NORESERVE (0x4000)
/* heap pages and size classes */
#define PAGE_SHIFT (16)
#define PAGE_SIZE (65536)
#define NCLASSES (20)
#define MAX_SMALL (16384)
/* page table entry: kind u8 | class u8 | noscan u8 | dirty u8 | run u32 (head: pages; tail: back to head) */
#define PT_KIND (0)
#define PT_CLASS (1)
#define PT_NOSCAN (2)
#define PT_DIRTY (3)
#define PT_RUN (4)
#define PG_FREE (0)
#define PG_SMALL (1)
#define PG_LARGE (2)
#define PG_TAIL (3)
/* collection policy */
#define GC_MIN_TRIGGER (0x800000) /* 8 MiB */
#define GC_DEFAULT_LIMIT (0x20000000) /* 512 MiB of live data */
#define GC_HEADROOM (0x4000000) /* heap reservation beyond the limit (64 MiB) */
#define GC_RESERVE (0x100000) /* 1 MiB for the out-of-memory report itself */
#define BUDGET_UNITS (65536) /* work units between deadline checks (rt_poll) */
/* str: header(8) | len (u32 + pad, read with ld) | bytes */
#define STR_LEN (8)
#define STR_DATA (16)

/* ---- string builder (arena): ptr | len | cap ---- */
#define SB_PTR (0)
#define SB_LEN (8)
#define SB_CAP (16)
#define SB_SIZE (24)

/* ---- tokens: kind:u16 | flags:u16 | start:u32 | end:u32 | pad:u32 (16 bytes) ---- */
#define TOK_KIND (0)
#define TOK_FLAGS (2)
#define TOK_START (4)
#define TOK_END (8)
#define TOK_SIZE (16)
#define TF_NEWLINE_BEFORE (1) /* a newline precedes this token */

#define T_EOF (0)
#define T_IDENT (1)
#define T_TYPE_IDENT (2)
#define T_INT (3)
#define T_FLOAT (4)
#define T_STRING (5)
#define T_COMMENT (6)
#define T_UNDERSCORE (7)
#define T_ERROR (8)
/* keywords: contiguous, in the order of kw_table (lex.c) */
#define T_KW_FIRST (10)
#define T_FN (10)
#define T_RECORD (11)
#define T_VARIANT (12)
#define T_TEST (13)
#define T_IMPORT (14)
#define T_LET (15)
#define T_VAR (16)
#define T_SET (17)
#define T_IF (18)
#define T_ELSE (19)
#define T_WHILE (20)
#define T_FOR (21)
#define T_IN (22)
#define T_MATCH (23)
#define T_CASE (24)
#define T_RETURN (25)
#define T_BREAK (26)
#define T_CONTINUE (27)
#define T_TRY (28)
#define T_NEW (29)
#define T_AND (30)
#define T_OR (31)
#define T_NOT (32)
#define T_TRUE (33)
#define T_FALSE (34)
#define T_KINT (35)
#define T_KFLOAT (36)
#define T_KBOOL (37)
#define T_KSTR (38)
#define T_KLIST (39)
#define T_KMAP (40)
#define T_KRESULT (41)
#define T_KVOID (42)
#define T_USES (43)
#define T_PROP (44)
#define T_REQUIRES (45)
#define T_ENSURES (46)
#define T_EXPORT (47) /* spec 010 */
#define T_KW_LAST (47)
/* punctuation */
#define T_LPAREN (50)
#define T_RPAREN (51)
#define T_LBRACE (52)
#define T_RBRACE (53)
#define T_LBRACKET (54)
#define T_RBRACKET (55)
#define T_COMMA (56)
#define T_SEMI (57)
#define T_COLON (58)
#define T_DOT (59)
#define T_DOTDOT (60)
#define T_ARROW (61)
#define T_ASSIGN (62)
#define T_EQ (63)
#define T_NE (64)
#define T_LT (65)
#define T_LE (66)
#define T_GT (67)
#define T_GE (68)
#define T_PLUS (69)
#define T_MINUS (70)
#define T_STAR (71)
#define T_SLASH (72)
#define T_PERCENT (73)
#define T_AT (74) /* spec 012: `@` of an annotation */

/* ---- source files (diag.c file table): FILE_SIZE bytes each ---- */
#define FILE_PATH (0) /* str object */
#define FILE_TEXT (8)
#define FILE_LEN (16)
#define FILE_LINES (24) /* u32[] of line-start offsets */
#define FILE_NLINES (32)
#define FILE_TOKENS (40)
#define FILE_AST (48)
#define FILE_DIR (56) /* str object: directory part of the path, with trailing '/' or empty */
#define FILE_SCOPE (64) /* head of the file's top-level symbol chain */
#define FILE_STATE (72) /* import loading: 0 new, 1 loading, 2 done */
#define FILE_GEN (80) /* generation (spec 006 edits: the re-check loads files anew) */
#define FILE_STD (88) /* spec 007: 1 for an embedded standard-library file */
#define FILE_PKG (96) /* spec 007: the package's name (str) for a package file, else 0 */
#define FILE_SIZE (104)
#define MAX_FILES (256)

/* ---- diagnostics ---- */
#define SEV_ERROR (0)
#define SEV_WARNING (1)
#define D_CODE (0) /* cstr, e.g. "E0201" */
#define D_SEV (8)
#define D_FILE (16)
#define D_START (24)
#define D_END (32)
#define D_MSG (40) /* str */
#define D_EXPECTED (48) /* str or 0 */
#define D_FOUND (56) /* str or 0 */
#define D_FIXES (64) /* list of fix records (in order) */
#define D_RELATED (72)
#define D_SEQ (80)
#define D_SCOPE (88) /* W0601: vec of {name str, type} pairs, innermost first, or 0 */
#define D_SIZE (96)
/* fix: next | file | start | end | replacement str */
#define FX_NEXT (0)
#define FX_FILE (8)
#define FX_START (16)
#define FX_END (24)
#define FX_TEXT (32)
#define FIX_SIZE (40)
#define DIAG_CAP_SHOWN (50)

/* ---- growable pointer vector (compiler internals): n | cap | data ---- */
#define VEC_N (0)
#define VEC_CAP (8)
#define VEC_DATA (16)
#define VEC_SIZE (24)

/* ---- AST nodes: 64 bytes. Header: kind:u16 | flags:u16 | file:u32 | type:ptr | start:u32 | end:u32 ---- */
#define N_KIND (0)
#define N_FLAGS (2)
#define N_FILE (4)
#define N_TYPE (8)
#define N_START (16)
#define N_END (20)
#define N_A (24)
#define N_B (32)
#define N_C (40)
#define N_D (48)
#define N_E (56)
#define NODE_SIZE (64)

/* node kinds (fields in comments) */
#define N_ERROR (1)
#define N_FILE_NODE (2) /* A items vec */
#define N_IMPORT (3) /* A segments vec (str), B name node (last segment), D target file id + 1 */
#define N_FN (4) /* A name, B params vec, C return type node|0, D body, E local count */
#define N_PARAM (5) /* A name, B type node, E slot */
#define N_RECORD (6) /* A name, B fields vec */
#define N_FIELD_DECL (7) /* A name, B type node */
#define N_VARIANT (8) /* A name, B cases vec */
#define N_CASE_DECL (9) /* A name, B payload type node|0 */
#define N_TEST (10) /* A name str, B body, E local count */
#define N_COMMENT (11)
#define N_NAME (12) /* A str */
#define N_TYPE_NODE (13) /* A TY_ kind, B name node | elem | key | inner, C value type node */
#define N_BLOCK (14) /* A stmts vec */
#define N_LET (15) /* A name node|0 (for _), B type node|0, C expr, E slot; flags: NF_MUT for var */
#define N_SET (16) /* A place, B expr */
#define N_IF (17) /* A cond, B then block, C else block|if|0 */
#define N_WHILE (18) /* A cond, B body */
#define N_FOR (19) /* A name node, B expr|range start, C range end|0, D body, E slot */
#define N_MATCH (20) /* A scrutinee, B arms vec */
#define N_ARM (21) /* A case name node, B binding name node|0, C body, D case index, E binding slot (-1 none) */
#define N_RETURN (22) /* A expr|0 */
#define N_BREAK (23)
#define N_CONTINUE (24)
#define N_EXPR_STMT (25) /* A call or try expression */
#define N_INT (30) /* A value */
#define N_FLOAT (31) /* A f64 bits */
#define N_STR (32) /* A str object */
#define N_BOOL (33) /* A 0|1 */
#define N_IDENT (34) /* A str, B symbol, C slot */
#define N_CALL (35) /* A name node, B args vec, C fn decl, D builtin id (0 = user fn) */
#define N_VCASE (36) /* A type name node, B case name node, C payload expr|0, D variant decl, E case index */
#define N_FIELD (37) /* A object, B field name node, C field index */
#define N_INDEX (38) /* A object, B index; flags NF_MAP when the object is a map */
#define N_BINARY (39) /* A left, B right, C op token, D OP_ code */
#define N_UNARY (40) /* A operand, C op token, D OP_ code */
#define N_TRY (41) /* A expr */
#define N_LIST (42) /* A elements vec */
#define N_NEW_RECORD (43) /* A type name node, B inits vec, C record decl */
#define N_FINIT (44) /* A name node, B expr, C field index */
#define N_NEW_LIST (45) /* A element type node */
#define N_NEW_MAP (46) /* A key type node, B value type node */
#define N_TPARAM (47) /* A name node, B constraint (TPC_*) */
#define N_USES (48) /* A vec of N_EFFECT */
#define N_EFFECT (49) /* A effect bit (0 = unknown name), B vec of scope N_STR | 0, C effect-variable name node | 0, D name text (str) */
#define N_LAMBDA (50) /* A name node ("anonymous function"; params in N_B like N_FN), B return type node|0, C N_USES|0 (only for E0507), D body, E local count */
#define N_FN_TYPE (51) /* type node: A param type nodes vec, B return type node|0, C N_USES|0 */
#define N_ANNOTATION (53) /* spec 012: `@NAME(STR, ...)`: A name node, B vec of N_STR; an item, before the item it annotates */
#define N_NEW_COLL (54) /* spec 014: `new ordered_map[K, V]` etc.: A the collection type node */
#define N_CALL_VALUE (52) /* A callee expr, B args vec, C how (CV_*), D field index (CV_FIELD) */
#define TPC_NONE (0)
#define TPC_ORDERED (1)
#define TPC_EFFECTS (2)
#define CV_CLOSURE (1)
#define CV_FIELD (2)

/* ---- function side record, reached from N_B of the N_FN name node ---- */
#define FX_TPARAMS (0) /* vec of N_TPARAM | 0 */
#define FX_USES (8) /* N_USES | 0 */
#define FX_DECLARED (16) /* effect set */
#define FX_REQUIRED (24) /* effect set (computed requirement) */
#define FX_DIRECT (32) /* effect set (builtins and value calls in the body) */
#define FX_BAD (40) /* the clause has an unknown effect or invalid scope: no effect reports */
#define FX_CAPS (48) /* anonymous functions: vec of captures {symbol, outer slot, inner slot} */
#define FX_BADCAPS (56) /* anonymous functions: identifiers that capture a `var` (E0505) */
#define FX_CLOSURE (64) /* named functions: the closure that is the function as a value (runtime cache) */
#define FX_CODE (72) /* the compiled function (spec 004): functions, lambdas and tests */
#define FX_PARAMS (80) /* properties: vec of N_PARAM */
#define FX_REQUIRES (88) /* vec of `requires` conditions | 0 */
#define FX_ENSURES (96) /* vec of `ensures` conditions | 0 */
#define FX_HOLES (104) /* 1 when the body or a clause has a hole: no W0201/W0501 */
#define FX_RETSLOT (112) /* slot of `ret` (read only when FX_ENSURES and a return type) */
#define FX_CLAUSE_AT (120) /* source offset of the first contract clause, or 0 */
#define FX_SIZE (128)

/* ---- effect bits and effect sets (effects.c) ---- */
#define EF_CONSOLE (1)
#define EF_INPUT (2)
#define EF_FREAD (4)
#define EF_FWRITE (8)
#define EF_FDELETE (16) /* spec 017; bit order is the canonical order */
#define EF_NET (32)
#define EF_CLOCK (64)
#define EF_RANDOM (128)
#define EF_ENV (256)
#define EF_SERVE (512) /* spec 011 */
#define EF_PROCESS (1024) /* spec 017 */
#define EF_ALL (2047)
#define ES_MASK (0) /* u64 bits */
#define ES_SCOPES (8) /* vec of scope pairs (ESC_*) sorted by (bit, bytes) | 0 */
#define ES_EVARS (16) /* vec of effect-variable N_TPARAM nodes | 0 */
#define ES_ANY (24) /* u64 bits needed in some scope, any scope (builtin uses); never in declarations */
#define ES_SIZE (32)
#define ESC_BIT (0)
#define ESC_STR (8) /* str object */
#define ESC_SIZE (16)

#define NF_MUT (1)
#define NF_MAP (2)
#define NF_DISCARD (4) /* `_` binding */
#define NF_TERMINATES (8) /* statement never falls through (set by the checker) */
#define NF_METHOD (16) /* N_CALL_VALUE written `obj.name(args)`: a field call, or v1's E0110 */
#define NF_FNVAL (32) /* N_IDENT naming a function used as a value (N_D: the declaration) */
#define NF_PROP (64) /* N_TEST that is a property (parameters in FX_PARAMS) */
#define NF_EMPTY_ARM (128) /* spec 009: N_ARM written `case NAME()` */
#define NF_EXPORT (512) /* spec 010: item declared with `export` */
#define NF_EXPECT_OPS (1024) /* spec 018: an `expect` call compiled with the comparison's operands */
#define NF_EXPECT_NEG (2048) /* spec 018: ... of `not (comparison)` */
#define NF_MUT_SKIP (4096)   /* spec 018: a `not` that a mutant child compiles as its operand alone */
#define NF_PAREN (256) /* expression written in parentheses (vo fmt keeps them around `and` in `or`) */

/* builtin ids used outside check_expr.c (all ids: check_expr.c builtin_names) */
#define B_SORT_BY (42)
#define B_MAP (43)
#define B_FILTER (44)
#define B_FOLD (45)
#define B_ANY (46)
#define B_ALL (47)
#define B_TODO (48)
#define B_ASSUME (49)
#define B_REQ_FAIL (50) /* internal: compiled contract checks, never named in source */
#define B_ENS_FAIL (51)
#define B_FLOAT_TEXT (52) /* spec 007: __float_text, callable only from std/ (research R5) */
#define B_HTTP_EXCHANGE (53) /* spec 007: __http_exchange (host, port, request) -> result[str], net */
#define B_TEXT_FLOAT (54) /* spec 007: __text_float (s) -> float, up to 17 significant digits (json) */
#define B_COLL (61) /* spec 014: __coll(OP, a, b, c), std.collections only */
#define B_CMAP (62) /* spec 016: concurrent_map(xs, limit, f) */
#define B_FS (63) /* spec 017: __fs(op, a, b) -> result[str], std.fs only (effects by op, in std/fs.vo) */
#define B_RUN (64) /* spec 017: __run(argv, input, env) -> result[str], process; argv[0] is the program */
#define B_HTTPS (65) /* spec 019: __https_exchange(host, port, request) -> result[str], net, std.http only */
#define B_MATH (66) /* spec 020: __math(op, x, y) -> float, std.math only (src/math.c) */
#define B_MATH_INT (67) /* spec 020: __math_int(op, x) -> int: floor, ceil, round */
#define B_LAST (67) /* the last builtin id with a btab entry */

/* operator codes (set by the checker) */
#define OP_ADD_INT (1)
#define OP_SUB_INT (2)
#define OP_MUL_INT (3)
#define OP_DIV_INT (4)
#define OP_REM_INT (5)
#define OP_ADD_FLOAT (6)
#define OP_SUB_FLOAT (7)
#define OP_MUL_FLOAT (8)
#define OP_DIV_FLOAT (9)
#define OP_CONCAT (10)
#define OP_EQ (11)
#define OP_NE (12)
#define OP_LT_INT (13)
#define OP_LE_INT (14)
#define OP_GT_INT (15)
#define OP_GE_INT (16)
#define OP_LT_FLOAT (17)
#define OP_LE_FLOAT (18)
#define OP_GT_FLOAT (19)
#define OP_GE_FLOAT (20)
#define OP_LT_STR (21)
#define OP_LE_STR (22)
#define OP_GT_STR (23)
#define OP_GE_STR (24)
#define OP_AND (25)
#define OP_OR (26)
#define OP_NEG_INT (27)
#define OP_NEG_FLOAT (28)
#define OP_NOT (29)
#define OP_LT_PARAM (30) /* < <= > >= on an `ordered` type parameter: the kind comes from FR_TARGS */
#define OP_LE_PARAM (31)
#define OP_GT_PARAM (32)
#define OP_GE_PARAM (33)

/* ---- semantic types (interned): kind | a | b | decl ---- */
#define TY_KIND (0)
#define TY_A (8)
#define TY_B (16)
#define TY_DECL (24)
#define TYPE_SIZE (32)
#define TY_INT (1)
#define TY_FLOAT (2)
#define TY_BOOL (3)
#define TY_STR (4)
#define TY_VOID (5)
#define TY_LIST (6)
#define TY_MAP (7)
#define TY_RESULT (8)
#define TY_RECORD (9)
#define TY_VARIANT (10)
#define TY_ERROR (11)
#define TY_NAMED (12) /* syntax only: a TYPE_IDENT before resolution */
#define TY_FUNC (13) /* A param type vec, B return type, DECL effect set */
#define TY_PARAM (14) /* A index, B constraint (TPC_*), DECL owner decl node */
#define TY_EFFSET (15)
/* spec 014: the std.collections types (A key or element type, B value type or 0); numbered above
   TY_VOID for rt_concrete's shortcut */
#define TY_OMAP (16)
#define TY_OSET (17)
#define TY_PQUEUE (18)
#define TY_DEQUE (19)
#define TY_IS_COLL(k) ((k) >= TY_OMAP && (k) <= TY_DEQUE) /* an effect parameter's binding: DECL = the effect set */

/* ---- symbols: name | kind | flags | decl | type | slot | next | name node ---- */
#define SY_NAME (0)
#define SY_KIND (8)
#define SY_FLAGS (16)
#define SY_DECL (24)
#define SY_TYPE (32)
#define SY_SLOT (40)
#define SY_NEXT (48)
#define SY_NODE (56)
#define SY_RENAME (64) /* convention-conforming name (str) when the declared name breaks it, else 0 */
#define SY_EFFECTS (72) /* functions: declared effect set */
#define SY_DEPTH (72) /* locals: lambda nesting depth at definition */
#define SYM_SIZE (80)
#define SYM_FN (1)
#define SYM_RECORD (2)
#define SYM_VARIANT (3)
#define SYM_LOCAL (4)
#define SYM_TPARAM (5) /* a type parameter: SY_TYPE is its TY_PARAM */
#define SYF_MUT (1)
#define SYF_USED (2)

/* ---- runtime objects ---- */
/* list: header | len | cap | data (slot[cap]) */
#define L_LEN (8)
#define L_CAP (16)
#define L_DATA (24)
#define LIST_SIZE (24)
/* map: header | count | cap | index (u32[cap*2]) | entries ({key, value, hash, live}[cap]) | n_entries */
#define M_COUNT (8)
#define M_CAP (16)
#define M_INDEX (24)
#define M_ENTRIES (32)
#define M_USED (40)
#define MAP_SIZE (40)
#define ME_KEY (0)
#define ME_VAL (8)
#define ME_HASH (16)
#define ME_LIVE (20)
#define ME_SIZE (24)
/* variant / result: header | tag | payload */
#define V_TAG (8)
#define V_PAY (16)
#define VARIANT_SIZE (16)
/* record: header | slot[field_count] */
#define R_FIELDS (8)
/* closure: header | fn node | capture count | type args | captures (slot[n]) */
/* capture entries (FX_CAPS of an anonymous function): symbol | slot outside | slot inside */
#define CP_SYM (0)
#define CP_OUTER (8)
#define CP_INNER (16)
#define CP_SIZE (24)
#define CL_FN (8)
#define CL_N (16)
#define CL_TARGS (24)
#define CL_CAPS (32)

/* ---- interpreter frames on the value stack: parent | fn node | call node | slots... ---- */
#define FR_PARENT (0)
#define FR_FN (8)
#define FR_CALL (16)
#define FR_TARGS (24) /* type-argument vec of a generic call | 0 */
#define FR_EFFECTS (32) /* effect set the running code is held to */
#define FR_CODE (40) /* compiled function (spec 004) */
#define FR_RPC (48) /* return pc in the caller's code; 0 = return to the native caller of vm_run */
#define FR_RDST (56) /* caller's slot for the result, or -1 */
#define FR_SLOTS (64)

/* ---- bytecode (spec 004, data-model.md): 32-byte instructions, direct-threaded ---- */
#define I_HANDLER (0) /* handler address */
#define I_A (8) /* i32: usually the destination slot */
#define I_B (12) /* i32: a source slot or the first argument slot */
#define I_C (16) /* i64: a slot, a constant, a jump target, a compiled function, ... */
#define I_NODE (24) /* the source node (spans for errors, static types, cur_node) */
#define I_SIZE (32)
/* compiled function: header, then the instructions */
#define CF_CODE (0) /* address of the first instruction */
#define CF_N (8) /* instruction count */
#define CF_SLOTS (16) /* frame size in slots: locals + temporaries */
#define CF_NPARAMS (24)
#define CF_DECL (32) /* N_FN, N_LAMBDA or N_TEST node */
#define CF_HDR (40)
/* opcodes: indexes into vm_ops (src/vm.c). Slot operands are byte offsets (slot * 8) from FR_SLOTS. */
/* OPC_ADD_INT .. OPC_GE_STR are OP_ADD_INT .. OP_GE_STR + 1, in the same order. */
#define OPC_MOV (0) /* a = b */
#define OPC_LOADK (1) /* a = c (constant) */
#define OPC_ADD_INT (2) /* a = b op c, for every binary OP_ code 1..24 */
#define OPC_GE_STR (25)
#define OPC_NOT (26) /* a = not b */
#define OPC_NEG_INT (27) /* a = -b (R0001 for MIN) */
#define OPC_NEG_FLOAT (28)
#define OPC_JMP (29) /* pc = c */
#define OPC_JT (30) /* if b: pc = c */
#define OPC_JF (31) /* if not b: pc = c */
#define OPC_FOR_RANGE (32) /* counter b, end b+8: if counter >= end: pc = c; else step, a = counter */
#define OPC_FOR_LIST (33) /* list b, index b+8: if index >= len: pc = c; else step, a = element */
#define OPC_INC (34) /* a = a + 1 (loop counters: no overflow possible) */
#define OPC_TICK (35) /* count a step (while loops) */
#define OPC_CALL (36) /* a = call c (declaration) with arguments in b, b+8, ... */
#define OPC_CALL_BUILTIN (37) /* a = builtin node(b, b+8, b+16), c = argument count */
#define OPC_RET (38) /* return b (b < 0: 0) */
#define OPC_CMP_PARAM (39) /* a = b <op> c on an `ordered` type parameter (op from the node) */
#define OPC_NEW_LIST (40) /* a = list_new(c) */
#define OPC_LIST_PUSH (41) /* push(a, b): list literals, after the list exists (as the tree walker) */
#define OPC_NEW_MAP (42) /* a = map_new() */
#define OPC_NEW_RECORD (43) /* a = a record of c fields (initialisers follow, in source order) */
#define OPC_SET_FIELD (44) /* a.(c) = b: c is the field's byte offset in the object */
#define OPC_GET_FIELD (45) /* a = b.(c) */
#define OPC_INDEX_LIST (46) /* a = b[c] (R0003 at the node) */
#define OPC_INDEX_MAP (47) /* a = b[c] (R0004 at the node) */
#define OPC_SET_INDEX_LIST (48) /* a[b] = c (R0003 at the node: the place) */
#define OPC_SET_INDEX_MAP (49) /* a[b] = c (cur_node: the node, the `set` statement) */
#define OPC_NEW_VARIANT (50) /* a = a variant with tag c */
#define OPC_SET_PAYLOAD (51) /* a's payload = b */
#define OPC_JTAG_NE (52) /* if b's tag != a: pc = c */
#define OPC_PAYLOAD (53) /* a = b's payload */
#define OPC_TRY (54) /* b ok: a = its payload; err: return b */
#define OPC_CLOSURE (55) /* a = make_closure(node: the lambda) */
#define OPC_FNVAL (56) /* a = fn_closure(c: the declaration) */
#define OPC_CALL_VALUE (57) /* a = (closure in c)(arguments in b, b+8, ...) */
#define OPC_ADD_INT_K (58) /* a = b op c, c a 32-bit constant (US1: constant operands) */
#define OPC_SUB_INT_K (59)
#define OPC_MUL_INT_K (60)
#define OPC_DIV_INT_K (61) /* c not 0 or -1 */
#define OPC_REM_INT_K (62) /* c not 0 */
#define OPC_EQ_INT (63) /* a = b == c for int and bool (no call) */
#define OPC_NE_INT (64)
/* fused compare-and-branch: if a <cond> b: pc = c (a, b slots); _K: b is a 32-bit constant */
#define OPC_JLT (65)
#define OPC_JLE (66)
#define OPC_JGT (67)
#define OPC_JGE (68)
#define OPC_JEQ (69)
#define OPC_JNE (70)
#define OPC_JLT_K (71)
#define OPC_JLE_K (72)
#define OPC_JGT_K (73)
#define OPC_JGE_K (74)
#define OPC_JEQ_K (75)
#define OPC_JNE_K (76)
#define OPC_FOR_STEP (77) /* range back-edge: b += 1; if b < b+8: step, a = b, pc = c */
#define OPC_FOR_LIST_STEP (78) /* list back-edge: (b+8) += 1; if < len: step, a = element, pc = c */
#define OPC_CALL_FAST (79) /* CALL of a non-generic named function, frame set up inline */
#define OPC_LEN (80) /* a = len(b): STR_LEN, L_LEN and M_COUNT are all at offset 8 */
#define OPC_PUSH (81) /* push(a, b) */
#define OPC_HAS (82) /* a = has(b, c) */
#define OPC_CALL_GENERIC (83) /* a = generic call (arguments in b), c = its call-site record (GS_*) */
#define OPC_TRACE_ENTER (84) /* spec 006: first instruction of every function, only with --trace */
#define OPC_INDEX_COLL (85)     /* spec 014: a = b[c] for an OrderedMap or Deque */
#define OPC_SET_INDEX_COLL (86) /* b... a[b] = c */
#define OPC_SNAPSHOT (87)       /* a = the elements of collection b as a list (for loops) */
#define OPC_NEW_COLL (88) /* spec 014: a = new OrderedMap[...] etc. (the node's type) */
#define OPC_COVER (89)    /* spec 018: coverage point a reached (b: its function's index for reach, or -1) */
#define OPC_COUNT (90)
/* generic call-site record (permanent): rt_targs and rt_es_subst results for the last caller */
#define GS_DECL (0) /* the callee */
#define GS_VALID (8) /* 1 once filled */
#define GS_CTARGS (16) /* key: the caller's FR_TARGS */
#define GS_EFF0 (24) /* key: the effect set before substitution */
#define GS_TARGS (32) /* rt_targs(N_E(call), caller) */
#define GS_EFF (40) /* rt_es_subst(eff0, callee, targs), or 0 */
#define GS_SIZE (48)
#define BI_LEN (16) /* builtin ids (check_expr.c builtin_names) */
#define BI_PUSH (23)
#define BI_HAS (26)
#define BI_EXPECT (29) /* spec 018 */

/* statement results */
#define ST_NORMAL (0)
#define ST_BREAK (1)
#define ST_CONTINUE (2)
#define ST_RETURN (3)

/* ---- --timeout inside the runtime (spec 003 US4) ---- */
/* Long runtime loops spend work units of rt_budget inline (written out at each site, since the */
/* linter does not expand macros): */
/* la t6, rt_budget / ld t5, 0(t6) / addi t5, t5, -N (or sub) / sd t5, 0(t6) / bgtz t5, 1000f / */
/* call rt_poll / 1000: */
/* When the budget runs out, rt_poll checks the clock (R0009 past the deadline). Keep live values in */
/* s registers (or saved) across it. */

/* ---- spec 006: queries and edits (src/query.c, src/edit.c; data-model.md) ---- */
/* reference log entry: one resolved name, logged by the checker while q_collect is 1 */
#define RL_KEY_A (0) /* decl node (items), declaring name node (locals), record/variant decl (members), 0 */
#define RL_KEY_B (8) /* member index, builtin id, name str (unresolved), else -1 */
#define RL_NODE (16) /* the node whose span is the reference */
#define RL_ITEM (24) /* enclosing function/test/property item, or 0 */
#define RL_ROLE (32)
#define RL_TYPE (40) /* declarations: the declared type (vo query type-at), else 0 */
#define RL_SIZE (48)
#define RR_DECL (0)
#define RR_CALL (1)
#define RR_VALUE (2)
#define RR_TYPE (3)
#define RR_IMPORT (4)
#define RR_USE (5)
/* edit change: replace [start, end) of file with new */
#define CH_FILE (0)
#define CH_START (8)
#define CH_END (16)
#define CH_OLD (24) /* str */
#define CH_NEW (32) /* str */
#define CH_SIZE (40)

/* ---- spec 006: recordings (src/replay.c) ---- */
#define EV_KIND (0)
#define EV_NAME (8) /* str (path or variable) or 0 */
#define EV_RESULT (16) /* EVR_* */
#define EV_VALUE (24) /* str, or int for now_ms */
#define EV_BYTES (32) /* 1 when the value was written with "bytes":true */
#define EV_PORT (40) /* spec 007: http events */
#define EV_REQUEST (48) /* spec 009: http events: "METHOD TARGET", or 0 in older recordings */
#define EV_SIZE (64)
#define EVK_READ_LINE (1)
#define EVK_READ_ALL (2)
#define EVK_READ_FILE (3)
#define EVK_WRITE_FILE (4)
#define EVK_ENV (5)
#define EVK_NOW_MS (6)
#define EVK_END (7)
#define EVK_HTTP (8)
#define EVK_SERVE_LISTEN (9)   /* spec 011 */
#define EVK_SERVE_REQUEST (10)
#define EVK_SERVE_RESPONSE (11)
#define EVK_INFO (12) /* spec 017: std.fs and std.process events */
#define EVK_LIST_DIR (13)
#define EVK_APPEND_FILE (14)
#define EVK_MAKE_DIR (15)
#define EVK_DELETE (16)
#define EVK_RENAME (17)
#define EVK_RUN (18)
#define EVR_OK (0)
#define EVR_ERR (1)
#define EVR_DENIED (2)
/* buffered output file (sys.c fbuf_*): traces and recordings */
#define FB_FD (0)
#define FB_BUF (8)
#define FB_LEN (16)
#define FB_ERR (24) /* 1 after a failed write */
#define FB_SIZE (32)
#define FB_CAP (65536)
#if defined(__x86_64__)
#include "platform/x86_64.h"
#elif defined(__riscv)
#include "platform/riscv64.h"
#elif defined(__aarch64__)
#include "platform/aarch64.h"
#else
#error "no platform layer for this architecture (src/platform/)"
#endif

#define NORETURN __attribute__((noreturn))
#define RETURNS_TWICE __attribute__((returns_twice))
#define CHECK_OFFSET(type, field, value) \
    _Static_assert(offsetof(type, field) == (value), #type "." #field " != " #value)
#define CHECK_SIZE(type, value) _Static_assert(sizeof(type) == (value), "sizeof " #type " != " #value)

typedef struct Node Node;
typedef struct Vec Vec;
typedef struct Str Str;
typedef struct Type Type;
typedef struct Sym Sym;
typedef struct EffSet EffSet;
typedef struct Fx Fx;
typedef struct Frame Frame;
typedef struct Instr Instr;
typedef struct CompiledFn CompiledFn;

/* One 8-byte word, read as an integer, a pointer or float bits. */
typedef union W {
    long i;
    unsigned long u;
    void *p;
    char *c;
    Node *n;
    Vec *v;
    Str *s;
    Type *t;
    Sym *sym;
    EffSet *es;
    Fx *fx;
} W;
_Static_assert(sizeof(W) == 8, "W is one word");

/* ---- heap objects: header size:u32 | kind:u32 ---- */
typedef struct ObjHdr {
    uint32_t size;
    uint32_t kind;
} ObjHdr;
CHECK_SIZE(ObjHdr, 8);

/* str: header | len (u32 + pad, read as a word) | bytes (NUL-terminated) */
struct Str {
    ObjHdr hdr;
    long len;
    char data[];
};
CHECK_OFFSET(Str, len, STR_LEN);
CHECK_OFFSET(Str, data, STR_DATA);

/* string builder (arena): ptr | len | cap */
typedef struct SB {
    char *ptr;
    long len;
    long cap;
} SB;
CHECK_OFFSET(SB, ptr, SB_PTR);
CHECK_OFFSET(SB, len, SB_LEN);
CHECK_OFFSET(SB, cap, SB_CAP);
CHECK_SIZE(SB, SB_SIZE);

/* growable pointer vector: n | cap | data */
struct Vec {
    long n;
    long cap;
    W *data;
};
CHECK_OFFSET(Vec, n, VEC_N);
CHECK_OFFSET(Vec, cap, VEC_CAP);
CHECK_OFFSET(Vec, data, VEC_DATA);
CHECK_SIZE(Vec, VEC_SIZE);

/* tokens: kind:u16 | flags:u16 | start:u32 | end:u32 | pad:u32 */
typedef struct Token {
    uint16_t kind;
    uint16_t flags;
    uint32_t start;
    uint32_t end;
    uint32_t pad;
} Token;
CHECK_OFFSET(Token, kind, TOK_KIND);
CHECK_OFFSET(Token, flags, TOK_FLAGS);
CHECK_OFFSET(Token, start, TOK_START);
CHECK_OFFSET(Token, end, TOK_END);
CHECK_SIZE(Token, TOK_SIZE);

/* source files (the diag.c file table) */
typedef struct FileRec {
    Str *path;
    char *text;
    long len;
    uint32_t *lines;
    long nlines;
    Token *tokens;
    Node *ast;
    Str *dir;
    Sym *scope;
    long state;
    long gen;
    long std;
    Str *pkg;
} FileRec;
CHECK_OFFSET(FileRec, path, FILE_PATH);
CHECK_OFFSET(FileRec, text, FILE_TEXT);
CHECK_OFFSET(FileRec, len, FILE_LEN);
CHECK_OFFSET(FileRec, lines, FILE_LINES);
CHECK_OFFSET(FileRec, nlines, FILE_NLINES);
CHECK_OFFSET(FileRec, tokens, FILE_TOKENS);
CHECK_OFFSET(FileRec, ast, FILE_AST);
CHECK_OFFSET(FileRec, dir, FILE_DIR);
CHECK_OFFSET(FileRec, scope, FILE_SCOPE);
CHECK_OFFSET(FileRec, state, FILE_STATE);
CHECK_OFFSET(FileRec, gen, FILE_GEN);
CHECK_OFFSET(FileRec, std, FILE_STD);
CHECK_OFFSET(FileRec, pkg, FILE_PKG);
CHECK_SIZE(FileRec, FILE_SIZE);

/* diagnostics and fixes */
typedef struct Fix {
    struct Fix *next;
    long file;
    long start;
    long end;
    Str *text;
} Fix;
CHECK_OFFSET(Fix, next, FX_NEXT);
CHECK_OFFSET(Fix, file, FX_FILE);
CHECK_OFFSET(Fix, start, FX_START);
CHECK_OFFSET(Fix, end, FX_END);
CHECK_OFFSET(Fix, text, FX_TEXT);
CHECK_SIZE(Fix, FIX_SIZE);

typedef struct Diag {
    const char *code;
    long sev;
    long file;
    long start;
    long end;
    Str *msg;
    Str *expected;
    Str *found;
    W fixes;
    W related;
    long seq;
    Vec *scope;
} Diag;
CHECK_OFFSET(Diag, code, D_CODE);
CHECK_OFFSET(Diag, sev, D_SEV);
CHECK_OFFSET(Diag, file, D_FILE);
CHECK_OFFSET(Diag, start, D_START);
CHECK_OFFSET(Diag, end, D_END);
CHECK_OFFSET(Diag, msg, D_MSG);
CHECK_OFFSET(Diag, expected, D_EXPECTED);
CHECK_OFFSET(Diag, found, D_FOUND);
CHECK_OFFSET(Diag, fixes, D_FIXES);
CHECK_OFFSET(Diag, related, D_RELATED);
CHECK_OFFSET(Diag, seq, D_SEQ);
CHECK_OFFSET(Diag, scope, D_SCOPE);
CHECK_SIZE(Diag, D_SIZE);

/* AST nodes: kind:u16 | flags:u16 | file:u32 | type | start:u32 | end:u32 | a..e */
struct Node {
    uint16_t kind;
    uint16_t flags;
    uint32_t file;
    Type *type;
    uint32_t start;
    uint32_t end;
    W a, b, c, d, e;
};
CHECK_OFFSET(Node, kind, N_KIND);
CHECK_OFFSET(Node, flags, N_FLAGS);
CHECK_OFFSET(Node, file, N_FILE);
CHECK_OFFSET(Node, type, N_TYPE);
CHECK_OFFSET(Node, start, N_START);
CHECK_OFFSET(Node, end, N_END);
CHECK_OFFSET(Node, a, N_A);
CHECK_OFFSET(Node, b, N_B);
CHECK_OFFSET(Node, c, N_C);
CHECK_OFFSET(Node, d, N_D);
CHECK_OFFSET(Node, e, N_E);
CHECK_SIZE(Node, NODE_SIZE);

/* the function side record, reached from N_B of the N_FN name node */
struct Fx {
    Vec *tparams;
    Node *uses;
    EffSet *declared;
    EffSet *required;
    EffSet *direct;
    long bad;
    Vec *caps;
    Vec *badcaps;
    W closure;
    CompiledFn *code;
    Vec *params;
    Vec *requires_;
    Vec *ensures;
    long holes;
    long retslot;
    long clause_at;
};
CHECK_OFFSET(Fx, tparams, FX_TPARAMS);
CHECK_OFFSET(Fx, uses, FX_USES);
CHECK_OFFSET(Fx, declared, FX_DECLARED);
CHECK_OFFSET(Fx, required, FX_REQUIRED);
CHECK_OFFSET(Fx, direct, FX_DIRECT);
CHECK_OFFSET(Fx, bad, FX_BAD);
CHECK_OFFSET(Fx, caps, FX_CAPS);
CHECK_OFFSET(Fx, badcaps, FX_BADCAPS);
CHECK_OFFSET(Fx, closure, FX_CLOSURE);
CHECK_OFFSET(Fx, code, FX_CODE);
CHECK_OFFSET(Fx, params, FX_PARAMS);
CHECK_OFFSET(Fx, requires_, FX_REQUIRES);
CHECK_OFFSET(Fx, ensures, FX_ENSURES);
CHECK_OFFSET(Fx, holes, FX_HOLES);
CHECK_OFFSET(Fx, retslot, FX_RETSLOT);
CHECK_OFFSET(Fx, clause_at, FX_CLAUSE_AT);
CHECK_SIZE(Fx, FX_SIZE);

/* effect sets */
struct EffSet {
    unsigned long mask;
    Vec *scopes;
    Vec *evars;
    unsigned long any;
};
CHECK_OFFSET(EffSet, mask, ES_MASK);
CHECK_OFFSET(EffSet, scopes, ES_SCOPES);
CHECK_OFFSET(EffSet, evars, ES_EVARS);
CHECK_OFFSET(EffSet, any, ES_ANY);
CHECK_SIZE(EffSet, ES_SIZE);

typedef struct EffScope {
    long bit;
    Str *str;
} EffScope;
CHECK_OFFSET(EffScope, bit, ESC_BIT);
CHECK_OFFSET(EffScope, str, ESC_STR);
CHECK_SIZE(EffScope, ESC_SIZE);

/* semantic types (interned): kind | a | b | decl */
struct Type {
    long kind;
    W a;
    W b;
    W decl;
};
CHECK_OFFSET(Type, kind, TY_KIND);
CHECK_OFFSET(Type, a, TY_A);
CHECK_OFFSET(Type, b, TY_B);
CHECK_OFFSET(Type, decl, TY_DECL);
CHECK_SIZE(Type, TYPE_SIZE);

/* symbols */
struct Sym {
    Str *name;
    long kind;
    long flags;
    Node *decl;
    Type *type;
    long slot;
    Sym *next;
    Node *node;
    Str *rename;
    W effects; /* functions: declared effect set; locals: lambda depth (SY_DEPTH) */
};
CHECK_OFFSET(Sym, name, SY_NAME);
CHECK_OFFSET(Sym, kind, SY_KIND);
CHECK_OFFSET(Sym, flags, SY_FLAGS);
CHECK_OFFSET(Sym, decl, SY_DECL);
CHECK_OFFSET(Sym, type, SY_TYPE);
CHECK_OFFSET(Sym, slot, SY_SLOT);
CHECK_OFFSET(Sym, next, SY_NEXT);
CHECK_OFFSET(Sym, node, SY_NODE);
CHECK_OFFSET(Sym, rename, SY_RENAME);
CHECK_OFFSET(Sym, effects, SY_EFFECTS);
CHECK_SIZE(Sym, SYM_SIZE);

/* ---- runtime objects ---- */
typedef struct List {
    ObjHdr hdr;
    long len;
    long cap;
    W *data;
} List;
CHECK_OFFSET(List, len, L_LEN);
CHECK_OFFSET(List, cap, L_CAP);
CHECK_OFFSET(List, data, L_DATA);

/* spec 014: OrderedMap, OrderedSet, PriorityQueue, Deque (src/rt_coll.c) */
typedef struct Coll {
    ObjHdr hdr;
    long count;    /* at offset 8, like a list's length: OPC_LEN */
    long kind;     /* TY_OMAP, TY_OSET, TY_PQUEUE, TY_DEQUE */
    Type *type;    /* the concrete collection type: its key or element type orders it */
    void *cmp;     /* the comparison function (a Closure), or 0 for the built-in order */
    void *root;    /* map and set: the AVL tree */
    W *data;       /* queue: (item, sequence) pairs in heap order; deque: the ring buffer */
    long cap;      /* data capacity in elements (a power of two) */
    long head;     /* deque: the front's index */
    long seq;      /* queue: pushes so far (ties leave in push order) */
} Coll;
#define COLL_SIZE (72) /* the fields after the header */
CHECK_OFFSET(Coll, count, L_LEN);

typedef struct MapEntry {
    W key;
    W val;
    uint32_t hash;
    uint32_t live;
} MapEntry;
CHECK_OFFSET(MapEntry, key, ME_KEY);
CHECK_OFFSET(MapEntry, val, ME_VAL);
CHECK_OFFSET(MapEntry, hash, ME_HASH);
CHECK_OFFSET(MapEntry, live, ME_LIVE);
CHECK_SIZE(MapEntry, ME_SIZE);

typedef struct Map {
    ObjHdr hdr;
    long count;
    long cap;
    uint32_t *index;
    MapEntry *entries;
    long used;
} Map;
CHECK_OFFSET(Map, count, M_COUNT);
CHECK_OFFSET(Map, cap, M_CAP);
CHECK_OFFSET(Map, index, M_INDEX);
CHECK_OFFSET(Map, entries, M_ENTRIES);
CHECK_OFFSET(Map, used, M_USED);

typedef struct Variant {
    ObjHdr hdr;
    long tag;
    W pay;
} Variant;
CHECK_OFFSET(Variant, tag, V_TAG);
CHECK_OFFSET(Variant, pay, V_PAY);

typedef struct Record {
    ObjHdr hdr;
    W fields[];
} Record;
CHECK_OFFSET(Record, fields, R_FIELDS);

typedef struct Capture {
    Sym *sym;
    long outer;
    long inner;
} Capture;
CHECK_OFFSET(Capture, sym, CP_SYM);
CHECK_OFFSET(Capture, outer, CP_OUTER);
CHECK_OFFSET(Capture, inner, CP_INNER);
CHECK_SIZE(Capture, CP_SIZE);

typedef struct Closure {
    ObjHdr hdr;
    Node *fn;
    long n;
    Vec *targs;
    W caps[];
} Closure;
CHECK_OFFSET(Closure, fn, CL_FN);
CHECK_OFFSET(Closure, n, CL_N);
CHECK_OFFSET(Closure, targs, CL_TARGS);
CHECK_OFFSET(Closure, caps, CL_CAPS);

/* interpreter frames on the value stack */
struct Frame {
    Frame *parent;
    Node *fn;
    Node *call;
    Vec *targs;
    EffSet *effects;
    CompiledFn *code;
    Instr *rpc;
    long rdst;
    W slots[];
};
CHECK_OFFSET(Frame, parent, FR_PARENT);
CHECK_OFFSET(Frame, fn, FR_FN);
CHECK_OFFSET(Frame, call, FR_CALL);
CHECK_OFFSET(Frame, targs, FR_TARGS);
CHECK_OFFSET(Frame, effects, FR_EFFECTS);
CHECK_OFFSET(Frame, code, FR_CODE);
CHECK_OFFSET(Frame, rpc, FR_RPC);
CHECK_OFFSET(Frame, rdst, FR_RDST);
CHECK_OFFSET(Frame, slots, FR_SLOTS);

/* bytecode: 32-byte instructions, direct-threaded */
struct Instr {
    void *handler;
    int32_t a;
    int32_t b;
    W c;
    Node *node;
};
CHECK_OFFSET(Instr, handler, I_HANDLER);
CHECK_OFFSET(Instr, a, I_A);
CHECK_OFFSET(Instr, b, I_B);
CHECK_OFFSET(Instr, c, I_C);
CHECK_OFFSET(Instr, node, I_NODE);
CHECK_SIZE(Instr, I_SIZE);

struct CompiledFn {
    Instr *code;
    long n;
    long slots;
    long nparams;
    Node *decl;
};
CHECK_OFFSET(CompiledFn, code, CF_CODE);
CHECK_OFFSET(CompiledFn, n, CF_N);
CHECK_OFFSET(CompiledFn, slots, CF_SLOTS);
CHECK_OFFSET(CompiledFn, nparams, CF_NPARAMS);
CHECK_OFFSET(CompiledFn, decl, CF_DECL);
CHECK_SIZE(CompiledFn, CF_HDR);

typedef struct GenericSite {
    Node *decl;
    long valid;
    Vec *ctargs;
    EffSet *eff0;
    Vec *targs;
    EffSet *eff;
} GenericSite;
CHECK_OFFSET(GenericSite, decl, GS_DECL);
CHECK_OFFSET(GenericSite, valid, GS_VALID);
CHECK_OFFSET(GenericSite, ctargs, GS_CTARGS);
CHECK_OFFSET(GenericSite, eff0, GS_EFF0);
CHECK_OFFSET(GenericSite, targs, GS_TARGS);
CHECK_OFFSET(GenericSite, eff, GS_EFF);
CHECK_SIZE(GenericSite, GS_SIZE);

/* ---- spec 006: query log entries, edit changes, recording events, buffered files ---- */
typedef struct RefLog {
    W key_a;
    W key_b;
    Node *node;
    Node *item;
    long role;
    Type *type;
} RefLog;
CHECK_OFFSET(RefLog, key_a, RL_KEY_A);
CHECK_OFFSET(RefLog, key_b, RL_KEY_B);
CHECK_OFFSET(RefLog, node, RL_NODE);
CHECK_OFFSET(RefLog, item, RL_ITEM);
CHECK_OFFSET(RefLog, role, RL_ROLE);
CHECK_OFFSET(RefLog, type, RL_TYPE);
CHECK_SIZE(RefLog, RL_SIZE);

typedef struct Change {
    long file;
    long start;
    long end;
    Str *old;
    Str *new_;
} Change;
CHECK_OFFSET(Change, file, CH_FILE);
CHECK_OFFSET(Change, start, CH_START);
CHECK_OFFSET(Change, end, CH_END);
CHECK_OFFSET(Change, old, CH_OLD);
CHECK_OFFSET(Change, new_, CH_NEW);
CHECK_SIZE(Change, CH_SIZE);

typedef struct Event {
    long kind;
    Str *name;
    long result;
    W value;
    long bytes;
    long port;
    Str *request;
    long tls; /* spec 019: an http event over HTTPS ("scheme":"https") */
} Event;
CHECK_OFFSET(Event, kind, EV_KIND);
CHECK_OFFSET(Event, name, EV_NAME);
CHECK_OFFSET(Event, result, EV_RESULT);
CHECK_OFFSET(Event, value, EV_VALUE);
CHECK_OFFSET(Event, bytes, EV_BYTES);
CHECK_OFFSET(Event, port, EV_PORT);
CHECK_OFFSET(Event, request, EV_REQUEST);
CHECK_SIZE(Event, EV_SIZE);

typedef struct FBuf {
    long fd;
    char *buf;
    long len;
    long err;
} FBuf;
CHECK_OFFSET(FBuf, fd, FB_FD);
CHECK_OFFSET(FBuf, buf, FB_BUF);
CHECK_OFFSET(FBuf, len, FB_LEN);
CHECK_OFFSET(FBuf, err, FB_ERR);
CHECK_SIZE(FBuf, FB_SIZE);

/* ---- collector roots held by C code (research R4, data-model.md "Root stack") ---- */
#define gc_roots ((void **)ADDR_ROOTS) /* ROOTS_CAP entries, mapped at startup (src/sys.c) */
extern long gc_roots_n;
void gc_roots_overflow(void) NORETURN;

/* Push the address of a local that holds a heap reference across an allocation; pop n. */
#ifdef GC_STRESS_BUILD
#define ROOTS_LIMIT ROOTS_HWM_LIMIT /* the suite must stay within 1/8 of the reservation */
#else
#define ROOTS_LIMIT ROOTS_CAP
#endif
#define GC_ROOT(addr)                                                    \
    do {                                                                 \
        if (gc_roots_n >= ROOTS_LIMIT) gc_roots_overflow();              \
        gc_roots[gc_roots_n++] = (void *)(addr);                         \
    } while (0)
#define GC_UNROOT(count) (gc_roots_n -= (count))

/* ---- embedded documents and standard library (build/embedded.c, tools/embed.py) ---- */
typedef struct StdFile {
    const char *path;
    const char *text;
    long len;
} StdFile;
CHECK_SIZE(StdFile, 24);
extern const char ref_md[];
extern const long ref_md_len;
extern const char errors_md[];
extern const long errors_md_len;
extern const StdFile std_files[];

extern Str *process_name(Str *s); /* effects.c, spec 017 */
extern const unsigned short builtin_effects[B_LAST + 1]; /* check_effects.c: effect bit per builtin id */

/* ---- the C side's own memory helpers (src/vo_c.c): GCC may emit calls to these ---- */
void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);

/* ---- every global function and variable of the toolchain, by file (a definition without its
   declaration here fails -Wmissing-prototypes) ---- */
/* src/ast.c */
extern long cur_file;
extern Vec *vec_new(void);
extern void vec_push(Vec *v, void *item);
extern void *vec_get(Vec *v, long i);
extern long vec_len(Vec *v);
extern Node *node_new(int kind, long start, long end);

/* src/check.c */
extern Type * t_int;
extern Type * t_float;
extern Type * t_bool;
extern Type * t_str;
extern Type * t_void;
extern Type * t_error;
extern Sym * ck_scope;
extern Type * ck_ret;
extern long ck_in_test;
extern long ck_test_checks;
extern long ck_test_holes;
extern long ck_loops;
extern long ck_locals;
extern long ck_mode;
extern long ck_broke;
extern Node * ck_fn;
extern Vec * ck_blk;
extern long ck_blk_idx;
extern void ck_init(void);
extern Type *ty_basic(long kind);
extern Type *ty_make(long kind, void *a, void *b, void *decl);
extern Type *ty_list(Type *elem);
extern Type *ty_result(Type *inner);
extern void ty_name_into(SB *sb, Type *t);
extern Str *ty_name(Type *t);
extern int ty_is(Type *t, long kind);
extern int ty_bad(Type *t);
extern Diag *ck_err(char *code, Node *node, Str *message);
extern Diag *ck_warn(char *code, Node *node, Str *message);
extern Diag *ck_err1(char *code, Node *node, char *tmpl, Str *arg);
extern Diag *ck_mismatch(Node *node, Type *expected, Type *found);
extern Sym *sym_new(Str *name, long kind, Node *decl, Type *type, Node *name_node);
extern void scope_add(Sym *s);
extern Sym *scope_find(Str *name);
extern void scope_leave(Sym *saved);
extern Str *convert_name(Str *name, int upper);
extern void check_naming(Node *name_node, int upper);
extern Type *resolve_type(Node *tn);
extern Type *decl_type(Node *decl);
extern Sym *new_local(Node *name_node, Type *type, int mutable_);
extern int expect_type(Node *node, Type *expected, Type *found);
extern void check_block(Node *block);
extern void mark_terminates(Node *node);
extern int terminates(Node *node);
extern void check_stmt(Node *st);
extern void check_cond(Node *expr);
extern void check_match(Node *match);
extern long count_members(Vec *v);
extern Node *member_at(Vec *v, long index);
extern long member_index(Vec *v, Str *name);
extern Str *case_name(Type *scrutinee, long index);
extern Type *case_payload(Type *scrutinee, long index);
extern long case_index(Type *scrutinee, Str *name);
extern int str_eq_cstr(Str *str, char *s);
extern void check_arm(Node *arm, Type *scrutinee, long *seen);
extern int report_missing_cases(Node *match, Type *type, long *seen, long ncases);
extern void define_item(Node *item);
extern void resolve_item(Node *item);
extern void resolve_item_types(Node *item);
extern void check_fn(Node *fn);
extern void check_test(Node *test);
extern void check_tests_unique(Vec *items);
extern Node *find_main(Vec *items);
extern void check_main(int file, Node *fn);
extern void check_items(int file, int mode);
extern int has_unknown_arm(Node *match);
extern void suggest_case(Diag *d, Type *type, Node *name_node);
extern Str *naming_rename(Node *name_node, int upper);
extern void rename_use(Sym *sym, Node *node);

/* src/check_effects.c */
extern Fx * ck_fx;
extern Fx *item_fx(Node *item);
extern void fx_declare(Node *item);
extern EffSet *fx_build_set(Node *uses, Fx *fx);
extern void fx_unknown_effect(Node *eff);
extern void fx_begin(Node *item);
extern Fx *fx_swap(Fx *fx);
extern void fx_value_call(EffSet *effects);
extern void fx_end(void);
extern void fx_builtin(long id);
extern void fx_call(Node *call, Node *decl);
extern void fx_program_begin(void);
extern Vec *fx_edges(void);
extern void fx_file_begin(void);
extern void fx_file_end(Vec *items);
extern Str *fx_item_name(Node *item);
extern Str *fx_render(EffSet *set);
extern Diag *fx_diag(char *code, int sev, Node *item, char *tmpl, EffSet *set);
extern void fx_fix_clause(Diag *d, Node *item, EffSet *set);
extern void fx_report(Node *item);
extern void fx_related(Diag *d, Fx *fx);

/* src/check_expr.c */
extern Node * ck_stmt_call;
extern int builtin_id(Str *name);
extern Type *code_type(int c);
extern Str *src_text(Node *node);
extern void suggest_conversion(Diag *d, Type *expected, Type *found, Node *node);
extern Type *bad_type(void);
extern Type *err_one_of(Node *node, char *expected_text, Type *found);
extern Type *check_arg(Node *call, long i, Type *expected);
extern int check_argc(Node *call, long want);
extern Type *check_sig_builtin(Node *call, char *sig);
extern Type *check_builtin(Node *call, long id, Type *expected);
extern Type *list_arg(Node *call);
extern Type *map_arg(Node *call);
extern Type *check_call(Node *call, Type *expected);
extern void report_undefined(Str *name, Node *node);
extern int is_placeholder(Sym *sym);
extern int suggest_name(Diag *d, Str *name, Node *node);
extern Type *check_ident(Node *node);
extern Type *check_place(Node *node, void *unused);
extern Type *check_field(Node *node);
extern void suggest_field(Diag *d, Type *record_type, Node *name_node);
extern Type *check_index(Node *node);
extern Type *op_error(Node *node, Type *left, Type *right);
extern Type *check_binary(Node *node);
extern Type *check_unary(Node *node);
extern Type *check_try(Node *node);
extern Type *check_list(Node *node, Type *expected);
extern Node *lookup_decl(Node *name_node, long kind, char *not_kind_msg);
extern Type *check_new_record(Node *node, Type *expected);
extern Str *str_new_copy(Str *str);
extern Type *check_vcase(Node *node, Type *expected);
extern Type *check_expr(Node *node, Type *expected);
extern Type *new_map_type(int unused, Node *node);
extern void suggest_import(Diag *d, Str *name, Node *node);
extern int sym_fits(Sym *sym, Node *node);

/* src/check_func.c */
extern long ck_depth;
extern Node * ck_cur_stmt;
extern void *ck_ctx_at(long level);
extern Type *check_lambda(Node *node);
extern long ck_capture(Sym *sym, Node *ident);
extern long ck_capture_at(Sym *sym, long level);
extern void report_bad_captures(Node *lambda);
extern Str *call_name(Node *call);
extern Type *check_call_value(Node *node, Type *expected);
extern Type *check_method_form(Node *node, Type *expected);
extern Type *check_hof(Node *call, long id);

/* src/check_generic.c */
extern void tparams_push(Vec *tparams, Node *owner);
extern int ty_ordered(Type *t);
extern int check_targ(Type *t, Node *tparam, Node *node, Str *owner_name);
extern Type *ty_argn_error(Node *node, Str *name, long want, long got);
extern Type *resolve_instance(Node *node, Node *decl, int kind, Vec *arg_nodes);
extern Type *field_type_in(Type *record_type, Type *field_type);
extern Vec *bindings_vec(Type **bindings, long n);
extern int bind_rest(Node *node, Vec *tparams, Type **bindings, Str *what);
extern Type *check_generic_call(Node *call, Node *decl, Type *expected);
extern Type *ty_subst_bindings(Type *t, Node *owner, Type **bindings);
extern Type *vcase_generic(Node *node, Node *decl, Node *payload_type_node, Type *expected);
extern int ck_param_order(Type *t, Node *node);
extern void infer_fix(Node *call, Diag *d);
extern Type *infer_from_use(Node *call, Str *name);

/* src/compile.c */
extern void * c_ops;
extern void compile_program(void);
extern void *c_code_alloc(long size);
extern void compile_fn(Node *decl);
extern long c_emit(long opc, long a, long b, long c, Node *node);
extern long c_jump(long opc, long b, long target, Node *node);
extern void c_target(long at, long target);
extern long c_here(void);
extern void c_patch_all(Vec *v, long target);
extern long c_new_temp(void);
extern long c_dst(long dst);
extern void c_unsupported(void);
extern void c_mov(long dst, long src, Node *node);
extern void cblock(Node *block);
extern void cstmt(Node *st);
extern void c_loop_enter(void);
extern void c_loop_leave(long cont, long exit, Vec *outer_brk, Vec *outer_cont);
extern int c_fits32(long v);
extern long c_const_op(Node *binary);
extern int c_int_eq(Node *binary);
extern long ccond(Node *cond);
extern long cexpr(Node *node, long dst);

/* src/data.c */
extern char *catalog_find(char *code);
extern Str *catalog_name(char *code);
extern Str *catalog_section(char *code);
extern int catalog_fixable(char *code);

/* src/diag.c */
extern FileRec file_table[];
extern long file_count;
extern Vec * te_related;
extern long cur_gen;
extern Diag ** diag_arr;
extern long diag_n;
extern long diag_errors;
extern long diag_warnings;
extern long diag_truncated;
extern long diag_mute;
extern int file_add(Str *path, char *text, long len);
extern FileRec *file_get(int id);
extern long pos_line(int file, long off);
extern long line_start(int file, long line);
extern long line_end(int file, long line);
extern long pos_col(int file, long off);
extern void diag_reset(void);
extern long pos_offset(int file, long line, long col);
extern Diag *diag_add(char *code, int sev, int file, long start, long end, Str *message);
extern void diag_set_ef(Diag *d, Str *expected, Str *found);
extern void diag_set_scope(Diag *d, Vec *v);
extern int ranges_touch(long s1, long e1, long s2, long e2);
extern void diag_add_fix(Diag *d, int file, long start, long end, Str *replacement);
extern void diag_add_related(Diag *d, int file, long start, long end, Str *message);
extern int diag_before(Diag *a, Diag *b);
extern void diag_sort(void);
extern void json_pos(SB *sb, int file, long off);
extern void json_span(SB *sb, int file, long start, long end);
extern void json_src_text(SB *sb, int file, long start, long end);
extern int fix_already_shown(Fix *fix);
extern void diag_filter_fixes(Diag *d);
extern void diag_json(SB *sb, Diag *d);
extern void human_loc(SB *sb, int file, long off);
extern void sb_visible(SB *sb, Str *str);
extern void human_snippet(SB *sb, int file, long start, long end);
extern void diag_human(SB *sb, Diag *d);
extern void diag_emit_all(int json, int fd);
extern void summary_emit(int json, int fd, char *command, long files, Str *extra);
extern void toolchain_error(int json, int fd, char *code, Str *message, Str *path);
extern long levenshtein(char *a, long na, char *b, long nb, long max);
extern long name_distance(Str *a, Str *b);
extern Diag *diag_last(void);

/* src/doc.c */
extern Str *doc_line_text(long file, Node *node, long skip);
extern long doc_is_example(long file, Node *node);
extern void doc_comments(long file, Vec *items, long index, Str **out);
extern void doc_out(SB *sb);
extern void doc_tparams(SB *sb, Node *decl);
extern void doc_signature(SB *sb, Node *item);
extern void doc_json_pair(SB *sb, Str *name, Type *type);
extern void doc_json_split(SB *sb, Str *text);
extern void doc_json_piece(SB *sb, Str *text, long from, long to);
extern void doc_json_shape(SB *sb, Node *item);
extern void doc_print_item(Str *module, Node *item, Str *desc, Str *example, long full);
extern long doc_std_file(Str *path);
extern long doc_module(char *module_word, Str *item_name);
extern long doc_file(long file, Str *module, Str *item_name);
extern long serve_address_port(Str *s, long *all);
/* src/serve.c (spec 011) */
extern long serve_guard;
extern long rt_root_frame;
extern int rt_start_cwd_known(void);
extern long rt_open_file(long bit, Str *path, long flags);
extern long rt_open_parent(long bit, Str *path, Str **name, int strict); /* spec 017 */
extern long fs_op_effects(long op);  /* check_effects.c, spec 017 */
extern void fx_builtin_fs(Node *call);
extern void rt_check_effect(long bit); /* rt_effects.c, spec 017: R0010 unless both grants have bit */
extern int rt_bit_scoped(long bit);
extern void io_pend(long kind, Str *name);                               /* replay.c, spec 017 */
extern void io_record(long kind, Str *name, Str *to, List *argv, Variant *r);
extern Variant *io_replay(long kind, Str *name, Str *to, List *argv);
extern Str *rt_resolve_for(long bit, Str *path); /* spec 017 */                                      /* spec 017 */
extern int rt_path_allowed(long bit, Str *path);                         /* spec 017 */
extern Variant *io_fs(long op, Str *a, Str *b);                         /* fs.c, spec 017 */
extern Variant *io_run(List *argv, Str *input, List *env);              /* process.c, spec 017 */
extern long read_whole_fd(long fd, char **out);
extern EffSet *rt_es_grant(EffSet *declared, EffSet *caller);
extern Variant *rt_serve_listen(Str *address);
/* spec 013 (R4): the state of framing one HTTP message (serve.c http_frame) */
typedef struct HttpFrame {
    long scanned;  /* bytes searched for the end of the head */
    long head;     /* the head's length with its blank line, 0 until found */
    long length;   /* Content-Length, -1 when none */
    long chunked;  /* Transfer-Encoding: chunked */
    long trailers; /* the last chunk was read */
    long at;       /* where chunk parsing resumes */
    long overhead; /* chunk framing bytes so far (counted against the head limit) */
    SB *body;      /* the decoded body */
    Str *out;      /* the framed message, when complete */
    char *why;     /* what was wrong, after -400 */
} HttpFrame;
extern long http_frame(HttpFrame *f, char *buf, long n, int eof, int response);
extern Str *rt_http_part(Str *m, long which);
extern Variant *rt_serve_next(long listener);
extern void fx_builtin_literal(Node *call, long id);
extern int ty_mentions_owner(Type *t, Node *owner);
extern void order_error(Type *t, Node *node, char *needed_by);
extern Type *elem_record_type;
extern int needs_context(Node *v);
extern Type *kind_arg(Node *call, long mask, char *what);
extern void rt_serve_reply(Str *text);
extern Variant *rt_serve_call(void *fn, long request, Node *call);
extern long tr_depth_get(void);
extern void tr_depth_set(long depth);
extern Variant *io_serve_listen(Str *address);
extern Variant *io_serve_next(long listener);
extern void io_serve_reply(Str *text);
extern void serve_deadline(long on);
extern void export_needed(char *code, Node *at, Node *decl, Str *message);
extern long doc_package(Str *name);
extern long doc_package_list(void);
extern void doc_package_names(void);
extern char *doc_builtin_rows(void);
extern char *doc_next_line(char *p);
extern long doc_starts(char *p, char *prefix);
extern Str *doc_cell(char *row, long index);
extern long doc_names_has(Str *names, Str *name);
extern long doc_builtins(Str *name);
extern void doc_print_builtin(Str *names, Str *signature, Str *notes);
extern void doc_module_list(void);
extern void doc_print_module(Str *name, void *unused, Str *description);
extern void doc_not_found(Str *name);
extern void doc_summary(long items);
extern int cmd_doc(void);
extern long cstr_index_doc(char *s, int c);

/* src/edit.c */
extern void ed_add(long file, long start, long end, Str *new_text);
extern Str *ed_text(long file);
extern long ed_map(long file, long off, long back);
extern long ed_valid_name(Str *name);
extern void ed_refuse(Str *message);
extern void ed_refuse_arg(char *tmpl, Str *arg);
extern void ed_rename(char *target, char *new_name);
extern RefLog *q_decl_entry(Vec *matches);
extern void ed_effects_line(Node *fn_decl, Str *old_name, Str *new_name);
extern Str *ed_trim(char *p, long n);
extern long ed_line_indent(long file, long off, long *start);
extern Node *q_last_member(Vec *members);
extern void ed_add_field(char *record, Str *spec);
extern void ed_save_errors(void);
extern long ed_recheck(void);
extern long ed_old_file(long new_file);
extern long ed_new_file(long old_file);
extern void ed_map_span_back(long *file, long *start, long *end);
extern long ed_new_errors(void);
extern long ed_match_old(Diag *d);
extern void ed_check_refs(void);
extern long ed_is_site(long new_file, long new_start);
extern void ed_print_change(Change *c);
extern long ed_changed_files(void);
extern void ed_finish(long written);
extern void sb_visible_plain(SB *sb, Str *str);
extern Str *ed_tmp_path(long file);
extern long ed_write_tmp(long file, Str *text);
extern void ed_unlink_tmps(void);
extern void ed_write_all(void);
extern void ed_undo(long failed_file, long upto);
extern int cmd_edit(void);

/* src/effects.c */
extern EffSet *es_new(long mask);
extern char *es_bit_name(long bit);
extern long es_bit_by_name(char *p, long n);
extern int es_scoped(EffSet *set, long bit);
extern void es_put_pair(EffSet *set, long bit, Str *str);
extern int es_add_scope(EffSet *set, long bit, Str *str);
extern int es_pair_covered(EffSet *set, long bit, Str *str);
extern int es_has_pair(EffSet *set, long bit, Str *str);
extern int es_has_evar(EffSet *set, void *param);
extern void es_add_evar(EffSet *set, void *param);
extern long es_evar_count(EffSet *set);
extern EffSet *es_union(EffSet *a, EffSet *b);
extern EffSet *es_missing(EffSet *d, EffSet *r);
extern int es_is_empty(EffSet *set);
extern int es_covers(EffSet *d, EffSet *r);
extern int es_lacks_bits(EffSet *d, EffSet *missing);
extern EffSet *es_unused(EffSet *d, EffSet *r);
extern EffSet *es_minus(EffSet *a, EffSet *b);
extern int es_equal(EffSet *a, EffSet *b);
extern void es_render_scope(SB *sb, Str *str);
extern void es_render(EffSet *set, SB *sb);
extern EffSet *es_subst(EffSet *set, Node *owner, Vec *args);

/* src/effects_record.c */
extern Str * rec_root;
extern Str *rec_relpath(int file);
extern long rec_find(int entry);
extern long find_up(int entry, char *slash_name, void **dir_out);
extern long find_up_dir(Str *dir, char *slash_name, void **dir_out);
extern void *rec_entry(Str *path, Str *name, EffSet *set);
extern int rec_cmp(void *a, void *b);
extern void rec_sort(Vec *v);
extern Vec *rec_entries(Vec *files);
extern long rec_skip_spaces(char *p, long i, long n);
extern EffSet *rec_parse_set(char *p, long n);
extern Vec *rec_parse(int file);
extern void *rec_change(long kind, void *new_entry, void *old_entry, EffSet *delta);
extern void *rec_lookup(Vec *entries, void *entry);
extern Vec *rec_compare(Vec *old, Vec *new_entries);
extern void rec_via(Diag *d, SB *sb, void *entry, EffSet *old_set);
extern void rec_review(int entry, Vec *files);
extern void rec_change_diag(void *change);
extern void rec_line(SB *sb, void *entry);
extern void rec_uses_text(SB *sb, EffSet *set);
extern Str *rec_before_after(void *entry);
extern void rec_print_change(SB *sb, void *change, int json);
extern int rec_command(int entry, Vec *files, int update, int json);

/* src/fmt.c */
extern Str *fmt_file(int file);

/* src/gc.c */
extern long gc_on;
extern long gc_busy;
extern long gc_perm;
extern long gc_limit;
extern long gc_since;
extern long gc_live;
extern long gc_peak;
extern long gc_total;
extern long gc_count;
extern long gc_roots_n;
extern long gc_stress;
extern void gc_reset(void);
extern void gc_init(void);
extern void sample_peak(void);
extern void *gc_alloc_slow(long size, int kind);
extern void *gc_alloc(long size, int kind);
extern void *gc_find(long addr);
extern void *gc_check(void *obj, int kind);
extern void gc_collect(void);
extern void gc_roots_overflow(void);
extern void gc_roots_check(void);
extern void size_into(SB *sb, long bytes);
extern void size_into_up(SB *sb, long bytes);
extern void gc_report(int fd, char *command, void *test);

/* src/interp.c */
extern long cur_frame;
extern long vsp;
extern long vbase;
extern long vstack_end;
extern long ret_val;
extern long deadline_ms;
extern long steps;
extern long rt_budget;
extern long recover_point;
extern long rt_err_code;
extern long rt_err_in_test; /* spec 018 */
extern long rt_err_msg;
extern long rt_err_node;
extern long rt_err_extra;
extern long cur_node;
extern char r0009[];
extern void gc_start(void);
extern void interp_init(void);
extern void rt_oom_live(long in_use);
extern void rt_oom(void);
extern Str *frame_fn_name(Frame *frame);
extern void rt_raise(char *code, Str *message, Node *node);
extern void rt_raise_c(char *code, char *message, Node *node);
extern void rt_div_zero(Node *node);
extern void rt_stack_full(Node *node);
extern void rt_overflow(Node *node);
extern void rt_index_error(Node *node, long index, long length);
extern void rt_key_error(Node *node, long key, Type *key_type);
extern void tick(Node *node);
extern void rt_poll(void);
extern void rt_timeout(Node *node);
extern void check_stack(Node *node);
extern long *local_addr(long slot);
extern long call_fn(Node *fn, Node *call, void *args, long *vals, void *closure);
extern int values_equal(long a, long b, Type *type);
extern int run_main(Node *main_fn);
extern void interp_reset(void);
extern long call_value(Closure *closure, Node *call, long *vals);
extern Closure *make_closure(Node *lambda);
extern Closure *fn_closure(Node *fn);
extern void run_test_body(Node *test);

/* src/lex.c */
extern int is_word_char(int c);
extern int keyword_kind(char *p, long n);
extern Token *lex(int file);

/* src/loader.c */
extern long ld_hard;
extern long file_find(Str *path);
extern Str *import_rel(Node *import);
extern Str *import_path(Node *import, Str *rel, FileRec *importer);
extern long std_lookup(Str *path);
extern void std_runtime_grants(void);
extern Str *str_join2(Str *a, Str *b);
extern long load_file(Str *path);
extern void report_cycle(Node *import, long target);
extern void load_imports(int file);
extern void bind_imports(int file);
extern Vec *ld_files(void);
extern void check_program(int file, int mode);
extern void bind_placeholder(Node *name_node);
extern int file_defines(int file, Str *name);
extern Str *str_join2_c(Str *a, char *b);
extern long path_defines(Str *path, Str *name);
extern Str *import_text_for(Str *rel, Str *name);
extern Str *scan_dir(Str *dir, Str *rel, Str *name, int self);
extern Str *find_import(int file, Str *name);
extern long import_insert_at(int file);

/* src/main.c */
extern long opt_json;
extern long opt_timeout;
extern long opt_mem_stats;
extern long opt_remove;
extern long opt_filter;
extern long opt_check;
extern long opt_update;
extern long prog_argc;
extern long prog_argv;
extern long opt_cmd;
extern long report_fd;
extern long pos_args;
extern long pos_n;
extern long opt_diff;
extern long opt_list; /* spec 018: vo test --list, --coverage, --mutate, --max-mutants, --test, --spec */
extern long opt_coverage;
extern long opt_mutate;
extern long opt_max_mutants;
extern Vec *opt_tests;
extern Vec *opt_specs;
extern long opt_dry_run;
extern long opt_trace;
extern long opt_trace_limit;
extern long opt_record;
extern long path_list;
extern long path_n;
extern int cstr_eq(char *a, char *b);
extern void usage_error(Str *message);
extern void usage_error_arg(char *tmpl, char *arg);
extern void path_push(Str *str);
extern void sort_paths(long from);
extern int ends_with_vo(Str *str);
extern void walk_dir(Str *dirpath, int fd);
extern long walk_strict, walk_failed;
extern Str *walk_root;
extern long pkg_cached(Str *path, char **text);
extern int pkg_contains(Str *real);
extern SB *sb_escape_controls(SB *sb);
extern void coll_reset_active(void);
extern long value_cmp(long a, long b, Type *t);
extern long record_cmp(long a, long b, Type *t);
extern Coll *coll_new(Type *type, struct Closure *cmp);
extern Coll *coll_copy(Coll *c);
extern List *coll_snapshot(Coll *c);
extern List *coll_values(Coll *c);
extern long coll_has(Coll *c, long key);
extern void coll_remove(Coll *c, long key);
extern long coll_get_or(Coll *c, long key, long dflt);
extern void coll_push(Coll *c, long x);
extern Variant *coll_pop(Coll *c);
extern long coll_index(Coll *c, long k, Node *node);
extern void coll_set_index(Coll *c, long k, long v, Node *node);
extern long coll_op(Node *call, long a0, long a1, long a2);
extern int coll_equal(Coll *a, Coll *b, Type *t);
extern long render_depth;
extern void expand_paths(void);
extern long load_source(Str *path);
extern char *cmd_name(void);
extern void need_paths(void);
extern int vo_main(long argc, char **argv, char **env);
extern long opt_spec006(char *flag, char *next);
extern void only_with(char *flag, char *commands);
extern void opt_spec006_check(void);
extern void out_paths_check(void);
extern void out_path_is_source(char *path);
extern void outputs_start(long is_test);
extern int not_implemented(void);
extern int finish_check(long files);
extern int cmd_check(void);
extern int cmd_effects(void);
extern int cmd_explain(void);
extern int cmd_reference(void);
extern int cmd_run(void);
extern int cmd_test(void);
extern int cmd_fmt(void);
extern void fmt_report(Str *path, int changed);
extern void write_file_safely(Str *path, Str *text);

/* src/net.c */
extern Variant *rt_http_exchange(Str *host, long port, Str *request);
extern Variant *io_http_exchange(Str *host, long port, Str *request);

/* src/num.c */
extern void int_to_dec(SB *sb, long v);
extern Str *int_to_str(long v);
extern int dec_to_int(char *p, long n, long *out);
extern long scale10(long bits, long k);
extern long text_to_float(char *p, long n);
extern int float_parse(char *p, long n, long *out);
extern void float_to_text(SB *sb, long bits);
extern Str *float_to_str(long bits);
extern long math_call(long op, long x, long y, Node *node); /* spec 020: src/math.c */
extern long math_int(long op, long x, Node *node);

/* src/packages.c */
extern long pkg_manifest;
extern Str * pkg_dir;
extern Vec * pkg_entries;
extern void pkg_load(Str *dir);
extern char *pkg_token(char *p, char *end);
extern long pkg_name_ok(char *p, long n);
extern long pkg_hex_decode(char *p, char *out);
extern void pkg_parse(long file);
extern Str *path_join(Str *dir, Str *rel);
extern void *pkg_find(Str *name);
extern long pkg_hash(Str *dir, char *out, int keep);
extern Str *pkg_hash_text(char *hash);
extern long pkg_verify(void *entry, Node *node);
extern void *pkg_import_path(Node *import, FileRec *importer);
extern Str *pkg_relpath(Str *path);
extern void *pkg_for_doc(Str *name);
extern Str *pkg_entry_name(void *entry);
extern Str *pkg_doc_path(void *entry, Str *rel);
extern Vec *pkg_doc_modules(void *entry);
extern Str *pkg_rel_dir(Str *from, Str *to);
extern void pkg_sort(void);
extern void pkg_write(void);
extern void pkg_report(char *change, void *entry, Str *old_hash_text);
extern void pkg_fail(char *code, Str *message);
extern int cmd_package(void);

/* src/parse.c */
extern long p_hard;
extern Str *tok_raw(int kind);
extern Node *parse_file(int file);

/* src/prop.c */
extern long pr_args;
extern long pr_best;
extern long pr_cand;
extern long pr_cases;
extern long pr_discarded;
extern long pr_shrinks;
extern long pr_stopped;
extern long pr_starved;
extern char assume_code[];
extern void prop_seed(long seed, long item, long case_no);
extern int prop_generatable(Type *t);
extern long prop_gen(Type *t, long size, long depth);
extern long prop_copy(long value, Type *t);
extern long pg_nmembers(Type *t);
extern Type *pg_member(Type *t, long i);
extern Variant *pg_new_variant(long tag, long payload);
extern long pg_coll_of(Type *t, List *xs, List *vs, int copy);
extern Vec *prop_shrinks(long value, Type *t);
extern int prop_run(Node *test, long item);
extern void prop_json(SB *sb, Node *test, int status);
extern void prop_cases_text(SB *sb);
extern void prop_text(SB *sb, Node *test, int status);

/* src/query.c */
extern long q_collect;
extern Vec * q_log;
extern Node * ck_item;
extern Vec * q_exprs;
extern Node *ck_item_set(Node *item);
extern RefLog *q_note(long key_a, long key_b, Node *node, long role);
extern void q_note_sym(Sym *sym, Node *node, long role);
extern void q_note_decl(Sym *sym);
extern long q_load(char *entry);
extern void q_members_all(void);
extern void q_members(Node *decl);
extern void q_fail(Str *message);
extern void q_fail_code(Str *message, char *code);
extern long q_program_file(Str *path);
extern int path_suffix_match(Str *a, Str *b);
extern long q_parse_pos(char *text, long *file_out);
extern long q_tail_number(char *text, long n, long *before);
extern RefLog *q_entry_at(long file, long off);
extern Node *q_item_named(Str *name, long kinds);
extern void q_ambiguous(Str *name, Vec *decls);
extern Str *q_suggest(Str *name, long kinds, Vec *members);
extern void q_unknown(char *tmpl, Str *name, Str *suggestion);
extern void q_resolve(char *target, long *keys);
extern long cstr_index(char *s, int c);
extern Vec *q_matches(long key_a, long key_b, long calls_only);
extern int q_same_key(long a1, long a2, long b1, long b2);
extern void q_insert_sorted(Vec *v, RefLog *entry);
extern void q_decl_first(Vec *v);
extern void q_line_text(SB *sb, long file, long off);
extern void q_out(SB *sb);
extern void q_ref(RefLog *r, char *target);
extern char *q_role_name(long role);
extern int q_finish(long results, char *noun1, char *noun_n);
extern int cmd_query(void);
extern void df_fail(Str *message);
extern void df_mismatch(long file, long line);
extern void df_verify(long file, long line, char *p, long n);
extern void df_push_key(Vec **vec_global, long file, long line);
extern void df_removed_scan(long file, char *p, long n);
extern long df_number(char **pp, char *end, long *out);
extern void df_range(char **pp, char *end, long *start, long *count);
extern long df_starts(char *p, long n, char *prefix);
extern long df_file(char *p, long n);
extern void df_parse(char *text, long len);
extern long df_count_in(Vec *v, long file, long l1, long l2, long open_start);
extern void im_add(void *item, char *role, void *via);
extern long im_has(void *item);
extern void im_items(void);
extern void im_removed(void);
extern void im_callers(void);
extern Str *im_name(void *x);
extern void im_print(void *entry);
extern long im_count(char *role);
extern void im_count_into(SB *sb, char *key, char *role);
extern void im_print_role(char *role);
extern int cmd_impact(void);
extern void q_note_expr(Node *node);
extern Node *q_expr_at(long file, long off);
extern RefLog *q_decl_of(long key_a, long key_b);
extern int q_type_at(long file, long off);
extern Str *q_type_name(Type *t);

/* src/replay.c */
extern long rcd_mode;
extern void rw(char *s);
extern void rw_dec(long v);
extern long rw_valid_utf8(char *p, long n);
extern void rw_str(char *p, long n, long bytes);
extern void rcd_event(long kind, Str *name, long result, Str *value, long int_value);
extern void rcd_result(long kind, Str *name, Variant *result);
extern long rp_fingerprint(void);
extern long fnv_bytes(long h, char *p, long n);
extern void rw_hex64(long v);
extern void rcd_start(void);
extern long rcd_finish(long status);
extern void rcd_event_raw(long kind, Str *name, long result, Str *value, long int_value);
extern void *io_read_line(Node *node);
extern void *io_read_all(Node *node);
extern void *io_read_file(Str *path);
extern void *io_write_file(Str *path, Str *text);
extern void *io_env(Str *name);
extern long io_now_ms(void);
extern void *rp_result(Event *event);
extern Event *rp_take(long kind, Str *name);
extern Str *rp_describe(char *tmpl, Str *name);
extern void rp_mismatch(long kind, Str *name, Event *event);
extern void rp_mismatch_now(long kind, Str *name);
extern void rp_bad(void);
extern char *rp_ws(char *p, char *end);
extern char *rp_string(char *p, char *end, Str **out);
extern char *rp_int(char *p, char *end, long *out);
extern long rp_key(Str *str);
extern void rp_line(char *p, char *end);
extern long rp_kind_of(Str *str);
extern long rp_result_of(Str *str);
extern void rp_read(char *text, long len);
extern void rp_check_event(Event *e);
extern int cmd_replay(void);
extern void rp_note_changed(void);

/* src/rt_call.c */
extern long builtin_call(Node *call, long a0, long a1, long a2);
extern Type *map_key_type(void);
extern long arg0_kind(void);

/* src/rt_effects.c */
extern long opt_seed;
extern long opt_seed_set;
extern long opt_clock;
extern long opt_clock_set;
extern long clock_fixed;
extern void rt_set_option(long which, long value);
extern void rt_effects_start(int is_test);
extern void pcg_seed(long seed);
extern long pcg_next32(void);
extern long rt_random_int(long lo, long hi);
extern long rt_now_ms(void);
extern Variant *rt_env(Str *name);
extern EffSet *rt_frame_effects(void);
extern void rt_check_bit(long id);
extern void rt_check_name(long bit, Str *name);
extern void rt_check_path(long bit, Str *path);
extern int path_under(Str *path, Str *dir);
extern Str *rt_resolve(Str *path);
extern Str *rt_start_cwd(void);
extern void rt_denied(long bit, Str *target);

/* src/rt_func.c */
extern List *hof_map(List *list, void *fn, Node *call);
extern List *hof_filter(List *list, void *fn, Node *call);
extern long hof_fold(List *list, long init, void *fn, Node *call);
extern int hof_any(List *list, void *fn, Node *call, int all);
extern void hof_merge(long lo, long mid, long hi, long *keys, long *from, long *to, int kind);
extern void hof_sort_by(List *list, void *fn, Node *call);

/* src/rt_io.c */
extern void rt_print(int fd, Str *s, int newline);
extern int stdin_fill(Node *node);
extern Variant *rt_read_line(Node *node);
extern Str *rt_read_all(Node *node);
extern Variant *rt_read_file(Str *path);
extern Variant *rt_write_file(Str *path, Str *text);
extern List *rt_args(void);
extern void rt_exit(long code, Node *node);

/* src/rt_list.c */
extern List *list_new(long cap);
extern void list_push(List *list, long value);
extern void mem_copy8(long *dst, long *src, long bytes);
extern Variant *make_result(int tag, long payload);
extern Variant *make_err(char *message);
extern Variant *list_pop(List *list);
extern List *list_slice(List *list, long a, long b, Node *node);
extern int elem_less(long a, long b, int kind);
extern void list_sort(List *list, int kind);

/* src/rt_map.c */
extern Map *map_new(void);
extern void map_alloc(Map *map, long cap);
extern long map_hash(long key, int key_kind);
extern long map_find(Map *map, long key, Type *key_type);
extern void map_index_insert(Map *map, long entry, long hash);
extern void map_grow(Map *map, Type *key_type);
extern void map_set(Map *map, long key, long value, Type *key_type);
extern void map_remove(Map *map, long key, Type *key_type);
extern List *map_keys(Map *map);

/* src/rt_render.c */
extern long render_cap;
extern void render_str_lit(SB *sb, Str *str);
extern void render_value(SB *sb, long value, Type *type);
extern void render_capped(SB *sb, long value, Type *type);

/* src/rt_str.c */
extern Str *str_concat(Str *a, Str *b);
extern int is_cont(Str *s, long i);
extern Str *rt_slice_str(Str *s, long a, long b, Node *node);
extern long str_find_from(Str *s, Str *sub, long from);
extern long rt_find(Str *s, Str *sub);
extern List *rt_split(Str *s, Str *sep, Node *node);
extern Str *rt_join(List *list, Str *sep);
extern int is_space(int c);
extern Str *rt_trim(Str *s);
extern List *rt_chars(Str *s);
extern Str *bool_str(long b);
extern Variant *rt_parse_int(Str *s);
extern Variant *rt_parse_float(Str *s);
extern long rt_int_of_float(long bits, Node *node);
extern long rt_float_of_int(long i);
extern long rt_abs(long v, int kind, Node *node);
extern long rt_minmax(long a, long b, int kind, int want_max);
extern Str *rt_case(Str *s, int upper);
extern long rt_ord(Str *s, Node *node);
extern Str *rt_chr(long cp, Node *node);

/* src/rt_test.c */
extern long tf_node;
extern long tf_msg;
extern long tf_expected;
extern long tf_actual;
extern void rt_expect(long cond, Node *node);
extern void rt_expect_cmp(long cond, long left, long right, Node *call);
extern int node_is_literal(Node *n);
extern long tf_nops; /* spec 018: operands of a failing comparison */
extern long tf_diff_path; /* spec 018: the first difference of a failing expect_eq */
extern long tf_diff_exp;
extern long tf_diff_act;
extern long tf_diff_char;
extern long tf_diff_len_a;
extern long tf_diff_len_e;
extern long tf_op_text[2];
extern long tf_op_val[2];
extern void rt_expect_eq(long actual, long expected, Node *node);

/* src/sha256.c */
extern void sha256_init(void);
extern void sha256_update(char *p, long n);
extern void sha256_final(char *out);
extern void sha256(char *p, long n, char *out);

/* src/stdprim.c */
extern long std_only_ok(Str *name, Node *node);
extern long std_text_float(Str *s);
extern Str *std_float_text(long bits);

/* src/sys.c */
extern long oom_hook;
extern long stack_floor;
extern long arena_ptr;
extern long arena_cap;
extern long envp;
extern long sys_start(void);
extern void vo_exit(int code);
extern long sys_read(int fd, void *buf, long n);
extern long sys_write(int fd, void *buf, long n);
extern long sys_open(char *path, int flags);
extern Str *tmp_path(Str *path, char *tag);
extern long open_tmp(Str *tmp);
extern long sys_close(int fd);
extern long sys_getdents64(int fd, void *buf, long n);
extern long sys_rename(char *from, char *to);
extern long sys_unlink(char *path);
extern long sys_readlink(char *path, char *buf, long size);
extern long sys_getcwd(char *buf, long size);
extern long sys_clock_ns(void);
extern long sys_clock_ms(void);
extern long sys_clock_real_ms(void);
extern long sys_wait_fd(long fd, long events, long ms);
extern long sys_wait_stdin(long ms);
extern long sys_fork(void);
extern long sys_pipe(int fds[2]);
extern long sys_dup_to(int from, int to);
extern void sys_pipe_size(int fd, long bytes);
extern long sys_wait(long pid, int *status);
extern void sys_kill(long pid);
extern void sys_die_with_parent(long parent);
extern long sys_getpid(void);
/* spec 016: tasks (src/task.c) */
extern long task_depth;
extern long task_ctl;
extern long task_path;
extern long task_recover_base;
extern List *task_map(List *xs, long limit, Closure *fn, Node *call);
extern void task_send(int kind, char *p, long n);
extern void task_exit(long status) NORETURN;
extern void task_jump(long status) NORETURN;
extern void rt_recover(long status) NORETURN;
extern void rt_task_seed(long path);
extern void tr_task_child(void);
extern long tr_task_end(void);
extern void tr_flush(void);
extern void tr_task_record(long index);
extern void tr_take(char *p, long n, long omitted);
extern void rcd_task_child(void);
extern void rcd_task_end(void);
extern void rcd_flush(void);
extern void rcd_take(char *p, long n);
extern long rp_position(void);
extern void rp_set_position(long next);
extern long write_checked(int fd, char *p, long n);
extern void write_all(int fd, char *p, long n);
extern void out_flush(void);
extern void out_bytes(int fd, char *p, long n);
extern void out_cstr(int fd, char *s);
extern void out_str(int fd, void *str);
extern long read_whole_file(char *path, char **out);
extern void *map_fixed(long addr, long size, long flags);
extern void *perm_alloc(long size);
extern void *alloc(long size);
extern void *alloc_bytes(long size);
extern void perm_begin(void);
extern void perm_end(void);
extern void *alloc_obj(int kind, long size);
extern void oom_default(void);
extern void *fbuf_open(char *path);
extern void fbuf_flush(void *fbp);
extern void fbuf_write(void *fbp, char *p, long n);

/* src/testrun.c */
extern long t_passed;
extern long t_failed;
extern long t_errored;
extern long t_timed_out;
extern void report_test(Node *test, int status, long ms);
extern void run_file_tests(int file);
extern long run_one_test(Node *test, long index);
extern void mut_note_test(Node *test, long index, long ms);
extern void mut_note(Node *n, Node *fn, long fn_index);
extern void compile_reset(void);
extern long mutate_run(void);
extern void mut_report(SB *sb);
extern Vec *sel_diff_fns;
#define CAP_MAX 65536 /* spec 018: captured test output per test */
extern char cap_buf[CAP_MAX];
extern long cap_len;
extern long cap_dropped;
extern long cap_on;
extern void cap_bytes(char *p, long n);
extern void tls_init(void); /* spec 019 */
extern int net_wait(long fd, int events, long cap_ms);
extern long net_open(Str *host, long port, Variant **err);
extern int parse_ipv4(char *s, long n, uint8_t *out);
extern Variant *net_err(char *tmpl, Str *host, long port);
extern Variant *tls_exchange(Str *host, long port, Str *request);
extern void tls_add_ca_file(char *path);
extern Variant *io_https_exchange(Str *host, long port, Str *request);
/* spec 018: coverage points (research R8), filled by the compiler with --coverage or --mutate */
#define COV_MAX 65536
enum { COV_FUNCTION, COV_LAMBDA, COV_THEN, COV_ELSE, COV_IF_FALSE, COV_CASE, COV_LOOP };
extern Node *cov_node[COV_MAX];
extern char cov_kind[COV_MAX];
extern Node *cov_owner[COV_MAX];
extern long cov_n;
extern long cov_on;     /* the compiler emits points */
extern char *cov_hit;   /* a shared byte per point */
extern char *cov_reach; /* --mutate: a shared byte per (test, function) */
extern long cov_nfn;    /* functions with a point */
extern long cov_test;   /* the index of the running test, for reach */
extern Node **cov_fns;  /* the functions, by index */
extern void *map_shared(long size);
extern int file_is_library(int file);
extern Str *test_id(Node *test); /* spec 018 */
extern int test_id_eq(Node *test, Str *id);
extern void json_test_id_into(SB *sb, Node *test);
extern int test_selected(Node *test, Vec *items, long index);
extern void impact_for_test(Vec *tests, Vec *fns);
extern void test_select_start(long *ids, long n);
extern void list_file_tests(int file);
extern void list_summary(long files);
extern Str *doc_block(long file, Vec *items, long index);
extern Vec *sel_diff_fns;
extern void test_summary(long files);

/* src/text.c */
extern void mem_copy(char *dst, char *src, long n);
extern int mem_eq(char *a, char *b, long n);
extern long cstr_len(char *s);
extern Str *str_new(char *p, long n);
extern Str *str_from_cstr(char *s);
extern int str_eq(Str *a, Str *b);
extern int str_eq_bytes(Str *s, char *p, long n);
extern int str_cmp(Str *a, Str *b);
extern SB *sb_new(void);
extern void sb_reserve(SB *sb, long n);
extern void sb_byte(SB *sb, int b);
extern void sb_bytes(SB *sb, char *p, long n);
extern void sb_cstr(SB *sb, char *s);
extern void sb_str(SB *sb, Str *str);
extern Str *sb_finish(SB *sb);
extern long utf8_seq_len(char *p, long n);
extern long utf8_validate(char *p, long n);
extern long cp_count(char *p, long n);
extern void json_escape_into(SB *sb, char *p, long n);
extern void json_str_into(SB *sb, Str *str);
extern Str *msg(char *tmpl, Str *a1, Str *a2, Str *a3);

/* src/types.c */
extern Node * ck_exp_tnode;
extern Type *ty_func(Vec *params, Type *ret, EffSet *effects);
extern Type *ty_func_of(Node *fn);
extern int ty_func_fits(Type *value, Type *expected);
extern void ty_func_name_into(SB *sb, Type *t);
extern void ty_effects_error(Node *node, Type *expected, Type *found);
extern Vec *ty_tparams_of(Node *decl);
extern Str *ty_param_name(Type *param);
extern Type *ty_instance(int kind, Node *decl, Vec *args);
extern void ty_args_into(SB *sb, Vec *args);
extern int ty_has_params(Type *t);
extern int ty_vec_has_params(Vec *types);
extern Type *ty_subst(Type *t, Node *owner, Vec *args);
extern Vec *ty_subst_vec(Vec *types, Node *owner, Vec *args);
extern int ty_unify(Type *pattern, Type *t, Node *owner, Type **bindings);
extern long memo_get(long kind, long a, long b);
extern void memo_put(long kind, long a, long b, long value);
extern Type *rt_concrete(Type *t);
extern Vec *rt_targs(Vec *bindings, Frame *caller);
extern EffSet *rt_es_subst(EffSet *set, Node *owner, Vec *args);

/* src/vm.c */
extern void * vm_ops[];
extern void * vm_ops_trace[];
extern long tr_on;
extern long rt_reporting;
extern long rt_err_trace_txt;
extern long rt_err_trace_json;
extern void vm_not_compiled(void);
extern void *vm_push(void *cf_, void *decl_, void *call_, long *vals, void *closure_);
extern long vm_run(void *start);
extern void vm_init(void);
extern void trace_start(long is_test);
extern void tr_call(void *frame_);
extern void tr_return(void *frame_, long value);
extern void tr_test(void *test);
extern void tr_error(char *code, void *message, void *node_);
extern void fb_cstr(void *fb, char *s);
extern void fb_dec(void *fb, long v);
extern void out_unwritable(char *path);
extern long exit_hooks(long status);
extern void frame_values(void *sb_, void *frame_, void *pos, long json);
extern void capture_trace(void *node);


/* POLL n units of the --timeout work budget (BUDGET_UNITS between deadline checks; rt_poll). */
#define POLL(units)                                         \
    do {                                                    \
        if ((rt_budget -= (long)(units)) <= 0) rt_poll(); \
    } while (0)

/* Attributes the generated declarations cannot carry (research R5): jmp_save returns twice, so
   the compiler must keep every local that lives across it in memory or a callee-saved register. */
long jmp_save(void *buf) RETURNS_TWICE;
void jmp_restore(void *buf, long value) NORETURN;

/* The platform layer, src/platform/<arch>.S (contracts/platform-layer.md). */
int vo_main(long argc, char **argv, char **env);
long vo_syscall0(long n);
long vo_syscall1(long n, long a);
long vo_syscall2(long n, long a, long b);
long vo_syscall3(long n, long a, long b, long c);
long vo_syscall4(long n, long a, long b, long c, long d);
long vo_syscall5(long n, long a, long b, long c, long d, long e);
long vo_syscall6(long n, long a, long b, long c, long d, long e, long f);

#endif
