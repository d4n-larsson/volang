; Scopes and definitions, for editors that highlight or select by them.

[(function_definition) (function_literal) (test_definition) (prop_definition) (block) (match_arm)] @local.scope

(function_definition name: (identifier) @local.definition.function)
(parameter name: (identifier) @local.definition.parameter)
(let_statement name: (identifier) @local.definition.var)
(for_statement name: (identifier) @local.definition.var)
(match_arm binding: (identifier) @local.definition.var)
(const_definition name: (identifier) @local.definition.constant)

(identifier) @local.reference
