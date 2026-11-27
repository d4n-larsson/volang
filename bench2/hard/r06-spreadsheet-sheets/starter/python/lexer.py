"""Splits formula text (spaces already removed) into tokens."""

import cells
from formula import Ref

FUNCTIONS = ("SUM", "MIN", "MAX")
SYMBOLS = "+-*/():"


class Token:
    def __init__(self, kind, text, value=None):
        self.kind = kind      # "num", "ref", "func" or the symbol itself
        self.text = text
        self.value = value    # the number of a "num", the Ref of a "ref"


def is_word_char(c):
    return ("A" <= c <= "Z") or ("a" <= c <= "z") or ("0" <= c <= "9") or c == "$"


def parse_ref_word(word):
    """A Ref for `A1`, `$A1`, `A$1` or `$A$1` (any letter case), or None."""
    i = 0
    abs_col = word.startswith("$")
    if abs_col:
        i = 1
    letter = word[i:i + 1]
    i += 1
    abs_row = word[i:i + 1] == "$"
    if abs_row:
        i += 1
    if "$" in word[i:]:
        return None
    cell = cells.parse_cell(letter + word[i:])
    if cell is None:
        return None
    return Ref(cell[0], cell[1], abs_col, abs_row)


def classify(word, before_paren):
    if before_paren:
        name = word.upper()
        return Token("func", name) if name in FUNCTIONS else None
    if cells.is_digits(word):
        return Token("num", word, int(word))
    ref = parse_ref_word(word)
    return Token("ref", word, ref) if ref is not None else None


def tokenize(text):
    """The tokens of the text, or None if it has a character or word that cannot be part of a formula."""
    tokens = []
    i = 0
    while i < len(text):
        c = text[i]
        if c in SYMBOLS:
            tokens.append(Token(c, c))
            i += 1
            continue
        if not is_word_char(c):
            return None
        j = i
        while j < len(text) and is_word_char(text[j]):
            j += 1
        tok = classify(text[i:j], j < len(text) and text[j] == "(")
        if tok is None:
            return None
        tokens.append(tok)
        i = j
    return tokens
