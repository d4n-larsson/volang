# Shared constants for every src/*.s file. Layouts: specs/001-vo-lang-core/data-model.md.

# ---- exit statuses (contracts/cli.md) ----
.equ EXIT_OK,        0
.equ EXIT_FAIL,      1
.equ EXIT_USAGE,     64
.equ EXIT_CHECK,     65
.equ EXIT_NOINPUT,   66
.equ EXIT_RUNTIME,   70
.equ EXIT_TIMEOUT,   124
.equ EXIT_INTERNAL,  125

# ---- Linux riscv64 syscalls ----
.equ SYS_getdents64, 61
.equ SYS_getcwd,     17
.equ SYS_readlinkat, 78
.equ SYS_openat,     56
.equ SYS_close,      57
.equ SYS_read,       63
.equ SYS_write,      64
.equ SYS_ppoll,      73
.equ SYS_fstat,      80
.equ SYS_exit_group, 94
.equ SYS_clock_gettime, 113
.equ SYS_renameat2,  276                 # riscv64 has no plain renameat
.equ SYS_munmap,     215
.equ SYS_mmap,       222
.equ AT_FDCWD,       -100
.equ O_RDONLY,       0
.equ O_WRCREAT,      0x241          # O_WRONLY | O_CREAT | O_TRUNC
.equ O_DIRECTORY,    0x10000

# ---- heap objects: header size:u32 | kind:u32 ----
.equ OBJ_STR,    1
.equ OBJ_LIST,   2
.equ OBJ_MAP,    3
.equ OBJ_RECORD, 4
.equ OBJ_VARIANT,5
.equ OBJ_RAW,    6
.equ OBJ_CLOSURE,7
.equ OBJ_BYTES,  8          # a raw buffer that holds no pointers (never scanned)
.equ OBJ_FREE,   15         # a reclaimed heap slot
.equ OBJ_MARK,   0x80000000 # mark bit in the kind word, set only while collecting

# ---- memory regions at fixed addresses (spec 003 data-model.md) ----
# All below 2^38, so the map also fits Sv39 RV64 hardware (user space up to 0x3f_ffff_ffff).
.equ ADDR_HEAP,      0x1000000000       # collected heap: 64 KiB pages, up to 64 GiB + headroom
.equ ADDR_PTAB,      0x2100000000       # heap page table: 8 bytes per page
.equ ADDR_MARKSTACK, 0x2200000000       # mark stack, 64 MiB reserved
.equ MARKSTACK_SIZE, 0x4000000
.equ ADDR_VSTACK,    0x2300000000       # interpreter value stack, 32 MiB
.equ VSTACK_SIZE,    0x2000000
.equ ADDR_PERM,      0x2400000000       # permanent arena, contiguous 16 MiB chunks
.equ ADDR_CODE,      0x2800000000       # compiled code (spec 004): never scanned by the collector
.equ CODE_SIZE,      0x10000000         # 256 MiB of address space, committed only when touched
.equ ADDR_STACK,     0x3000000000       # native stack, 64 MiB
.equ STACK_SIZE,     0x4000000
.equ MAP_FIXED_NOREPLACE, 0x100000
.equ MAP_NORESERVE,  0x4000
# heap pages and size classes
.equ PAGE_SHIFT,     16
.equ PAGE_SIZE,      65536
.equ NCLASSES,       20
.equ MAX_SMALL,      16384
# page table entry: kind u8 | class u8 | noscan u8 | dirty u8 | run u32 (head: pages; tail: back to head)
.equ PT_KIND, 0
.equ PT_CLASS, 1
.equ PT_NOSCAN, 2
.equ PT_DIRTY, 3
.equ PT_RUN, 4
.equ PG_FREE,  0
.equ PG_SMALL, 1
.equ PG_LARGE, 2
.equ PG_TAIL,  3
# collection policy
.equ GC_MIN_TRIGGER, 0x800000           # 8 MiB
.equ GC_DEFAULT_LIMIT, 0x20000000       # 512 MiB of live data
.equ GC_HEADROOM, 0x4000000             # heap reservation beyond the limit (64 MiB)
.equ GC_RESERVE, 0x100000               # 1 MiB for the out-of-memory report itself
.equ BUDGET_UNITS, 65536                # work units between deadline checks (rt_poll)
# str: header(8) | len (u32 + pad, read with ld) | bytes
.equ STR_LEN,    8
.equ STR_DATA,   16

