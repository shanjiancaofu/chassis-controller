"""Expression evaluation and typed symbol resolution."""

from __future__ import annotations

from dataclasses import dataclass

from .lexer import Token, tokenize
from .model import KconfigModel, Symbol


def _scalar(value: str | int | bool | None) -> str | int | bool:
    if value is None:
        return False
    if isinstance(value, (bool, int)):
        return value
    if value in {"y", "m"}:
        return True
    if value == "n":
        return False
    try:
        return int(value, 0)
    except ValueError:
        return value.strip('"')


def _truth(value: object) -> bool:
    return bool(_scalar(value if isinstance(value, (str, int, bool)) else None))


@dataclass
class _Expression:
    tokens: list[Token]
    values: dict[str, str]
    index: int = 0

    def peek(self) -> Token:
        return self.tokens[self.index]

    def take(self) -> Token:
        token = self.peek()
        self.index += 1
        return token

    def parse(self) -> bool:
        result = self.parse_or()
        if self.peek().kind != "eof":
            raise ValueError(f"unexpected token {self.peek().value!r}")
        return _truth(result)

    def parse_or(self) -> object:
        result = self.parse_and()
        while self.peek().value == "||":
            self.take()
            result = _truth(result) or _truth(self.parse_and())
        return result

    def parse_and(self) -> object:
        result = self.parse_compare()
        while self.peek().value == "&&":
            self.take()
            result = _truth(result) and _truth(self.parse_compare())
        return result

    def parse_compare(self) -> object:
        left = self.parse_unary()
        if self.peek().value in {"=", "==", "!="}:
            operator = self.take().value
            right = self.parse_unary()
            equal = _scalar(left) == _scalar(right)
            return equal if operator in {"=", "=="} else not equal
        return left

    def parse_unary(self) -> object:
        if self.peek().value == "!":
            self.take()
            return not _truth(self.parse_unary())
        if self.peek().value == "(":
            self.take()
            result = self.parse_or()
            if self.take().value != ")":
                raise ValueError("missing closing parenthesis")
            return result
        token = self.take()
        if token.kind == "word":
            return self.values.get(token.value, "n")
        if token.kind == "number":
            return int(token.value, 0)
        if token.kind == "string":
            return token.value
        raise ValueError(f"unexpected token {token.value!r}")


def expression_true(expression: str | None, values: dict[str, str]) -> bool:
    return True if expression is None else _Expression(tokenize(expression), values).parse()


def _validate(symbol: Symbol, raw: str, values: dict[str, str]) -> str:
    if symbol.kind == "bool":
        if raw not in {"y", "n"}:
            raise ValueError(f"CONFIG_{symbol.name}: bool must be y or n")
        return raw
    if symbol.kind in {"int", "hex"}:
        try:
            parsed = int(raw, 0)
        except ValueError as error:
            raise ValueError(f"CONFIG_{symbol.name}: invalid {symbol.kind} value {raw!r}") from error
        for minimum, maximum, condition in symbol.ranges:
            if expression_true(condition, values) and not (int(minimum, 0) <= parsed <= int(maximum, 0)):
                raise ValueError(f"CONFIG_{symbol.name}: {parsed} outside range {minimum}..{maximum}")
        return hex(parsed) if symbol.kind == "hex" else str(parsed)
    if symbol.kind == "string":
        if len(raw) < 2 or raw[0] != raw[-1] or raw[0] != '"':
            raise ValueError(f"CONFIG_{symbol.name}: string must be quoted")
        return raw
    raise ValueError(f"CONFIG_{symbol.name}: unsupported type {symbol.kind!r}")


def evaluate(model: KconfigModel, overrides: dict[str, str]) -> dict[str, str]:
    unknown = sorted(set(overrides) - set(model.symbols))
    if unknown:
        raise ValueError("unknown config symbols: " + ", ".join(unknown))
    values: dict[str, str] = {}
    for name, symbol in model.symbols.items():
        raw = overrides.get(name)
        if raw is None:
            raw = next((item.value for item in symbol.defaults if expression_true(item.condition, values)), "n" if symbol.kind == "bool" else "0" if symbol.kind in {"int", "hex"} else '""')
        values[name] = _validate(symbol, raw, values)
    for _ in range(len(model.symbols) + 1):
        changed = False
        for symbol in model.symbols.values():
            if not all(expression_true(condition, values) for condition in symbol.depends_on):
                disabled = "n" if symbol.kind == "bool" else "0" if symbol.kind in {"int", "hex"} else '""'
                if values[symbol.name] != disabled:
                    values[symbol.name] = disabled
                    changed = True
            if symbol.kind == "bool" and values[symbol.name] == "y":
                for selected in symbol.selects:
                    if expression_true(selected.condition, values) and values.get(selected.symbol) == "n":
                        values[selected.symbol] = "y"
                        changed = True
        if not changed:
            break
    else:
        raise ValueError("Kconfig dependency/select evaluation did not converge")
    return values
