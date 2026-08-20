#!/usr/bin/env python3
"""Check linked ELF memory use against the fixed Application partition."""

from __future__ import annotations

import argparse
import pathlib
import subprocess


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--size-tool", required=True)
    parser.add_argument("--elf", type=pathlib.Path, required=True)
    parser.add_argument("--flash-limit", type=int, required=True)
    parser.add_argument("--ram-limit", type=int, required=True)
    args = parser.parse_args()
    output = subprocess.run([args.size_tool, str(args.elf)], check=True,
                            text=True, capture_output=True).stdout.splitlines()
    fields = output[-1].split()
    if len(fields) < 3:
        raise RuntimeError("unexpected size output")
    text, data, bss = map(int, fields[:3])
    flash = text + data
    ram = data + bss
    if flash > args.flash_limit:
        raise RuntimeError(f"FLASH {flash} exceeds {args.flash_limit}")
    if ram > args.ram_limit:
        raise RuntimeError(f"RAM {ram} exceeds {args.ram_limit}")
    print(f"image size verified: flash={flash}/{args.flash_limit} "
          f"ram={ram}/{args.ram_limit}")


if __name__ == "__main__":
    main()