# ---- string builder (arena): ptr | len | cap ----
.equ SB_PTR, 0
.equ SB_LEN, 8
.equ SB_CAP, 16
.equ SB_SIZE, 24

# ---- tokens: kind:u16 | flags:u16 | start:u32 | end:u32 | pad:u32 (16 bytes) ----
.equ TOK_KIND,  0
.equ TOK_FLAGS, 2
.equ TOK_START, 4
.equ TOK_END,   8
.equ TOK_SIZE,  16
.equ TF_NEWLINE_BEFORE, 1           # a newline precedes this token

.equ T_EOF,        0
.equ T_IDENT,      1
.equ T_TYPE_IDENT, 2
.equ T_INT,        3
.equ T_FLOAT,      4
.equ T_STRING,     5
.equ T_COMMENT,    6
.equ T_UNDERSCORE, 7
.equ T_ERROR,      8
# keywords: contiguous, in the order of kw_table (lex.s)
.equ T_KW_FIRST,   10
.equ T_FN,         10
.equ T_RECORD,     11
.equ T_VARIANT,    12
.equ T_TEST,       13
.equ T_IMPORT,     14
.equ T_LET,        15
.equ T_VAR,        16
.equ T_SET,        17
.equ T_IF,         18
.equ T_ELSE,       19
.equ T_WHILE,      20
.equ T_FOR,        21
.equ T_IN,         22
.equ T_MATCH,      23
.equ T_CASE,       24
.equ T_RETURN,     25
.equ T_BREAK,      26
.equ T_CONTINUE,   27
.equ T_TRY,        28
.equ T_NEW,        29
.equ T_AND,        30
.equ T_OR,         31
.equ T_NOT,        32
.equ T_TRUE,       33
.equ T_FALSE,      34
.equ T_KINT,       35
.equ T_KFLOAT,     36
.equ T_KBOOL,      37
.equ T_KSTR,       38
.equ T_KLIST,      39
.equ T_KMAP,       40
.equ T_KRESULT,    41
.equ T_KVOID,      42
.equ T_USES,       43
.equ T_PROP,       44
.equ T_REQUIRES,   45
.equ T_ENSURES,    46
.equ T_KW_LAST,    46
# punctuation
.equ T_LPAREN,     50
.equ T_RPAREN,     51
.equ T_LBRACE,     52
.equ T_RBRACE,     53
.equ T_LBRACKET,   54
.equ T_RBRACKET,   55
.equ T_COMMA,      56
.equ T_SEMI,       57
.equ T_COLON,      58
.equ T_DOT,        59
.equ T_DOTDOT,     60
.equ T_ARROW,      61
.equ T_ASSIGN,     62
.equ T_EQ,         63
.equ T_NE,         64
.equ T_LT,         65
.equ T_LE,         66
.equ T_GT,         67
.equ T_GE,         68
.equ T_PLUS,       69
.equ T_MINUS,      70
.equ T_STAR,       71
.equ T_SLASH,      72
.equ T_PERCENT,    73

# ---- source files (diag.s file table): 64 bytes each ----
.equ FILE_PATH,   0                 # str object
.equ FILE_TEXT,   8
.equ FILE_LEN,    16
.equ FILE_LINES,  24                # u32[] of line-start offsets
.equ FILE_NLINES, 32
.equ FILE_TOKENS, 40
.equ FILE_AST,    48
.equ FILE_DIR,    56                # str object: directory part of the path, with trailing '/' or empty
.equ FILE_SCOPE,  64                # head of the file's top-level symbol chain
.equ FILE_STATE,  72                # import loading: 0 new, 1 loading, 2 done
.equ FILE_SIZE,   80
.equ MAX_FILES,   256

