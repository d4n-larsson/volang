import tokens
from machine import BUILTINS, NAMED, VmError


class Dictionary:
    def __init__(self):
        self.words = {}  # name -> list of primitive items: int or builtin name

    def expand(self, tok):
        """The primitive items a token stands for; raises VmError if it is unknown."""
        if tokens.is_number(tok):
            v = tokens.number(tok)
            if v is None:
                raise VmError("overflow")
            return [v]
        if tok in self.words:
            return self.words[tok]
        if tok in BUILTINS:
            return [tok]
        raise VmError(f"unknown word {tok}")

    def define(self, name, body):
        if not name or not all("a" <= c <= "z" for c in name) or name in NAMED:
            raise VmError(f"bad name {name}")
        items = []
        for t in body:
            items.extend(self.expand(t))
        self.words[name] = items
