"""Data model for the native Kconfig subset."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path


@dataclass(frozen=True)
class Default:
    value: str
    condition: str | None = None


@dataclass(frozen=True)
class Select:
    symbol: str
    condition: str | None = None


@dataclass
class Symbol:
    name: str
    kind: str | None = None
    prompt: str | None = None
    defaults: list[Default] = field(default_factory=list)
    ranges: list[tuple[str, str, str | None]] = field(default_factory=list)
    depends_on: list[str] = field(default_factory=list)
    selects: list[Select] = field(default_factory=list)
    defined_at: tuple[Path, int] | None = None


@dataclass
class KconfigModel:
    symbols: dict[str, Symbol] = field(default_factory=dict)
    source_files: list[Path] = field(default_factory=list)