# ---- diagnostics ----
.equ SEV_ERROR,   0
.equ SEV_WARNING, 1
.equ D_CODE,      0                 # cstr, e.g. "E0201"
.equ D_SEV,       8
.equ D_FILE,      16
.equ D_START,     24
.equ D_END,       32
.equ D_MSG,       40                # str
.equ D_EXPECTED,  48                # str or 0
.equ D_FOUND,     56                # str or 0
.equ D_FIXES,     64                # list of fix records (in order)
.equ D_RELATED,   72
.equ D_SEQ,       80
.equ D_SCOPE,     88                # W0601: vec of {name str, type} pairs, innermost first, or 0
.equ D_SIZE,      96
# fix: next | file | start | end | replacement str
.equ FX_NEXT, 0
.equ FX_FILE, 8
.equ FX_START, 16
.equ FX_END, 24
.equ FX_TEXT, 32
.equ FIX_SIZE, 40
.equ DIAG_CAP_SHOWN, 50

# ---- growable pointer vector (compiler internals): n | cap | data ----
.equ VEC_N, 0
.equ VEC_CAP, 8
.equ VEC_DATA, 16
.equ VEC_SIZE, 24

# ---- AST nodes: 64 bytes. Header: kind:u16 | flags:u16 | file:u32 | type:ptr | start:u32 | end:u32 ----
.equ N_KIND, 0
.equ N_FLAGS, 2
.equ N_FILE, 4
.equ N_TYPE, 8
.equ N_START, 16
.equ N_END, 20
.equ N_A, 24
.equ N_B, 32
.equ N_C, 40
.equ N_D, 48
.equ N_E, 56
.equ NODE_SIZE, 64

# node kinds (fields in comments)
.equ N_ERROR,     1
.equ N_FILE_NODE, 2     # A items vec
.equ N_IMPORT,    3     # A segments vec (str), B name node (last segment), C resolved symbol
.equ N_FN,        4     # A name, B params vec, C return type node|0, D body, E local count
.equ N_PARAM,     5     # A name, B type node, E slot
.equ N_RECORD,    6     # A name, B fields vec
.equ N_FIELD_DECL,7     # A name, B type node
.equ N_VARIANT,   8     # A name, B cases vec
.equ N_CASE_DECL, 9     # A name, B payload type node|0
.equ N_TEST,      10    # A name str, B body, E local count
.equ N_COMMENT,   11
.equ N_NAME,      12    # A str
.equ N_TYPE_NODE, 13    # A TY_ kind, B name node | elem | key | inner, C value type node
.equ N_BLOCK,     14    # A stmts vec
.equ N_LET,       15    # A name node|0 (for _), B type node|0, C expr, E slot; flags: NF_MUT for var
.equ N_SET,       16    # A place, B expr
.equ N_IF,        17    # A cond, B then block, C else block|if|0
.equ N_WHILE,     18    # A cond, B body
.equ N_FOR,       19    # A name node, B expr|range start, C range end|0, D body, E slot
.equ N_MATCH,     20    # A scrutinee, B arms vec
.equ N_ARM,       21    # A case name node, B binding name node|0, C body, D case index, E binding slot (-1 none)
.equ N_RETURN,    22    # A expr|0
.equ N_BREAK,     23
.equ N_CONTINUE,  24
.equ N_EXPR_STMT, 25    # A call or try expression
.equ N_INT,       30    # A value
.equ N_FLOAT,     31    # A f64 bits
.equ N_STR,       32    # A str object
.equ N_BOOL,      33    # A 0|1
.equ N_IDENT,     34    # A str, B symbol, C slot
.equ N_CALL,      35    # A name node, B args vec, C fn decl, D builtin id (0 = user fn)
.equ N_VCASE,     36    # A type name node, B case name node, C payload expr|0, D variant decl, E case index
.equ N_FIELD,     37    # A object, B field name node, C field index
.equ N_INDEX,     38    # A object, B index; flags NF_MAP when the object is a map
.equ N_BINARY,    39    # A left, B right, C op token, D OP_ code
.equ N_UNARY,     40    # A operand, C op token, D OP_ code
.equ N_TRY,       41    # A expr
.equ N_LIST,      42    # A elements vec
.equ N_NEW_RECORD,43    # A type name node, B inits vec, C record decl
.equ N_FINIT,     44    # A name node, B expr, C field index
.equ N_NEW_LIST,  45    # A element type node
.equ N_NEW_MAP,   46    # A key type node, B value type node
.equ N_TPARAM,    47    # A name node, B constraint (TPC_*)
.equ N_USES,      48    # A vec of N_EFFECT
.equ N_EFFECT,    49    # A effect bit (0 = unknown name), B vec of scope N_STR | 0, C effect-variable name node | 0, D name text (str)
.equ N_LAMBDA,    50    # A params vec, B return type node|0, C N_USES|0 (only for E0507), D body, E local count
.equ N_FN_TYPE,   51    # type node: A param type nodes vec, B return type node|0, C N_USES|0
.equ N_CALL_VALUE,52    # A callee expr, B args vec, C how (CV_*), D field index (CV_FIELD)
.equ TPC_NONE,    0
.equ TPC_ORDERED, 1
.equ TPC_EFFECTS, 2
.equ CV_CLOSURE,  1
.equ CV_FIELD,    2

