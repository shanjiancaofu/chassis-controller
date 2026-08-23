"""Generate C and CMake configuration files from a Kconfig model."""

from __future__ import annotations

from pathlib import Path


def write_if_changed(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text(encoding="utf-8") != content:
        path.write_text(content, encoding="utf-8")


def generate(values: dict[str, str], kinds: dict[str, str], header: Path, cmake: Path, config: Path | None = None) -> None:
    header_lines = ["#ifndef CHASSIS_AUTOCONF_H", "#define CHASSIS_AUTOCONF_H", ""]
    cmake_lines = ["# Generated; do not edit."]
    config_lines: list[str] = []
    for name in sorted(values):
        value = values[name]
        if kinds[name] in {"bool", "tristate"}:
            header_lines.append(f"#define CONFIG_{name} {1 if value == 'y' else 0}")
            cmake_lines.append(f"set(CONFIG_{name} {'ON' if value == 'y' else 'OFF'})")
        elif kinds[name] == "string":
            header_lines.append(f"#define CONFIG_{name} {value}")
            cmake_lines.append(f"set(CONFIG_{name} {value})")
        else:
            header_lines.append(f"#define CONFIG_{name} {value}")
            cmake_lines.append(f"set(CONFIG_{name} {int(value, 0)})")
        config_lines.append(f"CONFIG_{name}={value}")
    header_lines.extend(["", "#endif", ""])
    write_if_changed(header, "\n".join(header_lines))
    write_if_changed(cmake, "\n".join(cmake_lines) + "\n")
    if config is not None:
        write_if_changed(config, "\n".join(config_lines) + "\n")
