"""Parser for a .config/prj.conf value file."""

from __future__ import annotations

import re
from pathlib import Path


def parse_config(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        match = re.fullmatch(r"CONFIG_([A-Z][A-Z0-9_]*)=(.*)", line)
        if match is None:
            raise ValueError(f"{path}:{number}: expected CONFIG_NAME=value")
        name, value = match.groups()
        if name in values:
            raise ValueError(f"{path}:{number}: duplicate CONFIG_{name}")
        values[name] = value.strip()
    return values