# ---- function side record, reached from N_B of the N_FN name node ----
.equ FX_TPARAMS,  0     # vec of N_TPARAM | 0
.equ FX_USES,     8     # N_USES | 0
.equ FX_DECLARED, 16    # effect set
.equ FX_REQUIRED, 24    # effect set (computed requirement)
.equ FX_DIRECT,   32    # effect set (builtins and value calls in the body)
.equ FX_BAD,      40    # the clause has an unknown effect or invalid scope: no effect reports
.equ FX_CAPS,     48    # anonymous functions: vec of captures {symbol, outer slot, inner slot}
.equ FX_BADCAPS,  56    # anonymous functions: identifiers that capture a `var` (E0505)
.equ FX_CLOSURE,  64    # named functions: the closure that is the function as a value (runtime cache)
.equ FX_CODE,     72    # the compiled function (spec 004): functions, lambdas and tests
.equ FX_PARAMS,   80    # properties: vec of N_PARAM
.equ FX_REQUIRES, 88    # vec of `requires` conditions | 0
.equ FX_ENSURES,  96    # vec of `ensures` conditions | 0
.equ FX_HOLES,    104   # 1 when the body or a clause has a hole: no W0201/W0501
.equ FX_RETSLOT,  112   # slot of `ret` (read only when FX_ENSURES and a return type)
.equ FX_CLAUSE_AT, 120  # source offset of the first contract clause, or 0
.equ FX_SIZE,     128

# ---- effect bits and effect sets (effects.s) ----
.equ EF_CONSOLE, 1
.equ EF_INPUT,   2
.equ EF_FREAD,   4
.equ EF_FWRITE,  8
.equ EF_NET,     16
.equ EF_CLOCK,   32
.equ EF_RANDOM,  64
.equ EF_ENV,     128
.equ EF_ALL,     255
.equ ES_MASK,    0      # u64 bits
.equ ES_SCOPES,  8      # vec of scope pairs (ESC_*) sorted by (bit, bytes) | 0
.equ ES_EVARS,   16     # vec of effect-variable N_TPARAM nodes | 0
.equ ES_ANY,     24     # u64 bits needed in some scope, any scope (builtin uses); never in declarations
.equ ES_SIZE,    32
.equ ESC_BIT,    0
.equ ESC_STR,    8      # str object
.equ ESC_SIZE,   16

.equ NF_MUT,     1
.equ NF_MAP,     2
.equ NF_DISCARD, 4      # `_` binding
.equ NF_TERMINATES, 8   # statement never falls through (set by the checker)
.equ NF_METHOD,  16     # N_CALL_VALUE written `obj.name(args)`: a field call, or v1's E0110
.equ NF_FNVAL,   32     # N_IDENT naming a function used as a value (N_D: the declaration)
.equ NF_PROP,    64     # N_TEST that is a property (parameters in FX_PARAMS)

# builtin ids used outside check_expr.s (all ids: check_expr.s builtin_names)
.equ B_SORT_BY, 42
.equ B_MAP,     43
.equ B_FILTER,  44
.equ B_FOLD,    45
.equ B_ANY,     46
.equ B_ALL,     47
.equ B_TODO,    48
.equ B_ASSUME,  49
.equ B_REQ_FAIL, 50     # internal: compiled contract checks, never named in source
.equ B_ENS_FAIL, 51

