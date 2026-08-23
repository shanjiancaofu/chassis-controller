#!/usr/bin/env python3
"""Compatibility entry point for the native Kconfig pipeline."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.kconfig.engine import evaluate
from tools.kconfig.genconfig import generate


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--kconfig", type=Path, required=True)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--header", type=Path, required=True)
    parser.add_argument("--cmake", type=Path, required=True)
    parser.add_argument("--out-config", type=Path)
    args = parser.parse_args()
    values, kinds = evaluate(args.kconfig, args.config)
    generate(
        values,
        kinds,
        args.header,
        args.cmake,
        args.out_config,
    )


if __name__ == "__main__":
    main()
