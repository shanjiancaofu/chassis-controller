"""Pinned Kconfiglib adapter for chassis-controller configuration."""

from __future__ import annotations

import sys
from pathlib import Path

sys.dont_write_bytecode = True
VENDOR = Path(__file__).resolve().parent / "vendor"
if str(VENDOR) not in sys.path:
    sys.path.insert(0, str(VENDOR))

import kconfiglib


KINDS = {
    kconfiglib.BOOL: "bool",
    kconfiglib.TRISTATE: "tristate",
    kconfiglib.INT: "int",
    kconfiglib.HEX: "hex",
    kconfiglib.STRING: "string",
}


def evaluate(kconfig_path: Path, config_path: Path) -> tuple[dict[str, str], dict[str, str]]:
    kconf = kconfiglib.Kconfig(str(kconfig_path), warn=True, warn_to_stderr=True)
    kconf.warn_assign_undef = True
    message = kconf.load_config(str(config_path), replace=True)
    if message is None:
        raise ValueError(f"failed to load {config_path}")

    values: dict[str, str] = {}
    kinds: dict[str, str] = {}
    for name, symbol in kconf.syms.items():
        if symbol.nodes and symbol.type != kconfiglib.UNKNOWN:
            values[name] = symbol.str_value
            kinds[name] = KINDS[symbol.type]
    return values, kinds
