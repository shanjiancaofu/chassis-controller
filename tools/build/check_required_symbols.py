#!/usr/bin/env python3
"""Require critical linked symbols to be strong definitions."""

from __future__ import annotations

import argparse
import pathlib
import subprocess


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--nm-tool", required=True)
    parser.add_argument("--elf", type=pathlib.Path, required=True)
    parser.add_argument("--symbol", action="append", required=True)
    args = parser.parse_args()

    output = subprocess.run(
        [args.nm_tool, "-g", str(args.elf)], check=True, text=True,
        capture_output=True
    ).stdout.splitlines()
    definitions: dict[str, str] = {}
    for line in output:
        fields = line.split()
        if len(fields) >= 3:
            definitions[fields[-1]] = fields[-2]

    for symbol in args.symbol:
        symbol_type = definitions.get(symbol)
        if symbol_type not in {"T", "t"}:
            raise RuntimeError(
                f"required symbol {symbol} is not a strong text definition: "
                f"{symbol_type or 'missing'}"
            )
    print(f"required strong symbols verified: {', '.join(args.symbol)}")


if __name__ == "__main__":
    main()
