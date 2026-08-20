#!/usr/bin/env python3
"""Generate C/CMake configuration from the repository's Kconfig subset."""

from __future__ import annotations

import argparse
import pathlib
import re
from dataclasses import dataclass


@dataclass
class Symbol:
    name: str
    kind: str = ""
    default: str | None = None
    minimum: int | None = None
    maximum: int | None = None


def parse_schema(path: pathlib.Path) -> dict[str, Symbol]:
    symbols: dict[str, Symbol] = {}
    current: Symbol | None = None
    for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith("mainmenu "):
            continue
        match = re.fullmatch(r"config\s+([A-Z][A-Z0-9_]*)", line)
        if match:
            current = Symbol(match.group(1))
            symbols[current.name] = current
            continue
        if current is None:
            raise ValueError(f"{path}:{number}: property without config")
        if line in ("bool", "int", "string"):
            if current.kind:
                raise ValueError(f"{path}:{number}: duplicate type")
            current.kind = line
        elif line.startswith("default "):
            current.default = line.split(None, 1)[1]
        elif line.startswith("range "):
            parts = line.split()
            if len(parts) != 3:
                raise ValueError(f"{path}:{number}: invalid range")
            current.minimum, current.maximum = int(parts[1]), int(parts[2])
        else:
            raise ValueError(f"{path}:{number}: unsupported Kconfig syntax: {line}")
    for symbol in symbols.values():
        if not symbol.kind or symbol.default is None:
            raise ValueError(f"{path}: {symbol.name} requires type and default")
    return symbols


def parse_values(path: pathlib.Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        match = re.fullmatch(r"CONFIG_([A-Z][A-Z0-9_]*)=(.*)", line)
        if not match:
            raise ValueError(f"{path}:{number}: expected CONFIG_NAME=value")
        values[match.group(1)] = match.group(2).strip()
    return values


def validate(symbol: Symbol, value: str) -> str:
    if symbol.kind == "bool":
        if value not in ("y", "n"):
            raise ValueError(f"CONFIG_{symbol.name}: bool must be y or n")
        return value
    if symbol.kind == "int":
        parsed = int(value, 0)
        if symbol.minimum is not None and parsed < symbol.minimum:
            raise ValueError(f"CONFIG_{symbol.name}: below {symbol.minimum}")
        if symbol.maximum is not None and parsed > symbol.maximum:
            raise ValueError(f"CONFIG_{symbol.name}: above {symbol.maximum}")
        return str(parsed)
    if not (len(value) >= 2 and value[0] == value[-1] == '"'):
        raise ValueError(f"CONFIG_{symbol.name}: string must be quoted")
    return value


def write_if_changed(path: pathlib.Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text(encoding="utf-8") != content:
        path.write_text(content, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--kconfig", type=pathlib.Path, required=True)
    parser.add_argument("--config", type=pathlib.Path, required=True)
    parser.add_argument("--header", type=pathlib.Path, required=True)
    parser.add_argument("--cmake", type=pathlib.Path, required=True)
    args = parser.parse_args()

    symbols = parse_schema(args.kconfig)
    overrides = parse_values(args.config)
    unknown = sorted(set(overrides) - set(symbols))
    if unknown:
        raise ValueError("unknown config symbols: " + ", ".join(unknown))
    values = {
        name: validate(symbol, overrides.get(name, symbol.default or ""))
        for name, symbol in symbols.items()
    }

    header = ["#ifndef CHASSIS_AUTOCONF_H", "#define CHASSIS_AUTOCONF_H", ""]
    cmake = ["# Generated; do not edit."]
    for name in sorted(symbols):
        value = values[name]
        if symbols[name].kind == "bool":
            header.append(f"#define CONFIG_{name} {1 if value == 'y' else 0}")
            cmake.append(f"set(CONFIG_{name} {'ON' if value == 'y' else 'OFF'})")
        else:
            header.append(f"#define CONFIG_{name} {value}")
            cmake.append(f"set(CONFIG_{name} {value})")
    header.extend(["", "#endif", ""])
    write_if_changed(args.header, "\n".join(header))
    write_if_changed(args.cmake, "\n".join(cmake) + "\n")


if __name__ == "__main__":
    main()