# operator codes (set by the checker)
.equ OP_ADD_INT, 1
.equ OP_SUB_INT, 2
.equ OP_MUL_INT, 3
.equ OP_DIV_INT, 4
.equ OP_REM_INT, 5
.equ OP_ADD_FLOAT, 6
.equ OP_SUB_FLOAT, 7
.equ OP_MUL_FLOAT, 8
.equ OP_DIV_FLOAT, 9
.equ OP_CONCAT, 10
.equ OP_EQ, 11
.equ OP_NE, 12
.equ OP_LT_INT, 13
.equ OP_LE_INT, 14
.equ OP_GT_INT, 15
.equ OP_GE_INT, 16
.equ OP_LT_FLOAT, 17
.equ OP_LE_FLOAT, 18
.equ OP_GT_FLOAT, 19
.equ OP_GE_FLOAT, 20
.equ OP_LT_STR, 21
.equ OP_LE_STR, 22
.equ OP_GT_STR, 23
.equ OP_GE_STR, 24
.equ OP_AND, 25
.equ OP_OR, 26
.equ OP_NEG_INT, 27
.equ OP_NEG_FLOAT, 28
.equ OP_NOT, 29
.equ OP_LT_PARAM, 30     # < <= > >= on an `ordered` type parameter: the kind comes from FR_TARGS
.equ OP_LE_PARAM, 31
.equ OP_GT_PARAM, 32
.equ OP_GE_PARAM, 33

# ---- semantic types (interned): kind | a | b | decl ----
.equ TY_KIND, 0
.equ TY_A, 8
.equ TY_B, 16
.equ TY_DECL, 24
.equ TYPE_SIZE, 32
.equ TY_INT, 1
.equ TY_FLOAT, 2
.equ TY_BOOL, 3
.equ TY_STR, 4
.equ TY_VOID, 5
.equ TY_LIST, 6
.equ TY_MAP, 7
.equ TY_RESULT, 8
.equ TY_RECORD, 9
.equ TY_VARIANT, 10
.equ TY_ERROR, 11
.equ TY_NAMED, 12       # syntax only: a TYPE_IDENT before resolution
.equ TY_FUNC, 13        # A param type vec, B return type, DECL effect set
.equ TY_PARAM, 14       # A index, B constraint (TPC_*), DECL owner decl node
.equ TY_EFFSET, 15      # an effect parameter's binding: DECL = the effect set

# ---- symbols: name | kind | flags | decl | type | slot | next | name node ----
.equ SY_NAME, 0
.equ SY_KIND, 8
.equ SY_FLAGS, 16
.equ SY_DECL, 24
.equ SY_TYPE, 32
.equ SY_SLOT, 40
.equ SY_NEXT, 48
.equ SY_NODE, 56
.equ SY_RENAME, 64      # convention-conforming name (str) when the declared name breaks it, else 0
.equ SY_EFFECTS, 72     # functions: declared effect set
.equ SY_DEPTH, 72       # locals: lambda nesting depth at definition
.equ SYM_SIZE, 80
.equ SYM_FN, 1
.equ SYM_RECORD, 2
.equ SYM_VARIANT, 3
.equ SYM_LOCAL, 4
.equ SYM_TPARAM, 5      # a type parameter: SY_TYPE is its TY_PARAM
.equ SYF_MUT, 1
.equ SYF_USED, 2

# ---- runtime objects ----
# list: header | len | cap | data (slot[cap])
.equ L_LEN, 8
.equ L_CAP, 16
.equ L_DATA, 24
.equ LIST_SIZE, 24
# map: header | count | cap | index (u32[cap*2]) | entries ({key, value, hash, live}[cap]) | n_entries
.equ M_COUNT, 8
.equ M_CAP, 16
.equ M_INDEX, 24
.equ M_ENTRIES, 32
.equ M_USED, 40
.equ MAP_SIZE, 40
.equ ME_KEY, 0
.equ ME_VAL, 8
.equ ME_HASH, 16
.equ ME_LIVE, 20
.equ ME_SIZE, 24
# variant / result: header | tag | payload
.equ V_TAG, 8
.equ V_PAY, 16
.equ VARIANT_SIZE, 16
# record: header | slot[field_count]
.equ R_FIELDS, 8
# closure: header | fn node | capture count | type args | captures (slot[n])
# capture entries (FX_CAPS of an anonymous function): symbol | slot outside | slot inside
.equ CP_SYM, 0
.equ CP_OUTER, 8
.equ CP_INNER, 16
.equ CP_SIZE, 24
.equ CL_FN, 8
.equ CL_N, 16
.equ CL_TARGS, 24
.equ CL_CAPS, 32

