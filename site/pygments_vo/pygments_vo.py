"""Pygments lexer for vo (spec 015): keywords from docs/reference.md's lexical rules, the built-in types,
builtins, effects in `uses` clauses, `@spec` annotations, strings, numbers and `#` comments."""
from pygments.lexer import RegexLexer, bygroups, words
from pygments.token import Comment, Keyword, Name, Number, Operator, Punctuation, String, Text, Whitespace

KEYWORDS = ("fn", "record", "variant", "test", "prop", "import", "export", "const", "let", "var", "set", "if", "else",
            "while", "for", "in", "match", "case", "return", "break", "continue", "try", "new", "and", "or",
            "not", "uses", "requires", "ensures")
TYPES = ("int", "float", "bool", "str", "bytes", "list", "map", "result", "void", "ordered_map", "ordered_set",
         "priority_queue", "deque", "ordered", "effects", "json", "key")
BUILTINS = ("print", "write", "eprint", "read_line", "read_all", "read_file", "write_file", "args", "exit",
            "now_ms", "len", "slice", "find", "split", "join", "trim", "chars", "push", "pop", "sort", "has",
            "remove", "keys", "get_or", "expect", "expect_eq", "ok", "err", "abs", "min", "max", "upper",
            "lower", "ord", "chr", "env", "random_int", "sort_by", "map", "filter", "fold", "any", "all",
            "todo", "assume", "parse_int", "parse_float", "byte_at")
EFFECTS = ("console", "input", "file", "net", "serve", "clock", "random", "env", "process", "entropy", "log")


class VoLexer(RegexLexer):
    name = "vo"
    aliases = ["vo", "vo-test", "vo-fmt"]
    filenames = ["*.vo"]

    tokens = {
        "root": [
            (r"\s+", Whitespace),
            (r"#.*?$", Comment.Single),
            (r"@\w+", Name.Decorator),
            (r'"', String, "string"),
            (r"`[^`]*`", String),  # a raw string: any text, any lines
            (r"\b(true|false)\b", Keyword.Constant),
            (r"\b(ret)\b", Name.Builtin.Pseudo),
            (r"\buses\b", Keyword, "uses"),
            (words(KEYWORDS, suffix=r"\b"), Keyword),
            (words(TYPES, suffix=r"\b"), Keyword.Type),
            (words(BUILTINS, prefix=r"\b", suffix=r"(?=\s*\()"), Name.Builtin),
            (r"\b[A-Z]\w*", Name.Class),
            (r"\b0(x[0-9a-fA-F]+|b[01]+)\b", Number.Integer),
            (r"\b\d+(\.\d+([eE][+-]?\d+)?|[eE][+-]?\d+)\b", Number.Float),
            (r"\b\d+\b", Number.Integer),
            (r"(fn)(\s+)([a-z_]\w*)", bygroups(Keyword, Whitespace, Name.Function)),
            (r"[a-z_]\w*", Name),
            (r"->|=>|\.\.|==|!=|<=|>=|[-+*/%<>=]", Operator),
            (r"[{}()\[\],;:.]", Punctuation),
            (r".", Text),
        ],
        "string": [
            (r'\\[nrt"\\]|\\u\{[0-9a-fA-F]{1,6}\}', String.Escape),
            (r'"', String, "#pop"),
            (r'[^"\\]+', String),
            (r"\\", String),
        ],
        "uses": [  # effects up to the block or a contract clause
            (r"\s+", Whitespace),
            (words(EFFECTS, suffix=r"\b"), Name.Builtin.Pseudo),
            (r'"', String, "string"),
            (r"`[^`]*`", String),
            (r"[A-Z]\w*", Name.Class),
            (r"[.,()]", Punctuation),
            (r"(?=[{;]|requires|ensures)", Text, "#pop"),
            (r"\w+", Name),
            (r"(?=.)", Text, "#pop"),  # anything else (`:` after a parameter, `->`) ends the list
        ],
    }
