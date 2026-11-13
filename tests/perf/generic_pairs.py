"""SC-008 (spec 003): generic/specific program pairs for tests/perf.sh. Usage: generic_pairs.py DIR"""
import os, sys
d = sys.argv[1]
N = 300000
progs = {
"max_generic": f"""fn max_of[T: ordered](a: T, b: T) -> T {{
    if a > b {{
        return a;
    }}
    return b;
}}

fn main() uses console {{
    var best = 0;
    for i in 0..{N} {{
        set best = max_of(best, (i * 7919) % 100003);
    }}
    print(str(best));
}}
""",
"max_specific": f"""fn max_int(a: int, b: int) -> int {{
    if a > b {{
        return a;
    }}
    return b;
}}

fn main() uses console {{
    var best = 0;
    for i in 0..{N} {{
        set best = max_int(best, (i * 7919) % 100003);
    }}
    print(str(best));
}}
""",
"same_generic": f"""fn same[T](a: T, b: T) -> bool {{
    return a == b;
}}

fn main() uses console {{
    var n = 0;
    for i in 0..{N} {{
        if same(i % 7, 3) {{
            set n = n + 1;
        }}
    }}
    print(str(n));
}}
""",
"same_specific": f"""fn same_int(a: int, b: int) -> bool {{
    return a == b;
}}

fn main() uses console {{
    var n = 0;
    for i in 0..{N} {{
        if same_int(i % 7, 3) {{
            set n = n + 1;
        }}
    }}
    print(str(n));
}}
""",
"stack_generic": f"""record Stack[T] {{
    items: list[T];
}}

fn stack_push[T](s: Stack[T], x: T) {{
    push(s.items, x);
}}

fn stack_pop[T](s: Stack[T], fallback: T) -> T {{
    match pop(s.items) {{
        case ok(v) {{
            return v;
        }}
        case err(e) {{
            return fallback;
        }}
    }}
}}

fn main() uses console {{
    let s = new Stack[int] {{ items: new list[int] }};
    var total = 0;
    for i in 0..{N // 2} {{
        stack_push(s, i);
        set total = total + stack_pop(s, 0);
    }}
    print(str(total));
}}
""",
"stack_specific": f"""record IntStack {{
    items: list[int];
}}

fn int_push(s: IntStack, x: int) {{
    push(s.items, x);
}}

fn int_pop(s: IntStack, fallback: int) -> int {{
    match pop(s.items) {{
        case ok(v) {{
            return v;
        }}
        case err(e) {{
            return fallback;
        }}
    }}
}}

fn main() uses console {{
    let s = new IntStack {{ items: new list[int] }};
    var total = 0;
    for i in 0..{N // 2} {{
        int_push(s, i);
        set total = total + int_pop(s, 0);
    }}
    print(str(total));
}}
""",
}
for k, v in progs.items():
    open(os.path.join(d, k + ".vo"), "w").write(v)
