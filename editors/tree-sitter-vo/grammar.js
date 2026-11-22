/**
 * tree-sitter grammar for vo. It follows the toolchain's parser (src/parse.c) rule by rule; where the
 * toolchain reports a mistake and carries on (a method call, `elif`, a trailing comma), this grammar
 * simply does not match, and tree-sitter's own recovery takes over.
 *
 * tests/grammars.py holds it to the language: every .vo file in the repository that the toolchain
 * parses must parse here with no ERROR or MISSING node, and every word-like token of this grammar
 * must be a keyword, builtin type or contextual word of the language (and the other way round).
 * After a change: `make grammars` (regenerates src/ and the grammar hash).
 */

/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

const PREC = { or: 1, and: 2, not: 3, compare: 4, sum: 5, product: 6, unary: 7, postfix: 8 };

const commaSep1 = rule => seq(rule, repeat(seq(',', rule)));
const commaSep = rule => optional(commaSep1(rule));

module.exports = grammar({
  name: 'vo',

  word: $ => $.identifier,

  extras: $ => [/[ \t\r\n]/, $.comment],

  conflicts: $ => [
    // `f: fn() uses console, x: int`: after the comma, an effect or the next parameter (two tokens ahead)
    [$.uses_clause],
    // `Db[uses file.read, T]`: after the comma, a named effect or the next type argument
    [$._argument_uses],
  ],

  rules: {
    source_file: $ => repeat($._item),

    _item: $ => choice(
      $.function_definition,
      $.record_definition,
      $.variant_definition,
      $.test_definition,
      $.prop_definition,
      $.import_statement,
      $.const_definition,
      $.effects_definition,
      $.annotation,
    ),

    // ---- items ----

    function_definition: $ => seq(
      optional('export'),
      'fn',
      field('name', $.identifier),
      optional(field('type_parameters', $.type_parameters)),
      field('parameters', $.parameters),
      optional(seq('->', field('result', $._type))),
      optional($.uses_clause),
      repeat(choice($.requires_clause, $.ensures_clause)),
      field('body', $.block),
    ),

    type_parameters: $ => seq('[', commaSep1($.type_parameter), ']'),
    type_parameter: $ => seq(
      field('name', choice($.type_identifier, $.identifier)),
      optional(seq(':', field('constraint', $.constraint))),
    ),
    constraint: _ => choice('ordered', 'effects', 'json', 'key'),

    parameters: $ => seq('(', commaSep($.parameter), ')'),
    parameter: $ => seq(field('name', $.identifier), ':', field('type', $._type)),

    uses_clause: $ => seq('uses', commaSep1($.effect)),
    effect: $ => choice(
      seq(field('name', $.effect_name), optional(field('scope', $.arguments))),
      field('parameter', $.type_identifier),
    ),
    effect_name: $ => seq($.identifier, optional(seq('.', $.identifier))),

    requires_clause: $ => seq('requires', $._expression),
    ensures_clause: $ => seq('ensures', $._expression),

    record_definition: $ => seq(
      optional('export'),
      'record',
      field('name', $.type_identifier),
      optional(field('type_parameters', $.type_parameters)),
      '{',
      repeat(choice($.field_declaration, $.annotation)),
      '}',
    ),
    field_declaration: $ => seq(field('name', $.identifier), ':', field('type', $._type), ';'),

    variant_definition: $ => seq(
      optional('export'),
      'variant',
      field('name', $.type_identifier),
      optional(field('type_parameters', $.type_parameters)),
      '{',
      repeat($.case_declaration),
      '}',
    ),
    case_declaration: $ => seq(field('name', $.identifier), optional(seq('(', field('type', $._type), ')')), ';'),

    test_definition: $ => seq('test', field('name', $.string), optional($.uses_clause), field('body', $.block)),
    prop_definition: $ => seq(
      'prop',
      field('name', $.string),
      field('parameters', $.parameters),
      optional($.uses_clause),
      field('body', $.block),
    ),

    import_statement: $ => seq('import', field('path', $.import_path), ';'),
    // (`bytes` is a keyword, and the name of the module std.bytes)
    import_path: $ => seq(
      choice($.identifier, $.type_identifier),
      repeat(seq('.', choice($.identifier, $.type_identifier, alias('bytes', $.identifier)))),
    ),

    const_definition: $ => seq(
      optional('export'),
      'const',
      field('name', choice($.identifier, $.type_identifier)),
      optional(seq(':', field('type', $._type))),
      '=',
      field('value', $._expression),
      ';',
    ),

    effects_definition: $ => seq(
      optional('export'),
      'effects',
      field('name', $.type_identifier),
      '=',
      commaSep1($.effect),
      ';',
    ),

    annotation: $ => seq('@', field('name', $.identifier), field('arguments', $.arguments)),

    // ---- types ----

    _type: $ => choice(
      $.primitive_type,
      $.named_type,
      $.list_type,
      $.map_type,
      $.collection_type,
      $.result_type,
      $.function_type,
      $.parenthesized_type,
    ),
    primitive_type: _ => choice('int', 'float', 'bool', 'str', 'bytes'),
    named_type: $ => seq(field('name', $.type_identifier), optional(field('arguments', $.type_arguments))),
    // `[T, uses console, file.read]`: after the first effect only named effects continue the list
    // (without `uses` the toolchain reads the effects too, and reports E0327 with the fix)
    type_arguments: $ => seq('[', commaSep1(choice($._type, alias($._argument_uses, $.uses_clause))), ']'),
    _argument_uses: $ => seq(
      choice(seq('uses', $.effect), alias($._named_effect, $.effect)),
      repeat(seq(',', alias($._named_effect, $.effect))),
    ),
    _named_effect: $ => seq(field('name', $.effect_name), optional(field('scope', $.arguments))),
    list_type: $ => seq('list', '[', $._type, ']'),
    map_type: $ => seq('map', '[', $._type, ',', $._type, ']'),
    // (these four names are types only before `[`; elsewhere they are ordinary identifiers)
    collection_type: $ => seq(
      choice('ordered_map', 'ordered_set', 'priority_queue', 'deque'),
      '[',
      $._type,
      optional(seq(',', $._type)),
      ']',
    ),
    result_type: $ => seq('result', '[', $._result_part, optional(seq(',', $._result_part)), ']'),
    _result_part: $ => choice($._type, alias('void', $.primitive_type)),
    function_type: $ => prec.right(seq(
      'fn',
      '(',
      commaSep($._type),
      ')',
      optional(seq('->', field('result', $._type))),
      optional($.uses_clause),
    )),
    parenthesized_type: $ => seq('(', $._type, ')'),

    // ---- statements ----

    block: $ => seq('{', repeat($._statement), '}'),

    _statement: $ => choice(
      $.let_statement,
      $.set_statement,
      $.if_statement,
      $.while_statement,
      $.for_statement,
      $.match_statement,
      $.return_statement,
      $.break_statement,
      $.continue_statement,
      $.expression_statement,
    ),

    let_statement: $ => seq(
      choice('let', 'var'),
      field('name', choice($.identifier, '_')),
      optional(seq(':', field('type', $._type))),
      '=',
      field('value', $._expression),
      ';',
    ),
    set_statement: $ => seq('set', field('place', $._expression), '=', field('value', $._expression), ';'),
    if_statement: $ => seq(
      'if',
      field('condition', $._expression),
      field('consequence', $.block),
      optional(seq('else', field('alternative', choice($.if_statement, $.block)))),
    ),
    while_statement: $ => seq('while', field('condition', $._expression), field('body', $.block)),
    for_statement: $ => seq(
      'for',
      field('name', choice($.identifier, '_')),
      optional(seq(',', field('second', choice($.identifier, '_')))),
      'in',
      field('value', $._expression),
      optional(seq('..', field('end', $._expression))),
      field('body', $.block),
    ),
    match_statement: $ => seq('match', field('value', $._expression), '{', repeat($.match_arm), '}'),
    match_arm: $ => seq(
      'case',
      field('case', choice($.identifier, $.type_identifier)),
      optional(seq('(', optional(field('binding', choice($.identifier, '_'))), ')')),
      field('body', $.block),
    ),
    return_statement: $ => seq('return', optional($._expression), ';'),
    break_statement: _ => seq('break', ';'),
    continue_statement: _ => seq('continue', ';'),
    expression_statement: $ => seq($._expression, ';'),

    // ---- expressions ----

    _expression: $ => choice(
      $.identifier,
      $.integer,
      $.float,
      $.string,
      $.boolean,
      $.call_expression,
      $.variant_value,
      $.parenthesized_expression,
      $.list_literal,
      $.new_expression,
      $.function_literal,
      $.unary_expression,
      $.try_expression,
      $.binary_expression,
      $.field_expression,
      $.index_expression,
    ),

    // (`str`, `int`, `float` and `map` are keywords that are also functions)
    call_expression: $ => prec(PREC.postfix, seq(
      field('function', choice($._expression, alias(choice('str', 'int', 'float', 'map'), $.builtin_function))),
      field('arguments', $.arguments),
    )),
    arguments: $ => seq('(', commaSep($._expression), ')'),
    variant_value: $ => seq(field('type', $.type_identifier), '.', field('case', $.identifier)),
    parenthesized_expression: $ => seq('(', $._expression, ')'),
    list_literal: $ => seq('[', commaSep($._expression), ']'),
    new_expression: $ => seq('new', choice(
      seq(field('type', $.named_type), field('body', $.record_body)),
      $.list_type,
      $.map_type,
      $.collection_type,
    )),
    record_body: $ => seq('{', commaSep($.field_initializer), '}'),
    field_initializer: $ => seq(field('name', $.identifier), ':', field('value', $._expression)),
    function_literal: $ => seq(
      'fn',
      field('parameters', $.parameters),
      optional(seq('->', field('result', $._type))),
      optional($.uses_clause),
      field('body', $.block),
    ),
    unary_expression: $ => choice(
      prec(PREC.unary, seq('-', $._expression)),
      prec(PREC.not, seq('not', $._expression)),
    ),
    try_expression: $ => prec(PREC.unary, seq('try', $._expression)),
    binary_expression: $ => choice(
      prec.left(PREC.or, seq($._expression, 'or', $._expression)),
      prec.left(PREC.and, seq($._expression, 'and', $._expression)),
      prec.left(PREC.compare, seq($._expression, choice('==', '!=', '<', '<=', '>', '>='), $._expression)),
      prec.left(PREC.sum, seq($._expression, choice('+', '-'), $._expression)),
      prec.left(PREC.product, seq($._expression, choice('*', '/', '%'), $._expression)),
    ),
    field_expression: $ => prec(PREC.postfix, seq(field('value', $._expression), '.', field('field', $.identifier))),
    index_expression: $ => prec(PREC.postfix, seq(field('value', $._expression), '[', field('index', $._expression), ']')),

    // ---- tokens ----

    boolean: _ => choice('true', 'false'),
    integer: _ => /0x[0-9a-fA-F]+|0b[01]+|[0-9]+/,
    float: _ => /[0-9]+(\.[0-9]+([eE][+-]?[0-9]+)?|[eE][+-]?[0-9]+)/,
    // a quoted string on one line, or a raw string between backticks (any text, any lines)
    string: $ => choice(
      seq('"', repeat(choice($.escape_sequence, $.string_content)), token.immediate('"')),
      seq('`', optional(alias($.raw_content, $.string_content)), token.immediate('`')),
    ),
    string_content: _ => token.immediate(prec(1, /[^"\\\n]+/)),
    raw_content: _ => token.immediate(prec(1, /[^`]+/)),
    escape_sequence: _ => token.immediate(/\\([ntr"\\]|u\{[0-9a-fA-F]{1,6}\})/),
    comment: _ => token(seq('#', /[^\n]*/)),
    identifier: _ => /[a-z_][A-Za-z0-9_]*/,
    type_identifier: _ => /[A-Z][A-Za-z0-9_]*/,
  },
});
