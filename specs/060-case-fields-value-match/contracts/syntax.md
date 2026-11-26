# Contract: Syntax

## Grammar

```text
case_decl   = name ";" | name "(" type ")" ";" | name "{" field_decl field_decl { field_decl } "}"
field_decl  = { annotation } name ":" type ";"
case_value  = Type "." name | Type "." name "(" expr ")" | Type "." name "{" field_init { "," field_init } "}"
case_type   = Type [ "[" type_args "]" ] "." name          (a field case's fields as a type)
match       = "match" expr "{" { arm } [ else_arm ] "}"
arm         = "case" name [ "(" ( name | "_" ) ")" ] block    (variant, result)
            | "case" value block                              (int, str)
value       = int_literal | "-" int_literal | string_literal | const_name | module "." const_name
else_arm    = "else" block
```

- After `Type.case` in an expression, `{` starts a field literal only when followed by a name and `:`;
  otherwise it is the block after the expression (`if e == tcp.TcpError.closed {`).
- One form per situation: no value `name;`, one value `name(T);`, two or more `name { ... }`.

## Canonical form (`vo fmt`)

```text
variant LoadError {
    missing(str);
    malformed {
        path: str;
        line: int;
        reason: str;
    }
    empty;
}

match status {
    case 200 {
        return "ok";
    }
    else {
        return "other";
    }
}
```

A literal `LoadError.malformed { path: p, line: 3, reason: "x" }` follows the record literal's layout
rules (broken one field per line past 100 columns).

## Printing values

`LoadError.malformed { path: "a", line: 3, reason: "x" }` in test failures, traces and runtime frames;
JSON `{"malformed": {"path": "a", "line": 3, "reason": "x"}}`, as for a record payload.