# ---- interpreter frames on the value stack: parent | fn node | call node | slots... ----
.equ FR_PARENT, 0
.equ FR_FN, 8
.equ FR_CALL, 16
.equ FR_TARGS, 24       # type-argument vec of a generic call | 0
.equ FR_EFFECTS, 32     # effect set the running code is held to
.equ FR_CODE, 40        # compiled function (spec 004)
.equ FR_RPC, 48         # return pc in the caller's code; 0 = return to the native caller of vm_run
.equ FR_RDST, 56        # caller's slot for the result, or -1
.equ FR_SLOTS, 64

# ---- bytecode (spec 004, data-model.md): 32-byte instructions, direct-threaded ----
.equ I_HANDLER, 0       # handler address
.equ I_A, 8             # i32: usually the destination slot
.equ I_B, 12            # i32: a source slot or the first argument slot
.equ I_C, 16            # i64: a slot, a constant, a jump target, a compiled function, ...
.equ I_NODE, 24         # the source node (spans for errors, static types, cur_node)
.equ I_SIZE, 32
# compiled function: header, then the instructions
.equ CF_CODE, 0         # address of the first instruction
.equ CF_N, 8            # instruction count
.equ CF_SLOTS, 16       # frame size in slots: locals + temporaries
.equ CF_NPARAMS, 24
.equ CF_DECL, 32        # N_FN, N_LAMBDA or N_TEST node
.equ CF_HDR, 40
# opcodes: indexes into vm_ops (src/vm.s). Slot operands are byte offsets (slot * 8) from FR_SLOTS.
# OPC_ADD_INT .. OPC_GE_STR are OP_ADD_INT .. OP_GE_STR + 1, in the same order.
.equ OPC_MOV, 0          # a = b
.equ OPC_LOADK, 1        # a = c (constant)
.equ OPC_ADD_INT, 2      # a = b op c, for every binary OP_ code 1..24
.equ OPC_GE_STR, 25
.equ OPC_NOT, 26         # a = not b
.equ OPC_NEG_INT, 27     # a = -b (R0001 for MIN)
.equ OPC_NEG_FLOAT, 28
.equ OPC_JMP, 29         # pc = c
.equ OPC_JT, 30          # if b: pc = c
.equ OPC_JF, 31          # if not b: pc = c
.equ OPC_FOR_RANGE, 32   # counter b, end b+8: if counter >= end: pc = c; else step, a = counter
.equ OPC_FOR_LIST, 33    # list b, index b+8: if index >= len: pc = c; else step, a = element
.equ OPC_INC, 34         # a = a + 1 (loop counters: no overflow possible)
.equ OPC_TICK, 35        # count a step (while loops)
.equ OPC_CALL, 36        # a = call c (declaration) with arguments in b, b+8, ...
.equ OPC_CALL_BUILTIN, 37 # a = builtin node(b, b+8, b+16), c = argument count
.equ OPC_RET, 38         # return b (b < 0: 0)
.equ OPC_CMP_PARAM, 39   # a = b <op> c on an `ordered` type parameter (op from the node)
.equ OPC_NEW_LIST, 40     # a = list_new(c)
.equ OPC_LIST_PUSH, 41    # push(a, b): list literals, after the list exists (as the tree walker)
.equ OPC_NEW_MAP, 42      # a = map_new()
.equ OPC_NEW_RECORD, 43   # a = a record of c fields (initialisers follow, in source order)
.equ OPC_SET_FIELD, 44    # a.(c) = b: c is the field's byte offset in the object
.equ OPC_GET_FIELD, 45    # a = b.(c)
.equ OPC_INDEX_LIST, 46   # a = b[c] (R0003 at the node)
.equ OPC_INDEX_MAP, 47    # a = b[c] (R0004 at the node)
.equ OPC_SET_INDEX_LIST, 48 # a[b] = c (R0003 at the node: the place)
.equ OPC_SET_INDEX_MAP, 49  # a[b] = c (cur_node: the node, the `set` statement)
.equ OPC_NEW_VARIANT, 50  # a = a variant with tag c
.equ OPC_SET_PAYLOAD, 51  # a's payload = b
.equ OPC_JTAG_NE, 52      # if b's tag != a: pc = c
.equ OPC_PAYLOAD, 53      # a = b's payload
.equ OPC_TRY, 54          # b ok: a = its payload; err: return b
.equ OPC_CLOSURE, 55      # a = make_closure(node: the lambda)
.equ OPC_FNVAL, 56        # a = fn_closure(c: the declaration)
.equ OPC_CALL_VALUE, 57   # a = (closure in c)(arguments in b, b+8, ...)
.equ OPC_ADD_INT_K, 58   # a = b op c, c a 32-bit constant (US1: constant operands)
.equ OPC_SUB_INT_K, 59
.equ OPC_MUL_INT_K, 60
.equ OPC_DIV_INT_K, 61    # c not 0 or -1
.equ OPC_REM_INT_K, 62    # c not 0
.equ OPC_EQ_INT, 63       # a = b == c for int and bool (no call)
.equ OPC_NE_INT, 64
# fused compare-and-branch: if a <cond> b: pc = c (a, b slots); _K: b is a 32-bit constant
.equ OPC_JLT, 65
.equ OPC_JLE, 66
.equ OPC_JGT, 67
.equ OPC_JGE, 68
.equ OPC_JEQ, 69
.equ OPC_JNE, 70
.equ OPC_JLT_K, 71
.equ OPC_JLE_K, 72
.equ OPC_JGT_K, 73
.equ OPC_JGE_K, 74
.equ OPC_JEQ_K, 75
.equ OPC_JNE_K, 76
.equ OPC_FOR_STEP, 77     # range back-edge: b += 1; if b < b+8: step, a = b, pc = c
.equ OPC_FOR_LIST_STEP, 78 # list back-edge: (b+8) += 1; if < len: step, a = element, pc = c
.equ OPC_CALL_FAST, 79    # CALL of a non-generic named function, frame set up inline
.equ OPC_LEN, 80         # a = len(b): STR_LEN, L_LEN and M_COUNT are all at offset 8
.equ OPC_PUSH, 81         # push(a, b)
.equ OPC_HAS, 82         # a = has(b, c)
.equ OPC_CALL_GENERIC, 83 # a = generic call (arguments in b), c = its call-site record (GS_*)
.equ OPC_COUNT, 84
# generic call-site record (permanent): rt_targs and rt_es_subst results for the last caller
.equ GS_DECL, 0           # the callee
.equ GS_VALID, 8          # 1 once filled
.equ GS_CTARGS, 16        # key: the caller's FR_TARGS
.equ GS_EFF0, 24          # key: the effect set before substitution
.equ GS_TARGS, 32         # rt_targs(N_E(call), caller)
.equ GS_EFF, 40           # rt_es_subst(eff0, callee, targs), or 0
.equ GS_SIZE, 48
.equ BI_LEN, 16           # builtin ids (check_expr.s builtin_names)
.equ BI_PUSH, 23
.equ BI_HAS, 26

# statement results
.equ ST_NORMAL, 0
.equ ST_BREAK, 1
.equ ST_CONTINUE, 2
.equ ST_RETURN, 3

# ---- --timeout inside the runtime (spec 003 US4) ----
# Long runtime loops spend work units of rt_budget inline (written out at each site, since the
# linter does not expand macros):
#     la t6, rt_budget / ld t5, 0(t6) / addi t5, t5, -N (or sub) / sd t5, 0(t6) / bgtz t5, 1000f /
#     call rt_poll / 1000:
# When the budget runs out, rt_poll checks the clock (R0009 past the deadline). Keep live values in
# s registers (or saved) across it.
