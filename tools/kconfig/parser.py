"""Parser for the native, line-oriented Kconfig subset."""

from __future__ import annotations

import shlex
from pathlib import Path

from .model import Default, KconfigModel, Select, Symbol


def _split_condition(text: str) -> tuple[str, str | None]:
    marker = " if "
    if marker in text:
        value, condition = text.split(marker, 1)
        return value.strip(), condition.strip()
    return text.strip(), None


def _quoted_or_word(text: str) -> str:
    parts = shlex.split(text, comments=False, posix=True)
    if len(parts) != 1:
        raise ValueError(f"expected one value, got {text!r}")
    return parts[0]


def parse(path: Path) -> KconfigModel:
    model = KconfigModel()
    _parse_file(path.resolve(), model, [])
    return model


def _parse_file(path: Path, model: KconfigModel, include_stack: list[Path]) -> None:
    if path in include_stack:
        chain = " -> ".join(str(item) for item in include_stack + [path])
        raise ValueError(f"recursive Kconfig source: {chain}")
    model.source_files.append(path)
    lines = path.read_text(encoding="utf-8").splitlines()
    current: Symbol | None = None
    context_stack: list[str] = []
    condition_stack: list[str] = []
    for number, raw in enumerate(lines, 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("mainmenu "):
            current = None
            continue
        if line.startswith("menu "):
            context_stack.append(line)
            current = None
            continue
        if line in {"endmenu", "endif", "endchoice"}:
            if not context_stack:
                raise ValueError(f"{path}:{number}: unexpected {line}")
            if line == "endif" and not context_stack[-1].startswith("if "):
                raise ValueError(f"{path}:{number}: unexpected endif")
            if line == "endmenu" and not context_stack[-1].startswith("menu "):
                raise ValueError(f"{path}:{number}: unexpected endmenu")
            if line == "endchoice" and not context_stack[-1].startswith("choice"):
                raise ValueError(f"{path}:{number}: unexpected endchoice")
            context_stack.pop()
            if line == "endif":
                condition_stack.pop()
            current = None
            continue
        if line.startswith("if "):
            context_stack.append(line)
            condition_stack.append(line[3:].strip())
            current = None
            continue
        if line.startswith("choice"):
            context_stack.append(line)
            current = None
            continue
        if line.startswith("source "):
            source = _quoted_or_word(line[len("source "):])
            _parse_file((path.parent / source).resolve(), model, include_stack + [path])
            current = None
            continue
        if line.startswith("config ") or line.startswith("menuconfig "):
            name = line.split(None, 1)[1].strip()
            if not name.isidentifier() or name.upper() != name:
                raise ValueError(f"{path}:{number}: invalid symbol name {name!r}")
            current = model.symbols.setdefault(name, Symbol(name=name, defined_at=(path, number)))
            for condition in condition_stack:
                if condition not in current.depends_on:
                    current.depends_on.append(condition)
            continue
        if current is None:
            raise ValueError(f"{path}:{number}: property without config")
        parts = line.split(None, 1)
        keyword = parts[0]
        value = parts[1].strip() if len(parts) > 1 else ""
        if keyword in {"bool", "int", "hex", "string"}:
            if current.kind is not None and current.kind != keyword:
                raise ValueError(f"{path}:{number}: duplicate or conflicting type")
            current.kind = keyword
            if value:
                current.prompt = _quoted_or_word(value)
        elif keyword == "default":
            default, condition = _split_condition(value)
            current.defaults.append(Default(default, condition))
        elif keyword == "range":
            range_value, condition = _split_condition(value)
            range_parts = range_value.split()
            if len(range_parts) != 2:
                raise ValueError(f"{path}:{number}: range requires min and max")
            current.ranges.append((range_parts[0], range_parts[1], condition))
        elif keyword == "depends":
            if not value.startswith("on "):
                raise ValueError(f"{path}:{number}: expected depends on")
            current.depends_on.append(value[3:].strip())
        elif keyword == "select":
            target, condition = _split_condition(value)
            current.selects.append(Select(target, condition))
        else:
            raise ValueError(f"{path}:{number}: unsupported Kconfig syntax: {line}")
    if context_stack:
        raise ValueError(f"{path}: unterminated block {context_stack[-1]!r}")
    for symbol in model.symbols.values():
        if symbol.kind is None:
            location = symbol.defined_at or (path, 0)
            raise ValueError(f"{location[0]}:{location[1]}: {symbol.name} has no type")
