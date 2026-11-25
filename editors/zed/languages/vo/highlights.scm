; Highlighting for vo. No pattern here depends on which of two matching patterns an editor prefers:
; a node gets at most one capture. Plain names (variables) are left to the editor's default colour.

(comment) @comment

(string) @string
(escape_sequence) @string.escape
(integer) @number
(float) @number.float
(boolean) @boolean

[
  "fn" "record" "variant" "test" "prop" "import" "export" "const" "effects"
  "let" "var" "set" "new" "uses" "requires" "ensures"
] @keyword
["if" "else" "match" "case"] @keyword.conditional
["while" "for" "in" "break" "continue"] @keyword.repeat
"return" @keyword.return
"try" @keyword.exception
["and" "or" "not"] @keyword.operator
(constraint) @keyword.modifier

(primitive_type) @type.builtin
["list" "map" "result" "ordered_map" "ordered_set" "priority_queue" "deque"] @type.builtin
(type_identifier) @type

(builtin_function) @function.builtin
(function_definition name: (identifier) @function)
(call_expression function: (identifier) @function.call)

(parameter name: (identifier) @variable.parameter)
(field_declaration name: (identifier) @property)
(field_initializer name: (identifier) @property)
(field_expression field: (identifier) @property)
(case_declaration name: (identifier) @constructor)
(variant_value case: (identifier) @constructor)
(match_arm case: (identifier) @constructor)
(const_definition name: (identifier) @constant)
(import_path (identifier) @module)

; effects: the names in a `uses` clause and in an `effects` definition
(uses_clause "uses" (effect name: (effect_name) @constant.builtin))
(effects_definition (effect name: (effect_name) @constant.builtin))
(effects_definition name: (identifier) @constant.builtin) ; a lower-case set name (E0207)

(annotation "@" @attribute name: (identifier) @attribute)

["->" ".." "=" "==" "!=" "<" "<=" ">" ">=" "+" "-" "*" "/" "%"] @operator
["(" ")" "[" "]" "{" "}"] @punctuation.bracket
["," ";" ":" "."] @punctuation.delimiter
