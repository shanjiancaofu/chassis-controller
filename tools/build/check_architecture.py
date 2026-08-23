#!/usr/bin/env python3
"""Check that application layers do not bypass the board/driver boundary."""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path


FORBIDDEN_INCLUDE = re.compile(
    r'#\s*include\s*[<"](?:main|adc|dma|fdcan|gpio|iwdg|quadspi|rtc|spi|tim|usart)\.h[>"]'
    r'|#\s*include\s*[<"]stm32(?:g4xx|g4xx_hal|g4xx_ll)[^>"]*[>"]'
    r'|#\s*include\s*[<"]devicetree_generated\.h[>"]'
)
FORBIDDEN_SYMBOLS = (
    re.compile(r"\bHAL_[A-Za-z0-9_]+\b"),
    re.compile(r"\b[A-Z][A-Za-z0-9]*_HandleTypeDef\b"),
    re.compile(r"\bh(?:fdcan|spi|tim|uart|qspi|rtc|iwdg|adc|dma)[A-Za-z0-9_]*\b"),
)


@dataclass(frozen=True)
class Violation:
    path: Path
    line: int
    message: str


def scan_file(path: Path) -> list[Violation]:
    violations: list[Violation] = []
    for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if FORBIDDEN_INCLUDE.search(raw):
            message = ("generated Devicetree include" if
                       "devicetree_generated.h" in raw else
                       "direct CubeMX/HAL include")
            violations.append(Violation(path, number, message))
        for pattern in FORBIDDEN_SYMBOLS:
            match = pattern.search(raw)
            if match:
                violations.append(Violation(path, number, f"direct hardware symbol {match.group(0)!r}"))
                break
    return violations


def scan(root: Path, scopes: list[str]) -> list[Violation]:
    violations: list[Violation] = []
    for scope in scopes:
        directory = root / scope
        if not directory.exists():
            continue
        for path in sorted(directory.rglob("*")):
            if path.suffix not in {".c", ".h"}:
                continue
            violations.extend(scan_file(path))
    return violations


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--scope", action="append", required=True)
    args = parser.parse_args()
    violations = scan(args.root, args.scope)
    if violations:
        for violation in violations:
            print(f"{violation.path}:{violation.line}: {violation.message}", file=sys.stderr)
        return 1
    print("Application architecture boundaries verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
