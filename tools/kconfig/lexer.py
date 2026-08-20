"""Expression tokenizer for Kconfig conditions."""

from __future__ import annotations

import re
from dataclasses import dataclass


@dataclass(frozen=True)
class Token:
    kind: str
    value: str


_TOKEN = re.compile(
    r'''\s*(?:(?P<op>&&|\|\||!=|==|[()!,=])|(?P<string>"(?:\\.|[^"\\])*")|'''
    r'''(?P<number>0[xX][0-9a-fA-F]+|[0-9]+)|(?P<word>[A-Za-z_][A-Za-z0-9_]*))'''
)


def tokenize(expression: str) -> list[Token]:
    tokens: list[Token] = []
    position = 0
    while position < len(expression):
        match = _TOKEN.match(expression, position)
        if match is None:
            raise ValueError(f"invalid expression near: {expression[position:]!r}")
        if match.group("op"):
            kind = "op"
        elif match.group("string"):
            kind = "string"
        elif match.group("number"):
            kind = "number"
        else:
            kind = "word"
        tokens.append(Token(kind, match.group(kind)))
        position = match.end()
    tokens.append(Token("eof", ""))
    return tokens
